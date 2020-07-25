// -----------------------------------------------------------------------------
// Written by: F. Barney
// Date: 6/14/2020
// -----------------------------------------------------------------------------

#include "UFTSocket.hpp"

#include <chrono>
#include <iostream>

#define IP_ADDRESS(a, b, c, d)   ((a << 24) | (b << 16) | (c << 8) | d)
#define IP_ADDRESS_TO_STREAM(ip) ((ip >> 24) & 0xFF) << '.' << ((ip >> 16) & 0xFF) << '.' << ((ip >> 8) & 0xFF) << '.' << (ip & 0xFF)

#define LISTENER_HOST IP_ADDRESS(0, 0, 0, 0)
#define LISTENER_PORT 9000

int main(int argc, char* argv[])
{
	std::cout << std::endl << "UFT Sample Server" << std::endl;
	std::cout << " - Press Ctrl+C to exit" << std::endl << std::endl;

	UFTSocket listener;

	if (!listener.Open())
	{
		std::cerr << "Error opening listener" << std::endl;

		return -1;
	}

	if (!listener.Listen(LISTENER_HOST, LISTENER_PORT, 1))
	{
		std::cerr << "Error listening on " << IP_ADDRESS_TO_STREAM(LISTENER_HOST) << ':' << LISTENER_PORT << std::endl;

		listener.Close();

		return -1;
	}

	if (!listener.SetBlocking(true))
	{
		std::cerr << "Error setting blocking mode" << std::endl;

		listener.Close();

		return -1;
	}

	UFTSocket client;
	char recvFilePath[255];
	std::int64_t fileSize;

	UFTSocket_OnSendProgress onSend(
		[](UFTSocket_FileSize _bytesSent, UFTSocket_FileSize _fileSize, void* _lpParam)
		{
			std::cout << "Sent " << _bytesSent << '/' << _fileSize << " bytes" << std::endl;
		}
	);

	UFTSocket_OnReceiveProgress onReceive(
		[](UFTSocket_FileSize _bytesSent, UFTSocket_FileSize _fileSize, void* _lpParam)
		{
			std::cout << "Received " << _bytesSent << '/' << _fileSize << " bytes" << std::endl;
		}
	);

	while (listener.Accept(client))
	{
		std::cout << "Client connected" << std::endl;

		client.SetTimeout(
			10 * 1000
		);

		for (int i = 1; i < argc; i += 2)
		{
			if ((fileSize = client.SendFile(argv[i], argv[i + 1], onSend, nullptr)) <= 0)
			{
				std::cerr << "Error sending " << argv[i] << " to " << argv[i + 1] << std::endl;

				client.Disconnect();

				break;
			}

			std::cout << "Sent " << argv[i] << " to " << argv[i + 1] << std::endl;
		}

		while (client.IsConnected())
		{
			auto start = std::chrono::high_resolution_clock::now();

			while ((fileSize = client.ReceiveFile(recvFilePath, onReceive, nullptr)) > 0)
			{

				std::cout << "Received " << recvFilePath << " (" << fileSize << " bytes)" << std::endl;
			}

			if (fileSize == 0)
			{
				std::cerr << "Connection closed" << std::endl;

				client.Disconnect();
			}
			else if (fileSize == -2)
			{
				std::cerr << "Internal API Error" << std::endl;

				client.Disconnect();
			}
			else if (fileSize > 0)
			{
				auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
					std::chrono::high_resolution_clock::now() - start
				);

				std::cout << "Time: " << elapsed.count() << "ms" << std::endl;
			}
		}

		client.Close();

		std::cout << "Client disconnected" << std::endl;
	}

	listener.Close();

	return 0;
}
