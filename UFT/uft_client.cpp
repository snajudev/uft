#include "UFTClient.hpp"
#include "CmdLineArgs.hpp"

#include <cstdio>
#include <string>

#if !defined(WIN32)
	#include <arpa/inet.h>
#else
	#include <WS2tcpip.h>

	#pragma comment(lib, "Ws2_32.lib")
#endif

template<typename ... TArgs>
inline void Console_WriteLine(const char* format, TArgs ... args)
{
	printf(format, args ...);
	printf("\n");
}

void main_show_cli_usage(const char* arg0)
{
	Console_WriteLine("Example usage for %s", arg0);
	Console_WriteLine("%s --remote-host=127.0.0.1 --remote-port=9000 --command=get_file_list --path=\"{path}\" --timeout={seconds}", arg0);
	Console_WriteLine("%s --remote-host=127.0.0.1 --remote-port=9000 --command=send_file --source=\"{source}\" --destination=\"{destination}\" --timeout={seconds}", arg0);
	Console_WriteLine("%s --remote-host=127.0.0.1 --remote-port=9000 --command=receive_file --source=\"{source}\" --destination=\"{destination}\" --timeout={seconds}", arg0);
}

void main_on_arg_not_found(const std::string& arg)
{
	Console_WriteLine("Command line argument '%s' was not found", arg.c_str());
}

int main(int argc, char* argv[])
{
	CmdLineArgs args(
		argc,
		argv
	);

	std::string argRemoteHost;
	std::uint16_t argRemotePort = 9000;
	std::string argCommand;
	std::uint32_t argTimeout = 15 * 1000;
	std::string argPath; // optional
	std::string argSource; // optional
	std::string argDestination; // optional

	if (!args.TryGetValue("remote-host", argRemoteHost, main_on_arg_not_found) ||
		!args.TryGetValue("remote-port", argRemotePort, main_on_arg_not_found) ||
		!args.TryGetValue("command", argCommand, main_on_arg_not_found) ||
		!args.TryGetValue("timeout", argTimeout, main_on_arg_not_found))
	{
		main_show_cli_usage(argv[0]);

		return -1;
	}

	if (argCommand.compare("get_file_list"))
	{
		if (!args.TryGetValue("source", argSource, main_on_arg_not_found) ||
			!args.TryGetValue("destination", argDestination, main_on_arg_not_found))
		{
			main_show_cli_usage(argv[0]);

			return -2;
		}
	}
	else if (!args.TryGetValue("path", argPath, main_on_arg_not_found))
	{
		main_show_cli_usage(argv[0]);

		return -3;
	}

	in_addr addr;

	if (inet_pton(AF_INET, argRemoteHost.c_str(), &addr) != 1)
	{
		Console_WriteLine(
			"Invalid 'remote-host' format, expected IPv4"
		);

		return -4;
	}

	UFTClient client;

	if (!client.Connect(ntohl(addr.s_addr), argRemotePort))
	{
		Console_WriteLine(
			"Error connecting to %s:%u",
			argRemoteHost.c_str(),
			argRemotePort
		);

		return -5;
	}

	if (!client.SetTimeout(argTimeout))
	{
		Console_WriteLine(
			"Error setting client timeout"
		);

		return -6;
	}

	Console_WriteLine(
		"Connected to %u.%u.%u.%u:%u",
		(client.GetRemoteAddress() >> 24) & 0x000000FF,
		(client.GetRemoteAddress() >> 16) & 0x000000FF,
		(client.GetRemoteAddress() >> 8)  & 0x000000FF,
		(client.GetRemoteAddress() >> 0)  & 0x000000FF,
		client.GetRemotePort()
	);

	if (!argCommand.compare("send_file"))
	{
		Console_WriteLine(
			"Sending %s to %s",
			argSource.c_str(),
			argDestination.c_str()
		);

		UFTSESSION_ERROR_CODES errorCode;

		errorCode = client.SendFile(
			argSource.c_str(),
			argDestination.c_str(),
			[](std::uint64_t _bytesSent, std::uint64_t _fileSize, void* _lpParam)
			{
				Console_WriteLine(
					"Sent %llu/%llu bytes",
					_bytesSent,
					_fileSize
				);
			},
			nullptr
		);

		if (errorCode != UFTSESSION_ERROR_CODE_SUCCESS)
		{

			Console_WriteLine(
				"Error sending '%s' to '%s': %s",
				argSource.c_str(),
				argDestination.c_str(),
				UFTSESSION_ERROR_CODES_ToString(errorCode).c_str()
			);
		}
	}
	else if (!argCommand.compare("receive_file"))
	{
		Console_WriteLine(
			"Receiving %s from %s",
			argDestination.c_str(),
			argSource.c_str()
		);

		UFTSESSION_ERROR_CODES errorCode;

		errorCode = client.ReceiveFile(
			argSource.c_str(),
			argDestination.c_str(),
			[](std::uint64_t _bytesReceived, std::uint64_t _fileSize, void* _lpParam)
			{
				Console_WriteLine(
					"Received %llu/%llu bytes",
					_bytesReceived,
					_fileSize
				);
			},
			nullptr
		);

		if (errorCode != UFTSESSION_ERROR_CODE_SUCCESS)
		{

			Console_WriteLine(
				"Error receiving '%s' from '%s': %s",
				argDestination.c_str(),
				argSource.c_str(),
				UFTSESSION_ERROR_CODES_ToString(errorCode).c_str()
			);
		}
	}
	else if (!argCommand.compare("get_file_list"))
	{
		Console_WriteLine(
			"Retrieving file list for '%s'",
			argPath.c_str()
		);

		UFTSession_FileList files;
		UFTSESSION_ERROR_CODES errorCode;

		if ((errorCode = client.GetFileList(files, argPath.c_str())) == UFTSESSION_ERROR_CODE_SUCCESS)
		{
			for (auto& fileInfo : files)
			{
				Console_WriteLine(
					"[%s] Size: %llu, Timestamp: %llu",
					fileInfo.Path.c_str(),
					fileInfo.Size,
					fileInfo.Timestamp
				);
			}
		}
		else
		{

			Console_WriteLine(
				"Error receiving file list for '%s': %s",
				argPath.c_str(),
				UFTSESSION_ERROR_CODES_ToString(errorCode).c_str()
			);
		}
	}
	else
	{
		
		Console_WriteLine(
			"Invalid command '%s'",
			argCommand.c_str()
		);
	}

	client.Disconnect();

	return 0;
}
