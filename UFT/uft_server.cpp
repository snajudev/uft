#include "CmdLineArgs.hpp"
#include "UFTListener.hpp"

#include <mutex>
#include <cstdio>
#include <utility>

#if !defined(WIN32)
	#include <arpa/inet.h>
#else
	#include <WS2tcpip.h>

	#pragma comment(lib, "Ws2_32.lib")
#endif

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

void main_show_cli_usage(const char* arg0)
{
	Console_WriteLine("Example usage for %s", arg0);
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

	std::string argLocalHost("127.0.0.1");
	std::uint16_t argLocalPort = 9000;
	std::uint32_t argTimeout = 15 * 1000;

	if (!args.TryGetValue("local-host", argLocalHost, main_on_arg_not_found) ||
		!args.TryGetValue("local-port", argLocalPort, main_on_arg_not_found) ||
		!args.TryGetValue("timeout", argTimeout, main_on_arg_not_found))
	{
		main_show_cli_usage(argv[0]);

		return -1;
	}

	in_addr addr;

	if (inet_pton(AF_INET, argLocalHost.c_str(), &addr) != 1)
	{
		Console_WriteLine(
			"Invalid 'local-host' format, expected IPv4"
		);

		return -2;
	}

	UFTListener listener;
	
	if (!listener.Listen(ntohl(addr.s_addr), argLocalPort, 1))
	{
		Console_WriteLine(
			"Error listening on %s:%u",
			argLocalHost.c_str(),
			argLocalPort
		);

		return -4;
	}

	Console_WriteLine(
		"Waiting for a connection on %s:%u",
		argLocalHost.c_str(),
		argLocalPort
	);

	UFTSession session;

	if (!listener.Accept(session))
	{
		Console_WriteLine(
			"Error accepting remote connection"
		);

		listener.Close();

		return -5;
	}

	listener.Close();

	Console_WriteLine(
		"Accepted connection from %u.%u.%u.%u:%u",
		(session.GetRemoteAddress() >> 24) & 0x000000FF,
		(session.GetRemoteAddress() >> 16) & 0x000000FF,
		(session.GetRemoteAddress() >> 8)  & 0x000000FF,
		(session.GetRemoteAddress() >> 0)  & 0x000000FF,
		session.GetRemotePort()
	);

	if (!session.SetTimeout(argTimeout))
	{
		Console_WriteLine(
			"Error setting session timeout"
		);

		return -6;
	}

	UFTSESSION_ERROR_CODES errorCode;

	while ((errorCode = session.Update()) == UFTSESSION_ERROR_CODE_SUCCESS)
	{
	}

	session.Disconnect();

	switch (errorCode)
	{
		case UFTSESSION_ERROR_CODE_SUCCESS:
		case UFTSESSION_ERROR_CODE_NETWORK_NOT_CONNECTED:
		case UFTSESSION_ERROR_CODE_NETWORK_CONNECTION_LOST:
			break;

		default:
			Console_WriteLine(
				"UFTSession::Update() returned %s",
				UFTSESSION_ERROR_CODES_ToString(errorCode).c_str()
			);
			break;
	}

	return 0;
}
