#include "UFTSocket.hpp"
#include "CmdLineArgs.hpp"

#include <string>
#include <iostream>

#if !defined(WIN32)
	#include <arpa/inet.h>
#else
	#include <WS2tcpip.h>
#endif

void main_show_cli_usage(const char* arg0)
{
	std::cout << "Example usage for " << arg0 << std::endl;
	std::cout << arg0 << " --remote-host=127.0.0.1 --remote-port=9000 --command=send --source={source} --destination={destination}" << std::endl;
	std::cout << arg0 << " --remote-host=127.0.0.1 --remote-port=9000 --timeout={seconds} --command=send --source={source} --destination={destination}" << std::endl;
	std::cout << arg0 << " --remote-host=127.0.0.1 --remote-port=9000 --auth-username={username} --auth-password={password} --command=send --source={source} --destination={destination}" << std::endl;
	std::cout << arg0 << " --remote-host=127.0.0.1 --remote-port=9000 --auth-username={username} --auth-password={password} --timeout={seconds} --command=send --source={source} --destination={destination}" << std::endl;
	std::cout << arg0 << " --remote-host=127.0.0.1 --remote-port=9000 --command=receive --source={source} --destination={destination}" << std::endl;
	std::cout << arg0 << " --remote-host=127.0.0.1 --remote-port=9000 --timeout={seconds} --command=receive --source={source} --destination={destination}" << std::endl;
	std::cout << arg0 << " --remote-host=127.0.0.1 --remote-port=9000 --auth-username={username} --auth-password={password} --command=receive --source={source} --destination={destination}" << std::endl;
	std::cout << arg0 << " --remote-host=127.0.0.1 --remote-port=9000 --auth-username={username} --auth-password={password} --timeout={seconds} --command=receive --source={source} --destination={destination}" << std::endl;
	std::cout << arg0 << " --remote-host=127.0.0.1 --remote-port=9000 --command=file_list --path={path}" << std::endl;
}

void main_on_arg_not_found(const std::string& arg)
{
	std::cerr << "Command line argument '" << arg << "' was not found" << std::endl;
}

int main(int argc, char* argv[])
{
	CmdLineArgs args(
		argc,
		argv
	);

	std::string argRemoteHost;
	std::uint16_t argRemotePort;
	std::uint32_t argTimeout = 60; // optional
	std::string argCommand;
	std::string argPath; // optional
	std::string argSource; // optional
	std::string argDestination; // optional

	if (!args.TryGetValue("remote-host", argRemoteHost, main_on_arg_not_found) ||
		!args.TryGetValue("remote-port", argRemotePort, main_on_arg_not_found) ||
		!args.TryGetValue("command", argCommand, main_on_arg_not_found))
	{
		main_show_cli_usage(argv[0]);

		return -1;
	}

	args.TryGetValue(
		"timeout",
		argTimeout
	);

	if (argCommand.compare("file_list"))
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
		std::cerr << "Invalid 'remote-host' format, expected IPv4" << std::endl;

		return -4;
	}

	UFTSocket socket;

	if (!socket.Open())
	{
		std::cerr << "Error opening UFTSocket" << std::endl;

		return -5;
	}

	if (!socket.Connect(ntohl(addr.s_addr), argRemotePort))
	{
		std::cerr << "Error connecting to " << argRemoteHost << ':' << argRemotePort << std::endl;

		return -6;
	}

	std::cout << "Connected to " << argRemoteHost << ':' << argRemotePort << std::endl;

	std::string argUsername;
	std::string argPassword;

	bool useAuthentication =
		args.TryGetValue("auth-username", argUsername) &&
		args.TryGetValue("auth-password", argPassword, main_on_arg_not_found); // show output if username is set and password isn't

	std::cout << "Authenticating as " << (useAuthentication ? argUsername : "anonymous") << std::endl;

	if (useAuthentication)
	{

		// TODO: authenticate
	}

	if (!socket.SetTimeout(argTimeout))
	{

		std::cerr << "Warning: Error setting UFTSocket timeout" << std::endl;
	}

	bool success = false;

	if (!argCommand.compare("send"))
	{
		std::cout << "Sending " << argSource << " to " << argDestination << std::endl;

		success = socket.SendFile(
			argSource.c_str(),
			argDestination.c_str(),
			[](UFTSocket_FileSize _bytesSent, UFTSocket_FileSize _fileSize, void* _lpParam)
			{
				std::cout << "Sent " << _bytesSent << '/' << _fileSize << " bytes" << std::endl;
			},
			nullptr
		);
	}
	else if (!argCommand.compare("receive"))
	{
		std::cout << "Receiving " << argDestination<< " from " << argSource << std::endl;

		success = socket.ReceiveFile(
			argSource.c_str(),
			argDestination.c_str(),
			[](UFTSocket_FileSize _bytesReceived, UFTSocket_FileSize _fileSize, void* _lpParam)
			{
				std::cout << "Received " << _bytesReceived << '/' << _fileSize << " bytes" << std::endl;
			},
			nullptr
		);
	}
	else if (!argCommand.compare("file_list"))
	{
		std::cout << "Retrieving file list for '" << argPath << '\'' << std::endl;

		success = socket.GetFileList(
			argPath.c_str(),
			[](const UFTSocket_FileInfoList& _files, void* _lpParam)
			{
				for (auto& fileInfo : _files)
				{
					std::cout << '[' << fileInfo.Name << "] Size: " << fileInfo.Size << ", Timestamp: " << fileInfo.Timestamp << std::endl;
				}
			},
			nullptr
		);
	}
	else
	{

		std::cerr << "Invalid command '" << argCommand << '\'' << std::endl;
	}

	if (!success)
	{

		std::cerr << "Something went wrong." << std::endl;
	}

	socket.Close();

	return 0;
}
