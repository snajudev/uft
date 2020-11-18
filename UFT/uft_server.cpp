#include "UFTSocket.hpp"
#include "CmdLineArgs.hpp"

#include <list>
#include <mutex>
#include <queue>
#include <thread>
#include <sstream>
#include <utility>
#include <iostream>
#include <unordered_map>
#include <unordered_set>

#if !defined(WIN32)
	#include <arpa/inet.h>
#else
	#include <WS2tcpip.h>
#endif

struct Account
{
	std::string Username;
	std::string Password;

	std::string RemoteAddress;
	std::string WorkingDirectory;
};

std::string Account_ToString(const Account& account)
{
	std::stringstream ss;
	ss << "{ ";
	
	ss << "Username: " << account.Username
		<< ", Password: " << account.Password
		<< ", RemoteAddress: " << account.RemoteAddress
		<< ", WorkingDirectory: " << account.WorkingDirectory;

	ss << " }";

	return ss.str();
}

typedef std::uint64_t SessionId;

struct Session
{
	UFTSocket Socket;
	Account* lpAccount;
};

std::string Session_ToString(const Session& session)
{
	std::stringstream ss;
	ss << "{ ";

	ss << "Socket: { IsConnected: " << session.Socket.IsConnected() << ", IsBlocking: " << session.Socket.IsBlocking() << " }";
	
	ss << ", Account: " << session.lpAccount ? Account_ToString(*session.lpAccount) : "{ NULL }";

	ss << " }";

	return ss.str();
}

std::unordered_map<std::string, Account> accounts;

std::list<Session>                       sessions;

std::queue<Session>                      newSessions;
std::mutex                               newSessionsMutex;

inline void thread_sleep(std::uint32_t ms)
{
#if !defined(WIN32)
	timespec ts = { 0 };
	ts.tv_sec = ms / 1000;
	ts.tv_nsec = (ms - (ts.tv_sec * 1000)) * 1000000;

	nanosleep(&ts, nullptr);
#else
	Sleep(static_cast<DWORD>(ms));
#endif
}

void newSessions_Enqueue(Session&& session)
{
	std::lock_guard<std::mutex> lock(
		newSessionsMutex
	);

	newSessions.push(
		std::move(session)
	);
}

bool newSessions_Dequeue(Session& session)
{
	std::lock_guard<std::mutex> lock(
		newSessionsMutex
	);

	if (!newSessions.size())
	{

		return false;
	}

	// move asignment with Session is deleted so allocate one using existing memory

	new (&session) Session(
		std::move(newSessions.front())
	);

	newSessions.pop();

	return true;
}

template<typename ... TArgs>
inline void Console_WriteLine(const char* format, TArgs ... args)
{
	static std::mutex mutex;

	std::lock_guard<std::mutex> lock(
		mutex
	);

	printf(format, args ...);
	printf("\n");
}

void main_run_server(UFTSocket& listener, std::uint32_t timeout)
{
	std::thread workerThread(
		[&listener]()
		{
			Session session;

			do
			{
				while (newSessions_Dequeue(session))
				{
					sessions.emplace_back(
						std::move(session)
					);
				}

				sessions.remove_if(
					[](Session& _session)
					{
						if (!_session.Socket.Update())
						{
							Console_WriteLine(
								"Closed connection"
							);

							return true;
						}

						return false;
					}
				);

				thread_sleep(10);
			} while (listener.IsListening());
		}
	);

	Session session;
	
	do
	{
		while (listener.Accept(session.Socket))
		{
			Console_WriteLine(
				"Accepted new connection"
			);

			if (!session.Socket.SetBlocking(false))
			{
				Console_WriteLine(
					"Error entering non-blocking mode on new connection - disconnecting"
				);

				session.Socket.Close();

				continue;
			}

			if (!session.Socket.SetTimeout(timeout))
			{

				Console_WriteLine(
					"Warning: Error setting UFTSocket timeout on new connection"
				);
			}

			newSessions_Enqueue(
				std::move(session)
			);
		}
	} while (listener.IsListening());
	
	workerThread.join();

	while (newSessions_Dequeue(session))
	{
		Console_WriteLine("Cleaning up new session");

		session.Socket.Close();
	}

	for (auto& session : sessions)
	{
		Console_WriteLine("Cleaning up existing session");

		session.Socket.Close();
	}
}

void main_show_cli_usage(const char* arg0)
{
	Console_WriteLine("Example usage for %s", arg0);
	Console_WriteLine("%s --local-host=127.0.0.1 --local-port=9000", arg0);
	Console_WriteLine("%s --local-host=127.0.0.1 --local-port=9000 --timeout={seconds}", arg0);
}

void main_on_arg_not_found(const std::string& arg)
{
	Console_WriteLine(
		"Command line argument '%s' was not found",
		arg.c_str()
	);
}

int main(int argc, char* argv[])
{
	CmdLineArgs args(
		argc,
		argv
	);

	std::string argLocalHost;
	std::uint16_t argLocalPort;
	std::uint32_t argTimeout = 60;

	if (!args.TryGetValue("local-host", argLocalHost, main_on_arg_not_found) ||
		!args.TryGetValue("local-port", argLocalPort, main_on_arg_not_found))
	{
		main_show_cli_usage(argv[0]);

		return -1;
	}

	args.TryGetValue("timeout", argTimeout);

	in_addr addr;

	if (inet_pton(AF_INET, argLocalHost.c_str(), &addr) != 1)
	{
		Console_WriteLine(
			"Invalid 'local-host' format, expected IPv4"
		);

		return -2;
	}

	UFTSocket listener;
	
	if (!listener.Open())
	{
		Console_WriteLine(
			"Error opening UFTSocket"
		);

		return -3;
	}

	if (!listener.Listen(ntohl(addr.s_addr), argLocalPort, 10))
	{
		Console_WriteLine(
			"Error listening on %s",
			argLocalHost.c_str()
		);

		return -4;
	}

	Console_WriteLine(
		"Listening on %s:%u",
		argLocalHost.c_str(),
		argLocalPort
	);

	main_run_server(
		listener,
		argTimeout
	);

	listener.Close();

	return 0;
}
