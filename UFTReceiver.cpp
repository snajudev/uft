// -----------------------------------------------------------------------------
// Program: Materials ISS Experiment (MISSE)
// File: UFTReceiver.cpp
// Purpose: UFTSocket server for receiving files from UFTSender
// Written by: F. Barney
// Date: 6/14/2020
// -----------------------------------------------------------------------------

#include "UFTSocket.hpp"

#include <iostream>

#define IP_ADDRESS(a, b, c, d)   ((a << 24) | (b << 16) | (c << 8) | d)
#define IP_ADDRESS_TO_STREAM(ip) ((ip >> 24) & 0xFF) << '.' << ((ip >> 16) & 0xFF) << '.' << ((ip >> 8) & 0xFF) << '.' << (ip & 0xFF)

#define LISTENER_HOST IP_ADDRESS(0, 0, 0, 0)
#define LISTENER_PORT 9000

int main(int argc, char* argv[])
{
	std::cout << std::endl << "Misse UFT Receiver" << std::endl;
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

	UFTSocket client;
	std::string recvFilePath;
	std::uint64_t recvFileOffset;
	std::uint64_t recvFileByteCount;

	while (listener.Accept(client))
	{
		std::cout << "Client connected" << std::endl;

		while (client.IsConnected())
		{
			while (client.ReceiveFile(recvFilePath, recvFileOffset, recvFileByteCount))
			{
				std::cout << "Received " << recvFilePath << std::endl;
				std::cout << "Wrote " << recvFileByteCount << " bytes starting at offset " << recvFileOffset << std::endl;
			}
		}

		client.Close();

		std::cout << "Client disconnected" << std::endl;
	}

	listener.Close();

	return 0;
}
