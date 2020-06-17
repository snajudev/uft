// -----------------------------------------------------------------------------
// Program: Materials ISS Experiment (MISSE)
// File: UFTSocket.cpp
// Purpose: Socket wrapper for UDT file and buffer IO
// Written by: F. Barney
// Date: 6/14/2020
// -----------------------------------------------------------------------------

#include "UFTSocket.hpp"

#include <udt.h>
#include <assert.h>
#include <iostream>
#include <fstream>

typedef std::uint64_t UFTSocket_FileSize;
typedef std::uint64_t UFTSocket_FileOffset;

struct UFTSocket_FileHeader
{
	UFTSocket_FileSize   Size;
	UFTSocket_FileOffset Offset;
	char                 Path[255];
} __attribute__((__packed__));

inline void UFTSocket_WriteError(const char* message)
{
	std::cerr << message << std::endl;
}

inline void UFTSocket_WriteLastError(const char* function)
{
	std::cerr << "Error calling " << function << ": (" << UDT::getlasterror().getErrorCode() << ") " << UDT::getlasterror().getErrorMessage() << std::endl;
}

struct UFTSocket::Context
{
	bool IsOpen = false;
	bool IsBlocking = true;
	bool IsConnected = false;
	bool IsListening = false;

	int Timeout = 15 * 1000;
	
	UDTSOCKET Socket;
};

UFTSocket::UFTSocket()
	: lpContext(
		new Context()
	)
{
}

UFTSocket::~UFTSocket()
{
	delete lpContext;
}

bool UFTSocket::IsOpen() const
{
	return lpContext->IsOpen;
}

bool UFTSocket::IsBlocking() const
{
	return lpContext->IsBlocking;
}

bool UFTSocket::IsConnected() const
{
	return lpContext->IsConnected;
}

bool UFTSocket::IsListening() const
{
	return lpContext->IsListening;
}

int UFTSocket::GetTimeout() const
{
	return lpContext->Timeout;
}

bool UFTSocket::Open()
{
	assert(!IsOpen());

	UDT::startup();

	if ((lpContext->Socket = UDT::socket(AF_INET, SOCK_STREAM, 0)) == UDT::INVALID_SOCK)
	{
		UFTSocket_WriteLastError("UDT::socket");

		UDT::cleanup();

		return false;
	}

	// if the socket was opened, closed and then re-opened this will restore the state
	if (!SetBlocking(IsBlocking()) || !SetTimeout(GetTimeout()))
	{
		UDT::close(lpContext->Socket);

		UDT::cleanup();

		return false;
	}

	lpContext->IsOpen = true;

	return true;
}

void UFTSocket::Close()
{
	if (IsOpen())
	{
		if (IsConnected())
		{

			Disconnect();
		}

		lpContext->IsOpen = false;
		lpContext->IsListening = false;
	}
}

bool UFTSocket::SetBlocking(bool set)
{
	if (IsOpen())
	{
		if (UDT::setsockopt(lpContext->Socket, 0, UDT_SNDSYN, &set, sizeof(bool)) == UDT::ERROR)
		{
			UFTSocket_WriteLastError("UDT::setsockopt");

			return false;
		}

		if (UDT::setsockopt(lpContext->Socket, 0, UDT_RCVSYN, &set, sizeof(bool)) == UDT::ERROR)
		{
			UFTSocket_WriteLastError("UDT::setsockopt");

			return false;
		}
	}

	lpContext->IsBlocking = set;

	return true;
}

bool UFTSocket::SetTimeout(int milliseconds)
{
	if (IsOpen())
	{
		if (UDT::setsockopt(lpContext->Socket, 0, UDT_SNDTIMEO, &milliseconds, sizeof(int)) == UDT::ERROR)
		{
			UFTSocket_WriteLastError("UDT::setsockopt");

			return false;
		}

		if (UDT::setsockopt(lpContext->Socket, 0, UDT_RCVTIMEO, &milliseconds, sizeof(int)) == UDT::ERROR)
		{
			UFTSocket_WriteLastError("UDT::setsockopt");

			return false;
		}
	}

	lpContext->Timeout = milliseconds;

	return true;
}

bool UFTSocket::Listen(unsigned int host, unsigned short port, unsigned int backlog)
{
	assert(IsOpen());
	assert(!IsConnected());
	assert(!IsListening());

	sockaddr_in addr = { 0 };
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(host);

	if (UDT::bind(lpContext->Socket, (const sockaddr*)&addr, sizeof(addr)) == UDT::ERROR)
	{
		UFTSocket_WriteLastError("UDT::bind");

		return false;
	}

	if (UDT::listen(lpContext->Socket, static_cast<int>(backlog)) == UDT::ERROR)
	{
		UFTSocket_WriteLastError("UDT::listen");

		return false;
	}

	lpContext->IsListening = true;

	return true;
}

bool UFTSocket::Accept(UFTSocket& socket)
{
	assert(IsOpen());
	assert(IsListening());

	UDTSOCKET udtSocket;

	if ((udtSocket = UDT::accept(lpContext->Socket, nullptr, nullptr)) == UDT::INVALID_SOCK)
	{
		if (UDT::getlasterror().getErrorCode() != CUDTException::EASYNCRCV)
		{

			UFTSocket_WriteLastError("UDT::accept");
		}

		return false;
	}

	if (socket.IsOpen())
	{

		socket.Close();
	}

	socket.lpContext->IsOpen = true;
	socket.lpContext->IsConnected = true;
	socket.lpContext->IsListening = false;
	socket.lpContext->Socket = udtSocket;

	return true;
}

bool UFTSocket::Connect(unsigned int remoteHost, unsigned short remotePort)
{
	assert(IsOpen());
	assert(!IsConnected());
	assert(!IsListening());

	sockaddr_in addr = { 0 };
	addr.sin_family = AF_INET;
	addr.sin_port = htons(remotePort);
	addr.sin_addr.s_addr = htonl(remoteHost);

	if (UDT::connect(lpContext->Socket, (sockaddr*)&addr, sizeof(addr)) == UDT::ERROR)
	{
		UFTSocket_WriteLastError("UDT::connect");

		return false;
	}

	lpContext->IsConnected = true;

	return true;
}

void UFTSocket::Disconnect()
{
	if (IsConnected())
	{
		UDT::close(lpContext->Socket);

		lpContext->IsConnected = false;
	}
}

// @return -1 if would block
// @return number of bytes sent
// @return 0 on connection closed
int UFTSocket::Send(const void* lpBuffer, unsigned int size)
{
	assert(IsOpen());
	assert(IsConnected());

	int bytesSent;

	if ((bytesSent = UDT::send(lpContext->Socket, (const char*)lpBuffer, (int)size, 0)) == UDT::ERROR)
	{
		if (UDT::getlasterror().getErrorCode() == CUDTException::EASYNCSND)
		{

			return -1;
		}

		UFTSocket_WriteLastError("UDT::send");

		Disconnect();

		return 0;
	}

	return bytesSent;
}

// @return -1 if would block
// @return number of bytes read
// @return 0 on connection closed
int UFTSocket::Receive(void* lpBuffer, unsigned int size)
{
	assert(IsOpen());
	assert(IsConnected());

	int bytesReceived;

	if ((bytesReceived = UDT::recv(lpContext->Socket, (char*)lpBuffer, (int)size, 0)) == UDT::ERROR)
	{
		if (UDT::getlasterror().getErrorCode() == CUDTException::EASYNCSND)
		{

			return -1;
		}

		Disconnect();

		return 0;
	}

	return bytesReceived;
}

bool UFTSocket::SendFile(const char* lpSource, const char* lpDestination)
{
	assert(IsOpen());
	assert(IsConnected());

	std::fstream fStream(
		lpSource,
		std::ios::in | std::ios::binary
	);

	if (!fStream.is_open())
	{
		UFTSocket_WriteError("Source file not found");

		return false;
	}

	UFTSocket_FileHeader header = { 0 };
	header.Offset = 0;

	fStream.seekg(
		0,
		std::ios::end
	);

	header.Size = static_cast<UFTSocket_FileSize>(
		fStream.tellg()
	);

	fStream.seekg(
		0,
		std::ios::beg
	);

	snprintf(
		header.Path,
		sizeof(header.Path) - 1,
		lpDestination
	);

	if (!Send(&header, sizeof(header)))
	{
		UFTSocket_WriteError("Error sending UFTSocket_FileHeader");

		return false;
	}

	int64_t fStreamOffset = static_cast<int64_t>(
		header.Offset
	);

	if (UDT::sendfile(lpContext->Socket, fStream, fStreamOffset, static_cast<int64_t>(header.Size)) == UDT::ERROR)
	{
		UFTSocket_WriteLastError("UDT::sendfile");

		if (UDT::getlasterror().getErrorCode() == CUDTException::ECONNLOST)
		{

			Close();
		}

		return false;
	}

	return true;
}

bool UFTSocket::SendFileChunk(const char* lpSource, const char* lpDestination, long long index, long long size)
{
	assert(IsOpen());
	assert(IsConnected());

	std::fstream fStream(
		lpSource,
		std::ios::in | std::ios::binary
	);

	if (!fStream.is_open())
	{
		UFTSocket_WriteError("Source file not found");

		return false;
	}

	fStream.seekg(
		index
	);

	UFTSocket_FileHeader header = { 0 };
	header.Size = static_cast<UFTSocket_FileSize>(
		size
	);
	header.Offset = static_cast<UFTSocket_FileOffset>(
		index
	);

	snprintf(
		header.Path,
		sizeof(header.Path) - 1,
		lpDestination
	);

	if (!Send(&header, sizeof(header)))
	{
		UFTSocket_WriteError("Error sending UFTSocket_FileHeader");

		return false;
	}

	int64_t fStreamOffset = static_cast<int64_t>(
		header.Offset
	);

	if (UDT::sendfile(lpContext->Socket, fStream, fStreamOffset, static_cast<int64_t>(header.Size)) == UDT::ERROR)
	{
		UFTSocket_WriteLastError("UDT::sendfile");

		if (UDT::getlasterror().getErrorCode() == CUDTException::ECONNLOST)
		{

			Close();
		}

		return false;
	}

	return true;
}

bool UFTSocket::ReceiveFile(std::string& path, std::uint64_t& offset, std::uint64_t& size)
{
	assert(IsOpen());
	assert(IsConnected());

	UFTSocket_FileHeader header;

	int bytesReceived = 0;

	while ((bytesReceived = Receive(&header, sizeof(header))) == -1)
	{
	}

	if (bytesReceived == 0)
	{
		UFTSocket_WriteError("Error receiving UFTSocket_FileHeader");

		return false;
	}

	std::fstream fStream(
		header.Path,
		std::ios::out | std::ios::app | std::ios::binary
	);

	fStream.seekp(
		header.Offset
	);

	int64_t fStreamOffset = static_cast<int64_t>(
		header.Offset
	);

	if (UDT::recvfile(lpContext->Socket, fStream, fStreamOffset, static_cast<int64_t>(header.Size)) == UDT::ERROR)
	{
		UFTSocket_WriteError("UDT::recvfile");

		if (UDT::getlasterror().getErrorCode() == 2001)
		{

			Close();
		}

		return false;
	}

	path.assign(
		header.Path
	);

	size = header.Size;
	offset = header.Offset;

	return true;
}
