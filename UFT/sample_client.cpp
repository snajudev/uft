// -----------------------------------------------------------------------------
// Written by: F. Barney
// Date: 6/14/2020
// -----------------------------------------------------------------------------

#include "UFTSocket.hpp"

#include <chrono>
#include <iostream>

#define IP_ADDRESS(a, b, c, d)   ((a << 24) | (b << 16) | (c << 8) | d)
#define IP_ADDRESS_TO_STREAM(ip) ((ip >> 24) & 0xFF) << '.' << ((ip >> 16) & 0xFF) << '.' << ((ip >> 8) & 0xFF) << '.' << (ip & 0xFF)

//#define SOCKET_HOST IP_ADDRESS(127, 0, 0, 1)
#define SOCKET_HOST IP_ADDRESS(10, 0, 0, 116)
#define SOCKET_PORT 9000

int main(int argc, char* argv[])
{
	std::cout << std::endl << "UFT Sample Client" << std::endl;
	std::cout << " - Press Ctrl+C to exit" << std::endl << std::endl;

	if (argc != 3)
	{
		std::cerr << "Invalid args" << std::endl;
		std::cerr << argv[0] << " /path/to/local/source /path/on/remote/destination" << std::endl;

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

	if (!socket.Connect(SOCKET_HOST, SOCKET_PORT))
	{
		std::cerr << "Error connecting to " << IP_ADDRESS_TO_STREAM(SOCKET_HOST) << ':' << SOCKET_PORT << std::endl;

		socket.Close();

		return -1;
	}

	std::cout << "Connected to " << IP_ADDRESS_TO_STREAM(SOCKET_HOST) << ':' << SOCKET_PORT << std::endl;
	
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
