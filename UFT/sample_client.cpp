// -----------------------------------------------------------------------------
// Written by: F. Barney
// Date: 6/14/2020
// -----------------------------------------------------------------------------

#include "UFTSocket.hpp"

#include <chrono>
#include <iostream>

#include <arpa/inet.h>

#define IP_ADDRESS_TO_STREAM(ip) ((ip >> 24) & 0xFF) << '.' << ((ip >> 16) & 0xFF) << '.' << ((ip >> 8) & 0xFF) << '.' << (ip & 0xFF)

#define SOCKET_PORT 9000

int main(int argc, char* argv[])
{
	std::cout << std::endl << "UFT Sample Client" << std::endl;
	std::cout << " - Press Ctrl+C to exit" << std::endl << std::endl;

	if (argc != 4)
	{
		std::cerr << "Invalid args" << std::endl;
		std::cerr << argv[0] << " host /path/to/local/source /path/on/remote/destination" << std::endl;

		return -1;
	}

	in_addr addr;
	
	if (inet_pton(AF_INET, argv[1], &addr) != 1)
	{
		std::cerr << "Invalid IP: " << argv[1] << std::endl;

		return -1;
	}

	UFTSocket socket;

	socket.SetTimeout(
		10 * 1000
	);

	if (!socket.Open())
	{
		std::cerr << "Error opening socket" << std::endl;
		
		return -1;
	}

	auto remoteAddress = htonl(
		addr.s_addr
	);

	if (!socket.Connect(remoteAddress, SOCKET_PORT))
	{
		std::cerr << "Error connecting to " << IP_ADDRESS_TO_STREAM(remoteAddress) << ':' << SOCKET_PORT << std::endl;

		socket.Close();

		return -1;
	}

	std::cout << "Connected to " << IP_ADDRESS_TO_STREAM(remoteAddress) << ':' << SOCKET_PORT << std::endl;
	
	if (!socket.SetBlocking(true))
	{
		std::cerr << "Error setting blocking mode" << std::endl;

		socket.Disconnect();
		socket.Close();

		return -1;
	}

	UFTSocket_OnSendProgress onSend(
		[](UFTSocket_FileSize _bytesSent, UFTSocket_FileSize _fileSize, void* _lpParam)
		{
			std::cout << "Sent " << _bytesSent << '/' << _fileSize << " bytes" << std::endl;
		}
	);

	auto start = std::chrono::high_resolution_clock::now();

	if (socket.SendFile(argv[1], argv[2], onSend, nullptr) <= 0)
	{
		std::cerr << "Error sending " << argv[1] << " to " << argv[2] << std::endl;

		socket.Disconnect();
		socket.Close();

		return -1;
	}

	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::high_resolution_clock::now() - start
	);

	std::cout << "Time: " << elapsed.count() << "ms" << std::endl;

	socket.Disconnect();
	socket.Close();

	return 0;
}
