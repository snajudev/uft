// -----------------------------------------------------------------------------
// Program: Materials ISS Experiment (MISSE)
// File: UFTSocket.hpp
// Purpose: Socket wrapper for UDT file and buffer IO
// Written by: F. Barney
// Date: 6/14/2020
// -----------------------------------------------------------------------------

#ifndef UFTSOCKET_HPP
#define UFTSOCKET_HPP

#include <string>

class UFTSocket
{
	struct Context;

	Context* const lpContext;

	UFTSocket(UFTSocket&&) = delete;
	UFTSocket(const UFTSocket&) = delete;

public:
	UFTSocket();

	virtual ~UFTSocket();

	bool IsOpen() const;

	bool IsBlocking() const;

	bool IsConnected() const;

	bool IsListening() const;

	int GetTimeout() const;

	bool Open();

	void Close();

	bool SetBlocking(bool set);

	bool SetTimeout(int milliseconds);

	bool Listen(unsigned int host, unsigned short port, unsigned int backlog);

	bool Accept(UFTSocket& socket);

	bool Connect(unsigned int remoteHost, unsigned short remotePort);

	void Disconnect();

	// @return -1 if would block
	// @return number of bytes sent
	// @return 0 on connection closed
	int Send(const void* lpBuffer, unsigned int size);

	// @return -1 if would block
	// @return number of bytes read
	// @return 0 on connection closed
	int Receive(void* lpBuffer, unsigned int size);

	bool SendFile(const char* lpSource, const char* lpDestination);

	bool SendFileChunk(const char* lpSource, const char* lpDestination, long long index, long long size);

	bool ReceiveFile(std::string& path, std::uint64_t& offset, std::uint64_t& size);
};

#endif // !UFTSOCKET_HPP
