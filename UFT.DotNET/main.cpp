#include <UFTClient.hpp>
#include <UFTListener.hpp>

#include <utility>

#include <msclr/marshal_cppstd.h>

namespace UFT::DotNET
{
	public ref class UFTSession_FileInfo
	{
		System::String^ path;
		System::UInt64 size;
		System::UInt64 timestamp;

	public:
		property System::String^ Path
		{
		public:
			System::String^ get()
			{
				return path;
			}

			void set(System::String^ value)
			{
				path = value;
			}
		}

		property System::UInt64 Size
		{
		public:
			System::UInt64 get()
			{
				return size;
			}

			void set(System::UInt64 value)
			{
				size = value;
			}
		}

		property System::UInt64 Timestamp
		{
		public:
			System::UInt64 get()
			{
				return timestamp;
			}

			void set(System::UInt64 value)
			{
				timestamp = value;
			}
		}
	};
	
	public delegate void UFTSession_OnSendProgress(System::UInt64 bytesSent, System::UInt64 fileSize, System::Object^ param);

	public delegate void UFTSession_OnReceiveProgress(System::UInt64 bytesReceived, System::UInt64 fileSize, System::Object^ param);

	struct DetourContext
	{
		void* lpParam;
		void* lpDelegate;
	};

	void Detour_OnProgress_Send(std::uint64_t bytesTransferred, std::uint64_t fileSize, void* lpParam);

	void Detour_OnProgress_Receive(std::uint64_t bytesTransferred, std::uint64_t fileSize, void* lpParam);

	public ref class UFTSession
	{
	internal:
		::UFTSession* const lpSession;

	public:
		UFTSession()
			: lpSession(
				new ::UFTSession()
			)
		{
		}

		virtual ~UFTSession()
		{
			delete lpSession;
		}

		bool IsConnected()
		{
			return lpSession->IsConnected();
		}
		
		auto GetRemotePort()
		{
			return lpSession->GetRemotePort();
		}

		auto GetRemoteAddress()
		{
			return lpSession->GetRemoteAddress();
		}

		bool SetTimeout(System::TimeSpan value)
		{
			return lpSession->SetTimeout(
				System::Convert::ToInt32(
					value.TotalMilliseconds
				)
			);
		}

		// @return false on connection closed
		bool Update()
		{
			UFTSESSION_ERROR_CODES errorCode;

			if ((errorCode = lpSession->Update()) != UFTSESSION_ERROR_CODE_SUCCESS)
			{
				if (!ThrowExceptionOrReturnFalseOnConnectionLost(errorCode))
				{

					return false;
				}
			}

			return true;
		}

		// @return false on connection closed
		bool GetFileList([System::Runtime::InteropServices::OutAttribute] array<UFTSession_FileInfo^>^% files, System::String^ path)
		{
			auto _path = msclr::interop::marshal_as<std::string>(
				path
			);

			::UFTSession_FileList _files;
			UFTSESSION_ERROR_CODES errorCode;

			if ((errorCode = lpSession->GetFileList(_files, _path.c_str())) != UFTSESSION_ERROR_CODE_SUCCESS)
			{
				if (!ThrowExceptionOrReturnFalseOnConnectionLost(errorCode))
				{

					return false;
				}
			}

			files = gcnew array<UFTSession_FileInfo^>(
				_files.size()
			);

			for (System::Int32 i = 0; i < files->Length; ++i)
			{
				auto lpFileInfo = files[i] = gcnew UFTSession_FileInfo();
				lpFileInfo->Path = msclr::interop::marshal_as<System::String^>(
					_files[i].Path
				);
				lpFileInfo->Size = _files[i].Size;
				lpFileInfo->Timestamp = _files[i].Timestamp;
			}

			return true;
		}

		// @return false on connection closed
		bool SendFile(System::String^ source, System::String^ destination)
		{
			auto _source = msclr::interop::marshal_as<std::string>(
				source
			);

			auto _destination = msclr::interop::marshal_as<std::string>(
				destination
			);

			UFTSESSION_ERROR_CODES errorCode;

			if ((errorCode = lpSession->SendFile(_source.c_str(), _destination.c_str())) != UFTSESSION_ERROR_CODE_SUCCESS)
			{
				if (!ThrowExceptionOrReturnFalseOnConnectionLost(errorCode))
				{

					return false;
				}
			}

			return true;
		}
		// @return false on connection closed
		bool SendFile(System::String^ source, System::String^ destination, UFTSession_OnSendProgress^ onProgress, System::Object^ param)
		{
			auto _source = msclr::interop::marshal_as<std::string>(
				source
			);

			auto _destination = msclr::interop::marshal_as<std::string>(
				destination
			);

			System::Runtime::InteropServices::GCHandle hOnProgress(
				System::Runtime::InteropServices::GCHandle::Alloc(onProgress)
			);

			System::Runtime::InteropServices::GCHandle hParam(
				System::Runtime::InteropServices::GCHandle::Alloc(param)
			);
			
			DetourContext context;
			context.lpDelegate = System::Runtime::InteropServices::GCHandle::ToIntPtr(hOnProgress).ToPointer();
			context.lpParam = System::Runtime::InteropServices::GCHandle::ToIntPtr(hParam).ToPointer();

			UFTSESSION_ERROR_CODES errorCode;

			if ((errorCode = lpSession->SendFile(_source.c_str(), _destination.c_str(), &Detour_OnProgress_Send, &context)) != UFTSESSION_ERROR_CODE_SUCCESS)
			{
				if (!ThrowExceptionOrReturnFalseOnConnectionLost(errorCode))
				{

					return false;
				}
			}

			hParam.Free();
			hOnProgress.Free();

			return true;
		}

		// @return false on connection closed
		bool ReceiveFile(System::String^ source, System::String^ destination)
		{
			auto _source = msclr::interop::marshal_as<std::string>(
				source
			);
			
			auto _destination = msclr::interop::marshal_as<std::string>(
				destination
			);

			UFTSESSION_ERROR_CODES errorCode;

			if ((errorCode = lpSession->ReceiveFile(_source.c_str(), _destination.c_str())) != UFTSESSION_ERROR_CODE_SUCCESS)
			{
				if (!ThrowExceptionOrReturnFalseOnConnectionLost(errorCode))
				{

					return false;
				}
			}

			return true;
		}
		// @return false on connection closed
		bool ReceiveFile(System::String^ source, System::String^ destination, UFTSession_OnReceiveProgress^ onProgress, System::Object^ param)
		{
			auto _source = msclr::interop::marshal_as<std::string>(
				source
			);

			auto _destination = msclr::interop::marshal_as<std::string>(
				destination
			);

			System::Runtime::InteropServices::GCHandle hOnProgress(
				System::Runtime::InteropServices::GCHandle::Alloc(onProgress)
			);

			System::Runtime::InteropServices::GCHandle hParam(
				System::Runtime::InteropServices::GCHandle::Alloc(param)
			);

			DetourContext context;
			context.lpDelegate = System::Runtime::InteropServices::GCHandle::ToIntPtr(hOnProgress).ToPointer();
			context.lpParam = System::Runtime::InteropServices::GCHandle::ToIntPtr(hParam).ToPointer();

			UFTSESSION_ERROR_CODES errorCode;

			if ((errorCode = lpSession->ReceiveFile(_source.c_str(), _destination.c_str(), &Detour_OnProgress_Receive, &context)) != UFTSESSION_ERROR_CODE_SUCCESS)
			{
				if (!ThrowExceptionOrReturnFalseOnConnectionLost(errorCode))
				{

					return false;
				}
			}

			hParam.Free();
			hOnProgress.Free();

			return true;
		}

		void Disconnect()
		{
			lpSession->Disconnect();
		}

	private:
		static void ThrowExceptionForError(UFTSESSION_ERROR_CODES errorCode)
		{
			auto message = msclr::interop::marshal_as<System::String^>(
				UFTSESSION_ERROR_CODES_ToString(errorCode)
			);

			throw gcnew System::Exception(
				message
			);
		}

		static bool ThrowExceptionOrReturnFalseOnConnectionLost(UFTSESSION_ERROR_CODES errorCode)
		{
			switch (errorCode)
			{
				case UFTSESSION_ERROR_CODE_NETWORK_API_ERROR:
				case UFTSESSION_ERROR_CODE_NETWORK_NOT_CONNECTED:
				case UFTSESSION_ERROR_CODE_NETWORK_CONNECTION_LOST:
					return false;
			}

			ThrowExceptionForError(
				errorCode
			);

			return true;
		}
	};

	public ref class UFTClient
		: public UFTSession
	{
	public:
		UFTClient()
		{
		}

		bool Connect(System::Net::IPEndPoint^ remoteEP)
		{
			bool wasOpen;

			if (!(wasOpen = lpSession->GetSocket().IsOpen()) && !lpSession->GetSocket().Open())
			{

				return false;
			}

			auto remoteAddressBytes = remoteEP->Address->GetAddressBytes();
			System::Array::Reverse(remoteAddressBytes, 0, remoteAddressBytes->Length);

			auto remoteAddress = System::BitConverter::ToUInt32(
				remoteAddressBytes,
				0
			);
			
			if (!lpSession->GetSocket().Connect(remoteAddress, static_cast<uint16_t>(remoteEP->Port)))
			{
				if (!wasOpen)
				{

					lpSession->GetSocket().Close();
				}

				return false;
			}

			if (!lpSession->GetSocket().SetBlocking(false))
			{
				lpSession->GetSocket().Disconnect();

				if (!wasOpen)
				{

					lpSession->GetSocket().Close();
				}

				return false;
			}

			return true;
		}
	};

	public ref class UFTListener
	{
		::UFTListener* const lpListener;

	public:
		UFTListener()
			: lpListener(
				new ::UFTListener()
			)
		{
		}

		virtual ~UFTListener()
		{
			delete lpListener;
		}

		bool IsListening()
		{
			return lpListener->IsListening();
		}

		bool Accept(UFTSession^% session)
		{
			if (session == nullptr)
			{

				session = gcnew UFTSession();
			}

			if (!lpListener->Accept(*session->lpSession))
			{

				return false;
			}

			return true;
		}

		bool Listen(System::Net::IPEndPoint^ localEP, System::UInt32 backlog)
		{
			auto localAddressBytes = localEP->Address->GetAddressBytes();
			System::Array::Reverse(localAddressBytes, 0, localAddressBytes->Length);

			auto localAddress = System::BitConverter::ToUInt32(
				localAddressBytes,
				0
			);

			return lpListener->Listen(
				localAddress,
				static_cast<uint16_t>(localEP->Port),
				backlog
			);
		}

		void Close()
		{
			if (lpListener->GetSocket().IsOpen())
			{

				lpListener->Close();
			}
		}
	};
}

inline void UFT::DotNET::Detour_OnProgress_Send(std::uint64_t bytesTransferred, std::uint64_t fileSize, void* lpParam)
{
	auto lpContext = reinterpret_cast<DetourContext*>(
		lpParam
	);

	auto hDelegate = System::Runtime::InteropServices::GCHandle::FromIntPtr(
		System::IntPtr(lpContext->lpDelegate)
	);

	auto hParam = System::Runtime::InteropServices::GCHandle::FromIntPtr(
		System::IntPtr(lpContext->lpParam)
	);

	auto onProgress = static_cast<UFTSession_OnSendProgress^>(
		hDelegate.Target
	);

	onProgress(
		bytesTransferred,
		fileSize,
		hParam.Target
	);
}

inline void UFT::DotNET::Detour_OnProgress_Receive(std::uint64_t bytesTransferred, std::uint64_t fileSize, void* lpParam)
{
	auto lpContext = reinterpret_cast<DetourContext*>(
		lpParam
	);

	auto hDelegate = System::Runtime::InteropServices::GCHandle::FromIntPtr(
		System::IntPtr(lpContext->lpDelegate)
	);

	auto hParam = System::Runtime::InteropServices::GCHandle::FromIntPtr(
		System::IntPtr(lpContext->lpParam)
	);

	auto onProgress = static_cast<UFTSession_OnReceiveProgress^>(
		hDelegate.Target
	);

	onProgress(
		bytesTransferred,
		fileSize,
		hParam.Target
	);
}
