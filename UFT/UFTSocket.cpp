// -----------------------------------------------------------------------------
// Written by: F. Barney
// Date: 6/14/2020
// -----------------------------------------------------------------------------

#include "UFTSocket.hpp"

#if !defined(WIN32)
	#include <mutex>
#else
	#pragma comment(lib, "Ws2_32.lib")
	#pragma comment(lib, "zlibstatic.lib")
#endif

#include <atomic>

#include <udt.h>
#include <assert.h>

std::atomic<size_t> UDT_Init_RefCount = 0;

inline void UDT_Init()
{
	if (++UDT_Init_RefCount == 1)
	{

		UDT::startup();
	}
}

inline void UDT_Cleanup()
{
	if (--UDT_Init_RefCount == 0)
	{

		UDT::cleanup();
	}
}

class Mutex final
{
#if !defined(WIN32)
	std::mutex mutex;
#else
	CRITICAL_SECTION cs;
#endif

	Mutex(Mutex&&) = delete;
	Mutex(const Mutex&) = delete;

public:
	Mutex()
	{
#if defined(WIN32) || defined(_WIN32)
		InitializeCriticalSection(
			&cs
		);
#endif
	}

	~Mutex()
	{
#if defined(WIN32) || defined(_WIN32)
		DeleteCriticalSection(
			&cs
		);
#endif
	}

	void Lock()
	{
#if !defined(WIN32)
		mutex.lock();
#else
		EnterCriticalSection(
			&cs
		);
#endif
	}

	void Unlock()
	{
#if !defined(WIN32)
		mutex.unlock();
#else
		LeaveCriticalSection(
			&cs
		);
#endif
	}
};

struct UFTSocket::Context
{
	bool          IsOpen          = false;
	bool          IsBlocking      = true;
	bool          IsConnected     = false;
	bool          IsListening     = false;

	std::int32_t  Timeout         = 15 * 1000;
	
	UDTSOCKET     Socket;

	Mutex         IOMutex;

	std::uint16_t RemotePort      = 0;
	std::uint32_t RemoteAddress   = 0;
};

UFTSocket::UFTSocket()
	: lpContext(
		new Context()
	)
{
}

UFTSocket::UFTSocket(UFTSocket&& socket)
	: lpContext(
		socket.lpContext
	)
{
	socket.lpContext = new Context();
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

std::int32_t UFTSocket::GetTimeout() const
{
	return lpContext->Timeout;
}

std::uint16_t UFTSocket::GetRemotePort() const
{
	return lpContext->RemotePort;
}

std::uint32_t UFTSocket::GetRemoteAddress() const
{
	return lpContext->RemoteAddress;
}

bool UFTSocket::Open()
{
	assert(!IsOpen());

	UDT_Init();

	if ((lpContext->Socket = UDT::socket(AF_INET, SOCK_STREAM, 0)) == UDT::INVALID_SOCK)
	{
//		WriteLastError("UDT::socket");

		UDT_Cleanup();

		return false;
	}

	lpContext->IsOpen = true;

	// if the socket was opened, closed and then re-opened this will restore the state
	if (!SetBlocking(IsBlocking()) || !SetTimeout(GetTimeout()))
	{
		lpContext->IsOpen = false;

		UDT::close(lpContext->Socket);

		UDT_Cleanup();

		return false;
	}

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

		UDT_Cleanup();
	}
}

bool UFTSocket::SetBlocking(bool set)
{
	if (IsOpen())
	{
		if (UDT::setsockopt(lpContext->Socket, 0, UDT_SNDSYN, &set, sizeof(bool)) == UDT::ERROR)
		{
//			WriteLastError("UDT::setsockopt");

			return false;
		}

		if (UDT::setsockopt(lpContext->Socket, 0, UDT_RCVSYN, &set, sizeof(bool)) == UDT::ERROR)
		{
//			WriteLastError("UDT::setsockopt");

			return false;
		}
	}

	lpContext->IsBlocking = set;

	return true;
}

bool UFTSocket::SetTimeout(std::int32_t milliseconds)
{
	if (IsOpen())
	{
		if (UDT::setsockopt(lpContext->Socket, 0, UDT_SNDTIMEO, &milliseconds, sizeof(std::int32_t)) == UDT::ERROR)
		{
			printf("%s\n", UDT::getlasterror().getErrorMessage());
			
//			WriteLastError("UDT::setsockopt");

			return false;
		}

		if (UDT::setsockopt(lpContext->Socket, 0, UDT_RCVTIMEO, &milliseconds, sizeof(std::int32_t)) == UDT::ERROR)
		{
			printf("%s\n", UDT::getlasterror().getErrorMessage());

//			WriteLastError("UDT::setsockopt");

			return false;
		}
	}

	lpContext->Timeout = milliseconds;

	return true;
}

bool UFTSocket::Listen(std::uint32_t host, std::uint16_t port, std::uint32_t backlog)
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
//		WriteLastError("UDT::bind");

		return false;
	}

	if (UDT::listen(lpContext->Socket, static_cast<std::int32_t>(backlog)) == UDT::ERROR)
	{
//		WriteLastError("UDT::listen");

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

//			WriteLastError("UDT::accept");
		}

		return false;
	}

	sockaddr_in address;
	int addressSize = sizeof(address);

	if (UDT::getpeername(udtSocket, (sockaddr*)&address, &addressSize) == UDT::ERROR)
	{
		UDT::close(udtSocket);

		return false;
	}

	if (socket.IsOpen())
	{

		socket.Close();
	}

	++UDT_Init_RefCount;

	socket.lpContext->IsOpen = true;
	socket.lpContext->IsBlocking = IsBlocking();
	socket.lpContext->IsConnected = true;
	socket.lpContext->IsListening = false;
	socket.lpContext->Socket = udtSocket;
	socket.lpContext->Timeout = GetTimeout();
	socket.lpContext->RemotePort = ntohs(address.sin_port);
	socket.lpContext->RemoteAddress = ntohl(address.sin_addr.s_addr);

	return true;
}

bool UFTSocket::Connect(std::uint32_t remoteHost, std::uint16_t remotePort)
{
	assert(IsOpen());
	assert(!IsConnected());
	assert(!IsListening());

	sockaddr_in addr = { 0 };
	addr.sin_family = AF_INET;
	addr.sin_port = htons(remotePort);
	addr.sin_addr.s_addr = htonl(remoteHost);

	bool isBlocking = IsBlocking();

	if (!isBlocking && !SetBlocking(true))
	{

		return false;
	}

	if (UDT::connect(lpContext->Socket, (sockaddr*)&addr, sizeof(addr)) == UDT::ERROR)
	{
//		WriteLastError("UDT::connect");

		return false;
	}

	if (!isBlocking && !SetBlocking(false))
	{
		Disconnect();

		return false;
	}

	lpContext->IsConnected = true;

	lpContext->RemotePort = remotePort;
	lpContext->RemoteAddress = remoteHost;

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

void UFTSocket::LockIO()
{
	lpContext->IOMutex.Lock();
}

void UFTSocket::UnlockIO()
{
	lpContext->IOMutex.Unlock();
}

// @return number of bytes sent
// @return -1 if would block
// @return 0 on connection closed
std::int32_t UFTSocket::Send(const void* lpBuffer, std::uint32_t size)
{
	assert(IsOpen());
	assert(IsConnected());

	std::int32_t bytesSent;

	if ((bytesSent = UDT::send(lpContext->Socket, (const char*)lpBuffer, (std::int32_t)size, 0)) == UDT::ERROR)
	{
		if (UDT::getlasterror().getErrorCode() == CUDTException::EASYNCSND)
		{

			return -1;
		}

//		WriteLastError("UDT::send");

		Disconnect();
		Close();

		return 0;
	}

	return bytesSent;
}

// @return number of bytes read
// @return -1 if would block
// @return 0 on connection closed
std::int32_t UFTSocket::Receive(void* lpBuffer, std::uint32_t size)
{
	assert(IsOpen());
	assert(IsConnected());

	std::int32_t bytesReceived;

	if ((bytesReceived = UDT::recv(lpContext->Socket, (char*)lpBuffer, (std::int32_t)size, 0)) == UDT::ERROR)
	{
		if (UDT::getlasterror().getErrorCode() == CUDTException::EASYNCRCV)
		{

			return -1;
		}

		Disconnect();
		Close();

		return 0;
	}

	return bytesReceived;
}
