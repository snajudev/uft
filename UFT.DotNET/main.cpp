#include <UFTSocket.hpp>

#include <utility>

#include <msclr/marshal_cppstd.h>

namespace UFT::DotNET
{
	public value class UFTSocket_FileInfo
	{
		System::String^ name;
		System::UInt64 size;
		System::UInt64 timestamp;

	public:
		property System::String^ Name
		{
		public:
			System::String^ get()
			{
				return name;
			}

			void set(System::String^ value)
			{
				name = value;
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

	typedef System::Collections::Generic::List<UFTSocket_FileInfo> UFTSocket_FileInfoList;

	public delegate void UFTSocket_OnGetFileList(UFTSocket_FileInfoList^ files, System::Object^ param);

	public delegate void UFTSocket_OnSendProgress(System::UInt64 bytesSent, System::UInt64 fileSize, System::Object^ param);

	public delegate void UFTSocket_OnReceiveProgress(System::UInt64 bytesReceived, System::UInt64 fileSize, System::Object^ param);

	struct DetourContext
	{
		void* lpParam;
		void* lpDelegate;
	};

	void Detour_OnGetFileList(const ::UFTSocket_FileInfoList& files, void* lpParam);

	void Detour_OnProgress_Send(::UFTSocket_FileSize bytesTransferred, ::UFTSocket_FileSize fileSize, void* lpParam);

	void Detour_OnProgress_Receive(::UFTSocket_FileSize bytesTransferred, ::UFTSocket_FileSize fileSize, void* lpParam);

	public ref class UFTSocket
	{
		::UFTSocket* const lpSocket;
		
		explicit UFTSocket(::UFTSocket&& socket)
			: lpSocket(
				new ::UFTSocket(
					std::move(socket)
				)
			)
		{
		}

	public:
		UFTSocket()
			: lpSocket(
				new ::UFTSocket()
			)
		{
		}

		virtual ~UFTSocket()
		{
			delete lpSocket;
		}

		bool IsOpen()
		{
			return lpSocket->IsOpen();
		}

		bool IsBlocking()
		{
			return lpSocket->IsBlocking();
		}

		bool IsConnected()
		{
			return lpSocket->IsConnected();
		}

		bool IsListening()
		{
			return lpSocket->IsListening();
		}

		auto GetTimeout()
		{
			return System::Int32(
				lpSocket->GetTimeout()
			);
		}

		bool Open()
		{
			return lpSocket->Open();
		}

		void Close()
		{
			lpSocket->Close();
		}

		bool SetBlocking(bool set)
		{
			return lpSocket->SetBlocking(
				set
			);
		}

		bool SetTimeout(System::TimeSpan^ delta)
		{
			return lpSocket->SetTimeout(
				System::Convert::ToInt32(
					delta->TotalMilliseconds
				)
			);
		}

		bool Listen(System::Net::IPEndPoint^ localEP, System::UInt32 backlog)
		{
			auto localAddressBytes = localEP->Address->GetAddressBytes();
			System::Array::Reverse(localAddressBytes, 0, localAddressBytes->Length);

			auto localAddress = System::BitConverter::ToUInt32(
				localAddressBytes,
				0
			);

			return lpSocket->Listen(
				localAddress,
				static_cast<uint16_t>(localEP->Port),
				backlog
			);
		}

		bool Accept(UFTSocket^% socket)
		{
			::UFTSocket _socket;

			if (lpSocket->Accept(_socket))
			{
				socket = gcnew UFTSocket(
					std::move(_socket)
				);

				return true;
			}

			return false;
		}

		bool Connect(System::Net::IPEndPoint^ remoteEP)
		{
			auto remoteAddressBytes = remoteEP->Address->GetAddressBytes();
			System::Array::Reverse(remoteAddressBytes, 0, remoteAddressBytes->Length);

			auto remoteAddress = System::BitConverter::ToUInt32(
				remoteAddressBytes,
				0
			);

			return lpSocket->Connect(
				remoteAddress,
				static_cast<uint16_t>(remoteEP->Port)
			);
		}

		void Disconnect()
		{
			lpSocket->Disconnect();
		}

		// @return false on connection closed
		bool Update()
		{
			return lpSocket->Update();
		}

		// @return false on connection closed
		bool GetFileList(System::String^ source, UFTSocket_OnGetFileList^ onGetFileList, System::Object^ param)
		{
			auto _source = msclr::interop::marshal_as<std::string>(
				source
			);

			System::Runtime::InteropServices::GCHandle hOnGetFileList(
				System::Runtime::InteropServices::GCHandle::Alloc(onGetFileList)
			);

			System::Runtime::InteropServices::GCHandle hParam(
				System::Runtime::InteropServices::GCHandle::Alloc(param)
			);
			
			DetourContext context;
			context.lpDelegate = System::Runtime::InteropServices::GCHandle::ToIntPtr(hOnGetFileList).ToPointer();
			context.lpParam = System::Runtime::InteropServices::GCHandle::ToIntPtr(hParam).ToPointer();

			auto result = lpSocket->GetFileList(
				_source.c_str(),
				&Detour_OnGetFileList,
				&context
			);

			hParam.Free();
			hOnGetFileList.Free();

			return result;
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

			return lpSocket->SendFile(
				_source.c_str(),
				_destination.c_str()
			);
		}
		// @return false on connection closed
		bool SendFile(System::String^ source, System::String^ destination, UFTSocket_OnSendProgress^ onProgress, System::Object^ param)
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

			auto result = lpSocket->SendFile(
				_source.c_str(),
				_destination.c_str(),
				&Detour_OnProgress_Send,
				&context
			);

			hParam.Free();
			hOnProgress.Free();

			return result;
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

			return lpSocket->ReceiveFile(
				_source.c_str(),
				_destination.c_str()
			);
		}
		// @return false on connection closed
		bool ReceiveFile(System::String^ source, System::String^ destination, UFTSocket_OnReceiveProgress^ onProgress, System::Object^ param)
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

			auto result = lpSocket->ReceiveFile(
				_source.c_str(),
				_destination.c_str(),
				&Detour_OnProgress_Receive,
				&context
			);

			hParam.Free();
			hOnProgress.Free();

			return result;
		}
	};
}

inline void UFT::DotNET::Detour_OnGetFileList(const ::UFTSocket_FileInfoList& files, void* lpParam)
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

	auto onGetFileList = static_cast<UFTSocket_OnGetFileList^>(
		hDelegate.Target
	);

	auto _files = gcnew UFTSocket_FileInfoList();

	for (auto& file : files)
	{
		UFTSocket_FileInfo fileInfo;
		fileInfo.Name = msclr::interop::marshal_as<System::String^>(
			file.Name
		);
		fileInfo.Size = file.Size;
		fileInfo.Timestamp = file.Timestamp;

		_files->Add(fileInfo);
	}

	onGetFileList(
		_files,
		hParam.Target
	);
}

inline void UFT::DotNET::Detour_OnProgress_Send(::UFTSocket_FileSize bytesTransferred, ::UFTSocket_FileSize fileSize, void* lpParam)
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

	auto onProgress = static_cast<UFTSocket_OnSendProgress^>(
		hDelegate.Target
	);

	onProgress(
		bytesTransferred,
		fileSize,
		hParam.Target
	);
}

inline void UFT::DotNET::Detour_OnProgress_Receive(::UFTSocket_FileSize bytesTransferred, ::UFTSocket_FileSize fileSize, void* lpParam)
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

	auto onProgress = static_cast<UFTSocket_OnReceiveProgress^>(
		hDelegate.Target
	);

	onProgress(
		bytesTransferred,
		fileSize,
		hParam.Target
	);
}
