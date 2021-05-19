// -----------------------------------------------------------------------------
// Written by: F. Barney
// Date: 6/14/2020
// -----------------------------------------------------------------------------

#ifndef UFTSOCKET_HPP
#define UFTSOCKET_HPP

#include <cstdint>

class UFTSocket;

class UFTSocket_IOLockGuard final
{
	UFTSocket* const lpSocket;

	UFTSocket_IOLockGuard(UFTSocket_IOLockGuard&&) = delete;
	UFTSocket_IOLockGuard(const UFTSocket_IOLockGuard&) = delete;

public:
	explicit UFTSocket_IOLockGuard(UFTSocket& socket);

	~UFTSocket_IOLockGuard();
};

class UFTSocket
{
	struct Context;

	Context* lpContext;

	UFTSocket(const UFTSocket&) = delete;

public:
	UFTSocket();

	UFTSocket(UFTSocket&& socket);

	virtual ~UFTSocket();

	bool IsOpen() const;

	bool IsBlocking() const;

	bool IsConnected() const;

	bool IsListening() const;

	std::int32_t GetTimeout() const;

	std::uint16_t GetRemotePort() const;

	std::uint32_t GetRemoteAddress() const;

	bool Open();

	void Close();

	bool SetBlocking(bool set);

	bool SetTimeout(std::int32_t milliseconds);

	bool Listen(std::uint32_t host, std::uint16_t port, std::uint32_t backlog);

	bool Accept(UFTSocket& socket);

	bool Connect(std::uint32_t remoteHost, std::uint16_t remotePort);

	void Disconnect();

	void LockIO();

	void UnlockIO();

	// @return number of bytes sent
	// @return -1 if would block
	// @return 0 on connection closed
	std::int32_t Send(const void* lpBuffer, std::uint32_t size);

	// @return number of bytes read
	// @return -1 if would block
	// @return 0 on connection closed
	std::int32_t Receive(void* lpBuffer, std::uint32_t size);

	// @return number of bytes sent
	// @return 0 on connection closed
	std::int32_t SendAll(const void* lpBuffer, std::uint32_t size)
	{
		for (std::uint32_t i = 0; i < size; )
		{
			std::int32_t bytesSent;

			if (!IsConnected() || ((bytesSent = Send(&((const char*)lpBuffer)[i], size - i)) == 0))
			{

				return 0;
			}

			if (bytesSent > 0)
			{

				i += bytesSent;
			}
		}

		return static_cast<std::int32_t>(
			size
		);
	}

	// @return number of bytes read
	// @return 0 on connection closed
	std::int32_t ReceiveAll(void* lpBuffer, std::uint32_t size)
	{
		std::int32_t bytesRead;

		for (std::uint32_t i = 0; i < size; )
		{
			if (!IsConnected() || ((bytesRead = Receive(&((char*)lpBuffer)[i], size - i)) == 0))
			{

				return 0;
			}

			if (bytesRead > 0)
			{

				i += bytesRead;
			}
		}

		return static_cast<std::int32_t>(
			size
		);
	}

	// @return number of bytes read
	// @return -1 if would block
	// @return 0 on connection closed
	std::int32_t TryReceiveAll(void* lpBuffer, std::uint32_t size)
	{
		std::int32_t bytesRead;

		if (!IsConnected() || ((bytesRead = Receive(lpBuffer, size)) == 0))
		{

			return 0;
		}

		if (bytesRead == -1)
		{

			return -1;
		}

		for (std::uint32_t i = bytesRead; i < size; )
		{
			if (!IsConnected() || ((bytesRead = Receive(&((char*)lpBuffer)[i], size - i)) == 0))
			{

				return 0;
			}

			if (bytesRead > 0)
			{

				i += bytesRead;
			}
		}

		return static_cast<std::int32_t>(
			size
		);
	}

	UFTSocket& operator = (UFTSocket&&) = delete;
};

inline UFTSocket_IOLockGuard::UFTSocket_IOLockGuard(UFTSocket& socket)
	: lpSocket(
		&socket
	)
{
	lpSocket->LockIO();
}

inline UFTSocket_IOLockGuard::~UFTSocket_IOLockGuard()
{
	lpSocket->UnlockIO();
}

#endif // !UFTSOCKET_HPP
