// -----------------------------------------------------------------------------
// Written by: F. Barney
// Date: 5/14/2021
// -----------------------------------------------------------------------------

#ifndef UFTCLIENT_HPP
#define UFTCLIENT_HPP

#include "UFTSession.hpp"

class UFTClient
	: public UFTSession
{
public:
	UFTClient()
	{
	}

	bool Connect(std::uint32_t host, std::uint16_t port)
	{
		assert(!IsConnected());

		bool wasOpen;

		if (!(wasOpen = GetSocket().IsOpen()) && !GetSocket().Open())
		{

			return false;
		}

		if (!GetSocket().Connect(host, port))
		{
			if (!wasOpen)
			{

				GetSocket().Close();
			}

			return false;
		}

		if (!GetSocket().SetBlocking(false))
		{
			GetSocket().Disconnect();

			if (!wasOpen)
			{

				GetSocket().Close();
			}

			return false;
		}

		return true;
	}
};

#endif
