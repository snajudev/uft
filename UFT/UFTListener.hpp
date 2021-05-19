// -----------------------------------------------------------------------------
// Written by: F. Barney
// Date: 5/14/2021
// -----------------------------------------------------------------------------

#ifndef UFTLISTENER_HPP
#define UFTLISTENER_HPP

#include "UFTSocket.hpp"
#include "UFTSession.hpp"

#include <assert.h>

class UFTListener
{
	UFTSocket socket;

	UFTListener(UFTListener&&) = delete;
	UFTListener(const UFTListener&) = delete;

public:
	UFTListener()
	{
	}

	virtual ~UFTListener()
	{
	}

	bool IsListening() const
	{
		return socket.IsListening();
	}

	auto& GetSocket()
	{
		return socket;
	}
	auto& GetSocket() const
	{
		return socket;
	}

	bool Accept(UFTSession& session)
	{
		assert(IsListening());

		if (session.IsConnected())
		{

			session.Disconnect();
		}

		if (!GetSocket().Accept(session.GetSocket()))
		{

			return false;
		}

		if (!session.GetSocket().SetBlocking(GetSocket().IsBlocking()))
		{

			return false;
		}

		return true;
	}

	bool Listen(std::uint32_t host, std::uint16_t port, std::uint32_t backlog)
	{
		assert(!IsListening());

		if (!GetSocket().Open())
		{
			
			return false;
		}

		if (!GetSocket().Listen(host, port, backlog))
		{
			GetSocket().Close();

			return false;
		}

		if (!GetSocket().SetBlocking(true))
		{

			return true;
		}

		return true;
	}

	void Close()
	{
		if (socket.IsOpen())
		{

			socket.Close();
		}
	}
};

#endif
