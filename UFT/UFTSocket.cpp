// -----------------------------------------------------------------------------
// Written by: F. Barney
// Date: 6/14/2020
// -----------------------------------------------------------------------------

#if defined(WIN32)
	#pragma comment(lib, "Ws2_32.lib")
	#pragma comment(lib, "zlibstatic.lib")
#endif

#include "UFTSocket.hpp"
#include "ByteBuffer.hpp"
#include "BitConverter.hpp"

#if !defined(WIN32)
	#include <mutex>
#endif

#include <queue>
#include <cstdio>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>

#include <udt.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#if defined(WIN32)
	#define VC_EXTRALEAN

	#define WIN32_LEAN_AND_MEAN

	#include <winapifamily.h>

	#if !defined(_WIN32_WINNT)
		#define _WIN32_WINNT_NT4          0x0400 // Windows NT 4.0
		#define _WIN32_WINNT_WIN2K        0x0500 // Windows 2000
		#define _WIN32_WINNT_WINXP        0x0501 // Windows XP
		#define _WIN32_WINNT_WS03         0x0502 // Windows Server 2003
		#define _WIN32_WINNT_WIN6         0x0600 // Windows Vista
		#define _WIN32_WINNT_VISTA        0x0600 // Windows Vista
		#define _WIN32_WINNT_WS08         0x0600 // Windows Server 2008
		#define _WIN32_WINNT_LONGHORN     0x0600 // Windows Vista
		#define _WIN32_WINNT_WIN7         0x0601 // Windows 7
		#define _WIN32_WINNT_WIN8         0x0602 // Windows 8
		#define _WIN32_WINNT_WINBLUE      0x0603 // Windows 8.1
		#define _WIN32_WINNT_WINTHRESHOLD 0x0A00 // Windows 10
		#define _WIN32_WINNT_WIN10        0x0A00 // Windows 10

		#define WINVER			_WIN32_WINNT_WIN7
		#define _WIN32_WINNT	_WIN32_WINNT_WIN7

		#define WINAPI_FAMILY	WINAPI_FAMILY_DESKTOP_APP

		#include <Windows.h>
	#endif
#else
	#include <dirent.h>

	#include <sys/stat.h>
#endif

#define CreatePacketBuffer(buffer, opcode, capacity) \
	ByteBuffer buffer(sizeof(PacketHeader) + capacity); \
	buffer.Write(opcode); \
	buffer.Write(PacketHeader().DataSize)

// @return number of bytes sent
// @return 0 on connection closed
#define SendPacketBuffer(buffer) \
	[this, &buffer]() \
	{ \
		auto bufferSize = buffer.GetSize(); \
		buffer.SetOffsetW(sizeof(OPCodes)); \
		buffer.Write(static_cast<decltype(PacketHeader::DataSize)>(bufferSize - sizeof(PacketHeader))); \
		buffer.SetOffsetW(bufferSize); \
		return SendAll(buffer.GetBuffer(), bufferSize); \
	}()

class LockGuard;

class Mutex final
{
	friend LockGuard;

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
#if defined(WIN32)
		InitializeCriticalSection(
			&cs
		);
#endif
	}

	~Mutex()
	{
#if defined(WIN32)
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

class LockGuard final
{
#if !defined(WIN32)
	std::lock_guard<std::mutex> guard;
#else
	Mutex* const lpMutex;
#endif

	LockGuard(LockGuard&&) = delete;
	LockGuard(const LockGuard&) = delete;

public:
	explicit LockGuard(Mutex& mutex)
#if !defined(WIN32)
		: guard(
			mutex.mutex
		)
#else
		: lpMutex(
			&mutex
		)
#endif
	{
#if defined(WIN32)
		lpMutex->Lock();
#endif
	}

	~LockGuard()
	{
#if defined(WIN32)
		lpMutex->Unlock();
#endif
	}
};

struct UFTSocket::Context
{
	bool         IsOpen      = false;
	bool         IsBlocking  = true;
	bool         IsConnected = false;
	bool         IsListening = false;

	std::int32_t Timeout     = 15 * 1000;
	
	UDTSOCKET    Socket;

	Mutex        IOMutex;
};

UFTSocket::UFTSocket()
	: lpContext(
		new Context()
	),
	lpFileChunk(
		new FileChunk()
	),
	lpCompressedFileChunkBuffer(
		new CompressedFileChunkBuffer()
	)
{
}

UFTSocket::UFTSocket(UFTSocket&& socket)
	: lpContext(
		socket.lpContext
	),
	lpFileChunk(
		socket.lpFileChunk
	),
	lpCompressedFileChunkBuffer(
		socket.lpCompressedFileChunkBuffer
	)
{
	socket.lpContext = new Context();
	socket.lpFileChunk = new FileChunk();
	socket.lpCompressedFileChunkBuffer = new CompressedFileChunkBuffer();
}

UFTSocket::~UFTSocket()
{
	delete lpContext;
	delete lpFileChunk;
	delete lpCompressedFileChunkBuffer;
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

bool UFTSocket::Open()
{
	assert(!IsOpen());

	UDT::startup();

	if ((lpContext->Socket = UDT::socket(AF_INET, SOCK_STREAM, 0)) == UDT::INVALID_SOCK)
	{
		WriteLastError("UDT::socket");

		UDT::cleanup();

		return false;
	}

	lpContext->IsOpen = true;

	// if the socket was opened, closed and then re-opened this will restore the state
	if (!SetBlocking(IsBlocking()) || !SetTimeout(GetTimeout()))
	{
		lpContext->IsOpen = false;

		UDT::close(lpContext->Socket);

		UDT::cleanup();

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
	}
}

bool UFTSocket::SetBlocking(bool set)
{
	if (IsOpen())
	{
		if (UDT::setsockopt(lpContext->Socket, 0, UDT_SNDSYN, &set, sizeof(bool)) == UDT::ERROR)
		{
			WriteLastError("UDT::setsockopt");

			return false;
		}

		if (UDT::setsockopt(lpContext->Socket, 0, UDT_RCVSYN, &set, sizeof(bool)) == UDT::ERROR)
		{
			WriteLastError("UDT::setsockopt");

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
			WriteLastError("UDT::setsockopt");

			return false;
		}

		if (UDT::setsockopt(lpContext->Socket, 0, UDT_RCVTIMEO, &milliseconds, sizeof(std::int32_t)) == UDT::ERROR)
		{
			WriteLastError("UDT::setsockopt");

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
		WriteLastError("UDT::bind");

		return false;
	}

	if (UDT::listen(lpContext->Socket, static_cast<std::int32_t>(backlog)) == UDT::ERROR)
	{
		WriteLastError("UDT::listen");

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

			WriteLastError("UDT::accept");
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
		WriteLastError("UDT::connect");

		return false;
	}

	if (!isBlocking && !SetBlocking(false))
	{
		Disconnect();

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

// @return false on connection closed
bool UFTSocket::Update()
{
	if (IsOpen() && IsConnected())
	{
		LockGuard lock(
			lpContext->IOMutex
		);

		ByteBuffer packetBuffer;
		PacketHeader packetHeader;
		std::int32_t bytesReceived;

		while ((bytesReceived = ReadNextPacket(packetHeader, packetBuffer, false)) > 0)
		{
			switch (packetHeader.OPCode)
			{
				case OPCodes::GetFileList:
				{
					std::string path;

					if (!packetBuffer.Read<std::uint8_t>(path))
					{
						WriteError("Error reading OPCodes::GetFileList");

						Close();

						return false;
					}

					UFTSocket_FileInfoList fileList;
					GetFilesInPath(path.c_str(), fileList);

					// send OPCodes::FileList
					{
						CreatePacketBuffer(fileListBuffer, OPCodes::FileList, sizeof(std::uint32_t));
						fileListBuffer.Write(static_cast<std::uint32_t>(fileList.size()));

						if (!SendPacketBuffer(fileListBuffer))
						{
							WriteError("Error sending OPCodes::FileList");

							Close();

							return false;
						}
					}

					// send OPCodes::FileListEntry(s)
					for (auto& fileListEntry : fileList)
					{
						CreatePacketBuffer(fileListEntryBuffer, OPCodes::FileListEntry, sizeof(std::uint8_t) + 255 + sizeof(std::uint64_t) + sizeof(std::uint64_t));
						fileListEntryBuffer.Write<std::uint8_t>(fileListEntry.Name);
						fileListEntryBuffer.Write(fileListEntry.Size);
						fileListEntryBuffer.Write(fileListEntry.Timestamp);

						if (!SendPacketBuffer(fileListEntryBuffer))
						{
							WriteError("Error sending OPCodes::FileListEntry");

							Close();

							return false;
						}
					}
				}
				break;

				case OPCodes::SendFile:
				{
					std::string destinationFilePath;
					UFTSocket_FileSize sourceFileSize;
					UFTSocket_FileTimestamp sourceFileTimestamp;

					if (!packetBuffer.Read<std::uint8_t>(destinationFilePath) ||
						!packetBuffer.Read(sourceFileSize) ||
						!packetBuffer.Read(sourceFileTimestamp))
					{
						WriteError("Error reading OPCodes::SendFile");

						Close();

						return false;
					}

					int getFileInfoResult;
					FileInfo fileInfoDestination = { 0 };

					if (!(getFileInfoResult = GetFileInfo(destinationFilePath.c_str(), fileInfoDestination)))
					{
						WriteError("Error getting destination file info");

						Close();

						return false;
					}

					bool destinationFileExists = getFileInfoResult > 0;

					// send OPCodes::SendFileAck
					{
						CreatePacketBuffer(sendFileAckBuffer, OPCodes::SendFileAck, sizeof(std::uint64_t) + sizeof(std::uint64_t));
						sendFileAckBuffer.Write(fileInfoDestination.Size);
						sendFileAckBuffer.Write(fileInfoDestination.LastModified);

						if (!SendPacketBuffer(sendFileAckBuffer))
						{
							WriteError("Error sending OPCodes::SendFileAck");

							Close();

							return false;
						}
					}

					if (!destinationFileExists)
					{
						std::ofstream fStream(
							destinationFilePath,
							std::ios::binary | std::ios::trunc
						);

						if (!fStream.is_open())
						{
							WriteError("Error opening std::fstream");

							Close();

							return false;
						}

						// read OPCodes::SendFileChunk until end of source file
						for (std::uint64_t fileChunkSize, fileOffset = 0; fileOffset < sourceFileSize; )
						{
							ByteBuffer sendFileChunkBuffer;

							if (!ReadPacket(OPCodes::SendFileChunk, sendFileChunkBuffer, true))
							{
								WriteError("Error receiving OPCodes::SendFileChunk");

								Close();

								return false;
							}

							if (!sendFileChunkBuffer.Read(fileOffset) ||
								!sendFileChunkBuffer.Read(fileChunkSize))
							{
								WriteError("Error reading OPCodes::SendFileChunk");

								Close();

								return false;
							}

							if (!ReceiveCompressedChunk(*lpFileChunk))
							{
								WriteError("Error receiving compressed file chunk");

								Close();

								return false;
							}

							fStream.seekp(
								static_cast<std::streampos>(fileOffset)
							);

							fStream.write(
								reinterpret_cast<const char*>(&lpFileChunk->Buffer[0]),
								static_cast<std::streamsize>(lpFileChunk->BufferSize)
							);

							fileOffset += fileChunkSize;
						}
					}
					else
					{
						std::uint64_t fileOffset = 0;

						// verify and replace existing file buffer
						{
							std::fstream fStream(
								destinationFilePath,
								std::ios::binary | std::ios::in | std::ios::out | std::ios::ate
							);

							if (!fStream.is_open())
							{
								WriteError("Error opening std::fstream");

								Close();

								return false;
							}

							// read OPCodes::SendFileChunkHash until end of destination file
							for (std::uint64_t sourceFileChunkHash, destinationFileChunkHash, fileChunkSize; fileOffset < fileInfoDestination.Size; )
							{
								ByteBuffer sendFileChunkHashBuffer;

								if (!ReadPacket(OPCodes::SendFileChunkHash, sendFileChunkHashBuffer, true))
								{
									WriteError("Error receiving OPCodes::SendFileChunkHash");

									Close();

									return false;
								}

								if (!sendFileChunkHashBuffer.Read(fileOffset) ||
									!sendFileChunkHashBuffer.Read(fileChunkSize) ||
									!sendFileChunkHashBuffer.Read(sourceFileChunkHash))
								{
									WriteError("Error reading OPCodes::SendFileChunkHash");

									Close();

									return false;
								}

								fStream.seekg(
									fileOffset
								);

								fStream.read(
									reinterpret_cast<char*>(&lpFileChunk->Buffer[0]),
									fileChunkSize
								);

								lpFileChunk->BufferSize = static_cast<std::uint32_t>(
									fStream.gcount()
								);

								destinationFileChunkHash = CalculateHash(
									&lpFileChunk->Buffer[0],
									lpFileChunk->BufferSize
								);

								// send OPCodes::SendFileChunkHashAck
								{
									CreatePacketBuffer(sendFileChunkHashAckBuffer, OPCodes::SendFileChunkHashAck, sizeof(std::uint64_t) + sizeof(std::uint64_t) + sizeof(std::uint64_t));
									sendFileChunkHashAckBuffer.Write(fileOffset);
									sendFileChunkHashAckBuffer.Write<std::uint64_t>(lpFileChunk->BufferSize);
									sendFileChunkHashAckBuffer.Write(destinationFileChunkHash);

									if (!SendPacketBuffer(sendFileChunkHashAckBuffer))
									{
										WriteError("Error sending OPCodes::SendFileChunkHashAck");

										Close();

										return false;
									}
								}

								// replace chunk if it doesn't match
								if (sourceFileChunkHash != destinationFileChunkHash)
								{
									if (!ReceiveCompressedChunk(*lpFileChunk))
									{
										WriteError("Error receiving compressed file chunk");

										Close();

										return false;
									}

									// clear eof bit to continue seeking
									if (fStream.eof())
									{

										fStream.clear();
									}

									fStream.seekp(
										static_cast<std::streampos>(fileOffset)
									);

									fStream.write(
										reinterpret_cast<const char*>(&lpFileChunk->Buffer[0]),
										static_cast<std::streamsize>(lpFileChunk->BufferSize)
									);
								}

								fileOffset += fileChunkSize;
							}
						}

						// write remaining file buffer, if any
						if (fileOffset < sourceFileSize)
						{
							std::ofstream fStream(
								destinationFilePath,
								std::ios::binary | std::ios::ate
							);

							if (!fStream.is_open())
							{
								WriteError("Error opening std::ofstream");

								Close();

								return false;
							}

							// read OPCodes::SendFileChunk until end of source file
							for (std::uint64_t fileChunkSize; fileOffset < sourceFileSize; )
							{
								ByteBuffer sendFileChunkBuffer;

								if (!ReadPacket(OPCodes::SendFileChunk, sendFileChunkBuffer, true))
								{
									WriteError("Error receiving OPCodes::SendFileChunk");

									Close();

									return false;
								}

								if (!sendFileChunkBuffer.Read(fileOffset) ||
									!sendFileChunkBuffer.Read(fileChunkSize))
								{
									WriteError("Error reading OPCodes::SendFileChunk");

									Close();

									return false;
								}

								if (!ReceiveCompressedChunk(*lpFileChunk))
								{
									WriteError("Error receiving compressed file chunk");

									Close();

									return false;
								}

								fStream.seekp(
									static_cast<std::streampos>(fileOffset)
								);

								fStream.write(
									reinterpret_cast<const char*>(&lpFileChunk->Buffer[0]),
									static_cast<std::streamsize>(lpFileChunk->BufferSize)
								);

								fileOffset += fileChunkSize;
							}
						}
					}

					// read OPCodes::CompleteSendFile
					{
						ByteBuffer completeSendFileBuffer;

						if (!ReadPacket(OPCodes::CompleteSendFile, completeSendFileBuffer, true))
						{
							WriteError("Error receiving OPCodes::CompleteSendFile");

							Close();

							return false;
						}
					}

					// send OPCodes::CompleteSendFileAck
					{
						CreatePacketBuffer(completeSendFileAckBuffer, OPCodes::CompleteSendFileAck, 0);

						if (!SendPacketBuffer(completeSendFileAckBuffer))
						{
							WriteError("Error sending OPCodes::CompleteSendFileAck");

							Close();

							return false;
						}
					}
				}
				break;

				case OPCodes::ReceiveFile:
				{
					std::string sourceFile;
					std::uint64_t destinationFileSize;
					std::uint64_t destinationFileTimestamp;

					if (!packetBuffer.Read<std::uint8_t>(sourceFile) ||
						!packetBuffer.Read(destinationFileSize) ||
						!packetBuffer.Read(destinationFileTimestamp))
					{
						WriteError("Error reading OPCodes::ReceiveFile");

						Close();

						return false;
					}

					FileInfo sourceFileInfo;
					
					if (GetFileInfo(sourceFile.c_str(), sourceFileInfo) <= 0)
					{
						WriteError("Error getting source file info");

						Close();

						return false;
					}

					// send OPCodes::ReceiveFileAck
					{
						CreatePacketBuffer(receiveFileAckBuffer, OPCodes::ReceiveFileAck, sizeof(std::uint64_t) + sizeof(std::uint64_t));
						receiveFileAckBuffer.Write(sourceFileInfo.Size);
						receiveFileAckBuffer.Write(sourceFileInfo.LastModified);

						if (!SendPacketBuffer(receiveFileAckBuffer))
						{
							WriteError("Error sending OPCodes::ReceiveFileAck");

							Close();

							return false;
						}
					}

					std::ifstream fStream(
						sourceFile,
						std::ios::binary
					);

					if (!fStream.is_open())
					{
						WriteError("Error opening std::ifstream");

						Close();

						return false;
					}

					std::uint64_t fileOffset = 0;

					// send OPCodes::ReceiveFileChunkHash until end of destination file
					for (std::uint64_t sourceFileChunkHash, destinationFileChunkHash; fileOffset < destinationFileSize; )
					{
						fStream.seekg(
							static_cast<std::streampos>(fileOffset)
						);

						fStream.read(
							reinterpret_cast<char*>(&lpFileChunk->Buffer[0]),
							FILE_CHUNK_SIZE
						);

						lpFileChunk->BufferSize = static_cast<std::uint32_t>(
							fStream.gcount()
						);

						sourceFileChunkHash = CalculateHash(
							&lpFileChunk->Buffer[0],
							lpFileChunk->BufferSize
						);

						// send OPCodes::ReceiveFileChunkHash
						{
							CreatePacketBuffer(receiveFileChunkHashBuffer, OPCodes::ReceiveFileChunkHash, sizeof(std::uint64_t) + sizeof(std::uint64_t) + sizeof(std::uint64_t));
							receiveFileChunkHashBuffer.Write(fileOffset);
							receiveFileChunkHashBuffer.Write<std::uint64_t>(lpFileChunk->BufferSize);
							receiveFileChunkHashBuffer.Write(sourceFileChunkHash);

							if (!SendPacketBuffer(receiveFileChunkHashBuffer))
							{
								WriteError("Error sending OPCodes::ReceiveFileChunkHash");

								Close();

								return false;
							}
						}

						std::uint64_t destinationFileChunkOffset;
						std::uint64_t destinationFileChunkSize;

						// read OPCodes::ReceiveFileChunkHashAck
						{
							ByteBuffer receiveFileChunkHashAckBuffer;

							if (!ReadPacket(OPCodes::ReceiveFileChunkHashAck, receiveFileChunkHashAckBuffer, true))
							{
								WriteError("Error receiving OPCodes::ReceiveFileChunkHashAck");

								Close();

								return false;
							}

							if (!receiveFileChunkHashAckBuffer.Read(destinationFileChunkOffset) ||
								!receiveFileChunkHashAckBuffer.Read(destinationFileChunkSize) ||
								!receiveFileChunkHashAckBuffer.Read(destinationFileChunkHash))
							{
								WriteError("Error reading OPCodes::ReceiveFileChunkHashAck");

								Close();

								return false;
							}
						}

						// send new chunk if doesn't match
						if (sourceFileChunkHash != destinationFileChunkHash)
						{
							std::int32_t bytesSaved;

							if (!SendCompressedChunk(*lpFileChunk, bytesSaved))
							{
								WriteError("Error sending compressed file chunk");

								Close();

								return false;
							}
						}

						fileOffset += lpFileChunk->BufferSize;
					}

					// send OPCodes::ReceiveFileChunk until end of source file
					while (fileOffset < sourceFileInfo.Size)
					{
						fStream.seekg(
							static_cast<std::streampos>(fileOffset)
						);

						fStream.read(
							reinterpret_cast<char*>(&lpFileChunk->Buffer[0]),
							FILE_CHUNK_SIZE
						);

						lpFileChunk->BufferSize = static_cast<std::uint32_t>(
							fStream.gcount()
						);

						// send OPCodes::ReceiveFileChunk
						{
							CreatePacketBuffer(receiveFileChunkBuffer, OPCodes::ReceiveFileChunk, sizeof(std::uint64_t) + sizeof(std::uint64_t));
							receiveFileChunkBuffer.Write(fileOffset);
							receiveFileChunkBuffer.Write<std::uint64_t>(lpFileChunk->BufferSize);

							if (!SendPacketBuffer(receiveFileChunkBuffer))
							{
								WriteError("Error sending OPCodes::ReceiveFileChunk");

								Close();

								return false;
							}
						}

						std::int32_t bytesSaved;

						if (!SendCompressedChunk(*lpFileChunk, bytesSaved))
						{
							WriteError("Error sending compressed file chunk");

							Close();

							return false;
						}

						fileOffset += lpFileChunk->BufferSize;
					}

					// read OPCodes::CompleteReceiveFile
					{
						ByteBuffer completeReceiveFileBuffer;

						if (!ReadPacket(OPCodes::CompleteReceiveFile, completeReceiveFileBuffer, true))
						{
							WriteError("Error receiving OPCodes::CompleteReceiveFile");

							Close();

							return false;
						}
					}

					// send OPCodes::CompleteReceiveFileAck
					{
						CreatePacketBuffer(completeReceiveFileAckBuffer, OPCodes::CompleteReceiveFileAck, 0);

						if (!SendPacketBuffer(completeReceiveFileAckBuffer))
						{
							WriteError("Error sending OPCodes::CompleteReceiveFileAck");

							Close();

							return false;
						}
					}
				}
				break;

				default:
					Close();
					return false;
			}
		}

		switch (bytesReceived)
		{
			case 0:
			case -2:
				return false;
		}

		return true;
	}

	return false;
}

// @return false on connection closed
bool UFTSocket::GetFileList(const char* lpSource, UFTSocket_OnGetFileList onGetFileList, void* lpParam)
{
	if (IsOpen() && IsConnected())
	{
		LockGuard lock(
			lpContext->IOMutex
		);

		// send OPCodes::GetFileList
		{
			std::string source(
				lpSource
			);

			CreatePacketBuffer(getFileListBuffer, OPCodes::GetFileList, 255 + 1);
			getFileListBuffer.Write<std::uint8_t>(source);

			if (!SendPacketBuffer(getFileListBuffer))
			{
				WriteError("Error sending OPCodes::GetFileList");

				Close();

				return false;
			}
		}

		std::uint32_t fileInfoCount;

		// read OPCodes::FileList
		{
			ByteBuffer fileListBuffer;

			if (ReadPacket(OPCodes::FileList, fileListBuffer, true) <= 0)
			{
				WriteError("Error receiving OPCodes::FileList");

				Close();

				return false;
			}

			if (!fileListBuffer.Read(fileInfoCount))
			{
				WriteError("Error reading OPCodes::FileList");

				Close();

				return false;
			}
		}

		UFTSocket_FileInfoList fileInfoList;

		// read OPCodes::FileListEntry(s)
		{
			ByteBuffer fileInfoEntryBuffer;
			UFTSocket_FileInfo fileInfoEntry;

			for (std::uint32_t i = 0; i < fileInfoCount; ++i)
			{
				if (ReadPacket(OPCodes::FileListEntry, fileInfoEntryBuffer, true) <= 0)
				{
					WriteError("Error receiving OPCodes::FileListEntry");

					Close();

					return false;
				}

				if (!fileInfoEntryBuffer.Read<std::uint8_t>(fileInfoEntry.Name) ||
					!fileInfoEntryBuffer.Read(fileInfoEntry.Size) ||
					!fileInfoEntryBuffer.Read(fileInfoEntry.Timestamp))
				{
					WriteError("Error reading OPCodes::FileListEntry");

					Close();

					return false;
				}

				fileInfoList.push_back(
					std::move(fileInfoEntry)
				);
			}
		}

		onGetFileList(
			fileInfoList,
			lpParam
		);

		return true;
	}

	return false;
}

// @return false on connection closed
bool UFTSocket::SendFile(const char* lpSource, const char* lpDestination, UFTSocket_OnSendProgress onProgress, void* lpParam)
{
	if (IsOpen() && IsConnected())
	{
		LockGuard lock(
			lpContext->IOMutex
		);

		FileInfo fileInfo = { 0 };

		if (GetFileInfo(lpSource, fileInfo) <= 0)
		{
			WriteError("Error getting source file info");

			Close();

			return false;
		}

		UFTSocket_FileInfo fileInfoSource;
		fileInfoSource.Name = lpSource;
		fileInfoSource.Size = fileInfo.Size;
		fileInfoSource.Timestamp = fileInfo.LastModified;

		// send OPCodes::SendFile
		{
			std::string destination(
				lpDestination
			);

			CreatePacketBuffer(sendFileBuffer, OPCodes::SendFile, sizeof(std::uint8_t) + 255 + sizeof(std::uint64_t) + sizeof(std::uint64_t));
			sendFileBuffer.Write<std::uint8_t>(destination);
			sendFileBuffer.Write(fileInfoSource.Size);
			sendFileBuffer.Write(fileInfoSource.Timestamp);

			if (!SendPacketBuffer(sendFileBuffer))
			{
				Close();

				return false;
			}
		}

		UFTSocket_FileInfo fileInfoDestination;
		
		// read OPCodes::SendFileAck
		{
			ByteBuffer sendFileAckBuffer;

			if (!ReadPacket(OPCodes::SendFileAck, sendFileAckBuffer, true))
			{
				WriteError("Error receiving OPCodes::SendFileAck");

				Close();

				return false;
			}

			if (!sendFileAckBuffer.Read(fileInfoDestination.Size) ||
				!sendFileAckBuffer.Read(fileInfoDestination.Timestamp))
			{
				WriteError("Error reading OPCodes::SendFileAck");

				Close();

				return false;
			}
		}

		std::uint64_t fileOffset = 0;

		std::ifstream fStream(
			lpSource,
			std::ios::binary
		);

		if (!fStream.is_open())
		{
			WriteError("Error opening std::ifstream");

			Close();

			return false;
		}

		// send OPCodes::SendFileChunkHash until end of destination file
		for (std::uint64_t sourceFileChunkHash, destinationFileChunkHash; fileOffset < fileInfoDestination.Size; )
		{
			fStream.seekg(
				static_cast<std::streampos>(fileOffset)
			);

			fStream.read(
				reinterpret_cast<char*>(&lpFileChunk->Buffer[0]),
				FILE_CHUNK_SIZE
			);

			lpFileChunk->BufferSize = static_cast<std::uint32_t>(
				fStream.gcount()
			);

			sourceFileChunkHash = CalculateHash(
				&lpFileChunk->Buffer[0],
				lpFileChunk->BufferSize
			);

			// send OPCodes::SendFileChunkHash
			{
				CreatePacketBuffer(sendFileChunkHashBuffer, OPCodes::SendFileChunkHash, sizeof(std::uint64_t) + sizeof(std::uint64_t) + sizeof(std::uint64_t));
				sendFileChunkHashBuffer.Write(fileOffset);
				sendFileChunkHashBuffer.Write<std::uint64_t>(lpFileChunk->BufferSize);
				sendFileChunkHashBuffer.Write(sourceFileChunkHash);

				if (!SendPacketBuffer(sendFileChunkHashBuffer))
				{
					WriteError("Error sending OPCodes::SendFileChunkHash");

					Close();

					return false;
				}
			}

			std::uint64_t destinationFileChunkOffset;
			std::uint64_t destinationFileChunkSize;

			// read OPCodes::SendFileChunkHashAck
			{
				ByteBuffer sendFileChunkHashAckBuffer;

				if (!ReadPacket(OPCodes::SendFileChunkHashAck, sendFileChunkHashAckBuffer, true))
				{
					WriteError("Error receiving OPCodes::SendFileChunkHashAck");

					Close();

					return false;
				}

				if (!sendFileChunkHashAckBuffer.Read(destinationFileChunkOffset) ||
					!sendFileChunkHashAckBuffer.Read(destinationFileChunkSize) ||
					!sendFileChunkHashAckBuffer.Read(destinationFileChunkHash))
				{
					WriteError("Error reading OPCodes::SendFileChunkHashAck");

					Close();

					return false;
				}
			}

			// send new chunk if doesn't match
			if (sourceFileChunkHash != destinationFileChunkHash)
			{
				std::int32_t bytesSaved;

				if (!SendCompressedChunk(*lpFileChunk, bytesSaved))
				{
					WriteError("Error sending compressed file chunk");

					Close();

					return false;
				}
			}

			fileOffset += lpFileChunk->BufferSize;

			onProgress(
				fileOffset,
				fileInfoSource.Size,
				lpParam
			);
		}

		// send OPCodes::SendFileChunk until end of source file
		while (fileOffset < fileInfoSource.Size)
		{
			fStream.seekg(
				static_cast<std::streampos>(fileOffset)
			);

			fStream.read(
				reinterpret_cast<char*>(&lpFileChunk->Buffer[0]),
				FILE_CHUNK_SIZE
			);

			lpFileChunk->BufferSize = static_cast<std::uint32_t>(
				fStream.gcount()
			);

			// send OPCodes::SendFileChunk
			{
				CreatePacketBuffer(sendFileChunkBuffer, OPCodes::SendFileChunk, sizeof(std::uint64_t) + sizeof(std::uint64_t));
				sendFileChunkBuffer.Write(fileOffset);
				sendFileChunkBuffer.Write<std::uint64_t>(lpFileChunk->BufferSize);

				if (!SendPacketBuffer(sendFileChunkBuffer))
				{
					WriteError("Error sending OPCodes::SendFileChunk");

					Close();

					return false;
				}
			}

			std::int32_t bytesSaved;

			if (!SendCompressedChunk(*lpFileChunk, bytesSaved))
			{
				WriteError("Error sending compressed file chunk");

				Close();

				return false;
			}

			fileOffset += lpFileChunk->BufferSize;

			onProgress(
				fileOffset,
				fileInfoSource.Size,
				lpParam
			);
		}

		// send OPCodes::CompleteSendFile
		{
			CreatePacketBuffer(completeSendFileBuffer, OPCodes::CompleteSendFile, 0);

			if (!SendPacketBuffer(completeSendFileBuffer))
			{
				WriteError("Error sending OPCodes::CompleteSendFile");

				Close();

				return false;
			}
		}

		// read OPCodes::CompleteSendFileAck
		{
			ByteBuffer completeSendFileAckBuffer;

			if (!ReadPacket(OPCodes::CompleteSendFileAck, completeSendFileAckBuffer, true))
			{
				WriteError("Error receiving OPCodes::CompleteSendFileAck");

				Close();

				return false;
			}
		}

		return true;
	}

	return false;
}

// @return false on connection closed
bool UFTSocket::ReceiveFile(const char* lpSource, const char* lpDestination, UFTSocket_OnReceiveProgress onProgress, void* lpParam)
{
	if (IsOpen() && IsConnected())
	{
		LockGuard lock(
			lpContext->IOMutex
		);

		FileInfo fileInfo = { 0 };
		int getFileInfoResult;

		if (!(getFileInfoResult = GetFileInfo(lpDestination, fileInfo)))
		{
			Close();

			return false;
		}

		bool destinationFileExists = getFileInfoResult > 0;

		UFTSocket_FileInfo fileInfoDestination;
		fileInfoDestination.Name = lpDestination;
		fileInfoDestination.Size = fileInfo.Size;
		fileInfoDestination.Timestamp = fileInfo.LastModified;

		// send OPCodes::ReceiveFile
		{
			std::string source(
				lpSource
			);

			CreatePacketBuffer(receiveFileBuffer, OPCodes::ReceiveFile, sizeof(std::uint8_t) + 255 + sizeof(std::uint64_t) + sizeof(std::uint32_t));
			receiveFileBuffer.Write<std::uint8_t>(source);
			receiveFileBuffer.Write(fileInfoDestination.Size);
			receiveFileBuffer.Write(fileInfoDestination.Timestamp);

			if (!SendPacketBuffer(receiveFileBuffer))
			{
				WriteError("Error sending OPCodes::ReceiveFile");

				Close();

				return false;
			}
		}

		UFTSocket_FileInfo fileInfoSource;

		// read OPCodes::ReceiveFileAck
		{
			ByteBuffer receiveFileAckBuffer;

			if (!ReadPacket(OPCodes::ReceiveFileAck, receiveFileAckBuffer, true))
			{
				WriteError("Error receiving OPCodes::ReceiveFileAck");

				Close();

				return false;
			}

			if (!receiveFileAckBuffer.Read(fileInfoSource.Size) ||
				!receiveFileAckBuffer.Read(fileInfoSource.Timestamp))
			{
				WriteError("Error reading OPCodes::ReceiveFileAck");

				Close();

				return false;
			}
		}

		if (!destinationFileExists)
		{
			std::ofstream fStream(
				lpDestination,
				std::ios::binary | std::ios::trunc
			);

			if (!fStream.is_open())
			{
				WriteError("Error opening std::ofstream");

				Close();

				return false;
			}

			// read OPCodes::ReceiveFileChunk until end of source file
			for (std::uint64_t fileChunkSize, fileOffset = 0; fileOffset < fileInfoSource.Size; )
			{
				ByteBuffer receiveFileChunkBuffer;

				if (!ReadPacket(OPCodes::ReceiveFileChunk, receiveFileChunkBuffer, true))
				{
					WriteError("Error receiving OPCodes::ReceiveFileChunk");

					Close();

					return false;
				}

				if (!receiveFileChunkBuffer.Read(fileOffset) ||
					!receiveFileChunkBuffer.Read(fileChunkSize))
				{
					WriteError("Error reading OPCodes::ReceiveFileChunk");

					Close();

					return false;
				}

				if (!ReceiveCompressedChunk(*lpFileChunk))
				{
					WriteError("Error receiving compressed file chunk");

					Close();

					return false;
				}

				fStream.seekp(
					static_cast<std::streampos>(fileOffset)
				);

				fStream.write(
					reinterpret_cast<const char*>(&lpFileChunk->Buffer[0]),
					static_cast<std::streamsize>(lpFileChunk->BufferSize)
				);

				fileOffset += fileChunkSize;

				onProgress(
					fileOffset,
					fileInfoSource.Size,
					lpParam
				);
			}
		}
		else
		{
			std::uint64_t fileOffset = 0;

			// verify and replace existing file buffer
			{
				std::fstream fStream(
					lpDestination,
					std::ios::binary | std::ios::in | std::ios::out | std::ios::ate
				);

				if (!fStream.is_open())
				{
					WriteError("Error opening std::fstream");

					Close();

					return false;
				}

				// read OPCodes::ReceiveFileChunkHash until end of destination file
				for (std::uint64_t sourceFileChunkHash, destinationFileChunkHash, fileChunkSize; fileOffset < fileInfoDestination.Size; )
				{
					ByteBuffer sendFileChunkHashBuffer;

					if (!ReadPacket(OPCodes::ReceiveFileChunkHash, sendFileChunkHashBuffer, true))
					{
						WriteError("Error receiving OPCodes::ReceiveFileChunkHash");

						Close();

						return false;
					}

					if (!sendFileChunkHashBuffer.Read(fileOffset) ||
						!sendFileChunkHashBuffer.Read(fileChunkSize) ||
						!sendFileChunkHashBuffer.Read(sourceFileChunkHash))
					{
						WriteError("Error reading OPCodes::ReceiveFileChunkHash");

						Close();

						return false;
					}

					fStream.seekg(
						fileOffset
					);

					fStream.read(
						reinterpret_cast<char*>(&lpFileChunk->Buffer[0]),
						fileChunkSize
					);

					lpFileChunk->BufferSize = static_cast<std::uint32_t>(
						fStream.gcount()
					);

					destinationFileChunkHash = CalculateHash(
						&lpFileChunk->Buffer[0],
						lpFileChunk->BufferSize
					);

					// send OPCodes::ReceiveFileChunkHashAck
					{
						CreatePacketBuffer(receiveFileChunkHashBuffer, OPCodes::ReceiveFileChunkHashAck, sizeof(std::uint64_t) + sizeof(std::uint64_t) + sizeof(std::uint64_t));
						receiveFileChunkHashBuffer.Write(fileOffset);
						receiveFileChunkHashBuffer.Write<std::uint64_t>(lpFileChunk->BufferSize);
						receiveFileChunkHashBuffer.Write(destinationFileChunkHash);

						if (!SendPacketBuffer(receiveFileChunkHashBuffer))
						{
							WriteError("Error sending OPCodes::ReceiveFileChunkHashAck");

							Close();

							return false;
						}
					}

					// replace chunk if it doesn't match
					if (sourceFileChunkHash != destinationFileChunkHash)
					{
						if (!ReceiveCompressedChunk(*lpFileChunk))
						{
							WriteError("Error receiving compressed file chunk");

							Close();

							return false;
						}

						// clear eof bit to continue seeking
						if (fStream.eof())
						{

							fStream.clear();
						}

						fStream.seekp(
							static_cast<std::streampos>(fileOffset)
						);

						fStream.write(
							reinterpret_cast<const char*>(&lpFileChunk->Buffer[0]),
							static_cast<std::streamsize>(lpFileChunk->BufferSize)
						);
					}

					fileOffset += fileChunkSize;

					onProgress(
						fileOffset,
						fileInfoSource.Size,
						lpParam
					);
				}
			}

			// write remaining file buffer, if any
			{
				std::ofstream fStream(
					lpDestination,
					std::ios::binary | std::ios::ate
				);

				if (!fStream.is_open())
				{
					WriteError("Error opening std::ofstream");

					Close();

					return false;
				}

				// read OPCodes::ReceiveFileChunk until end of source file
				for (std::uint64_t fileChunkSize; fileOffset < fileInfoSource.Size; )
				{
					ByteBuffer receiveFileChunkBuffer;

					if (!ReadPacket(OPCodes::ReceiveFileChunk, receiveFileChunkBuffer, true))
					{
						WriteError("Error receiving OPCodes::ReceiveFileChunk");

						Close();

						return false;
					}

					if (!receiveFileChunkBuffer.Read(fileOffset) ||
						!receiveFileChunkBuffer.Read(fileChunkSize))
					{
						WriteError("Error reading OPCodes::ReceiveFileChunk");

						Close();

						return false;
					}

					if (!ReceiveCompressedChunk(*lpFileChunk))
					{
						WriteError("Error receiving compressed file chunk");

						Close();

						return false;
					}

					fStream.seekp(
						static_cast<std::streampos>(fileOffset)
					);

					fStream.write(
						reinterpret_cast<const char*>(&lpFileChunk->Buffer[0]),
						static_cast<std::streamsize>(lpFileChunk->BufferSize)
					);

					fileOffset += fileChunkSize;

					onProgress(
						fileOffset,
						fileInfoSource.Size,
						lpParam
					);
				}
			}
		}

		// send OPCodes::CompleteReceiveFile
		{
			CreatePacketBuffer(completeSendFileBuffer, OPCodes::CompleteReceiveFile, 0);

			if (!SendPacketBuffer(completeSendFileBuffer))
			{
				WriteError("Error sending OPCodes::CompleteReceiveFile");

				Close();

				return false;
			}
		}

		// read OPCodes::CompleteReceiveFileAck
		{
			ByteBuffer completeReceiveFileAckBuffer;

			if (!ReadPacket(OPCodes::CompleteReceiveFileAck, completeReceiveFileAckBuffer, true))
			{
				WriteError("Error receiving OPCodes::CompleteReceiveFileAck");

				Close();

				return false;
			}
		}

		return true;
	}

	return false;
}

// @return number of bytes received
// @return 0 on connection closed
// @return -1 if would block
// @return -2 on api error
std::int32_t UFTSocket::ReadPacket(OPCodes opcode, ByteBuffer& buffer, bool block)
{
	PacketHeader packetHeader;
	std::int32_t bytesReceived;

	if ((bytesReceived = ReadNextPacket(packetHeader, buffer, block)) > 0)
	{
		if (packetHeader.OPCode != opcode)
		{
			std::stringstream ss;
			ss << "Received unexpected OPCode [Received: "
				<< static_cast<std::uint32_t>(packetHeader.OPCode)
				<< ", Expected: " << static_cast<std::uint32_t>(opcode) << "]";

			WriteError(ss.str().c_str());

			Close();

			return -2;
		}
	}

	return bytesReceived;
}

// @return number of bytes received
// @return 0 on connection closed
// @return -1 if would block
// @return -2 on api error
std::int32_t UFTSocket::ReadNextPacket(PacketHeader& header, ByteBuffer& buffer, bool block)
{
	std::int32_t bytesRead;

	if (!block)
	{
		if ((bytesRead = TryReceiveAll(&header, sizeof(PacketHeader))) <= 0)
		{

			return bytesRead;
		}
	}
	else
	{
		if ((bytesRead = ReceiveAll(&header, sizeof(PacketHeader))) <= 0)
		{

			return bytesRead;
		}
	}

	header.OPCode = BitConverter::NetworkToHost(
		header.OPCode
	);
	header.DataSize = BitConverter::NetworkToHost(
		header.DataSize
	);

#define UFTSOCKET_HPP_READ_PACKET_AND_RETURN_TOTAL_BYTES() \
	{ \
		buffer = ByteBuffer(static_cast<std::size_t>(header.DataSize)); \
		std::int32_t _bytesRead = 0; \
		\
		if (header.DataSize && ((_bytesRead = ReceiveAll(buffer.GetBuffer(), buffer.GetCapacity())) == 0)) \
		{ \
			return 0; \
		} \
		\
		buffer.SetOffsetW(_bytesRead); \
		return bytesRead + _bytesRead; \
	}

	switch (header.OPCode)
	{
		case OPCodes::GetFileList:
		case OPCodes::FileList:
		case OPCodes::FileListEntry:
		case OPCodes::SendFile:
		case OPCodes::SendFileAck:
		case OPCodes::SendFileChunk:
		case OPCodes::SendFileChunkHash:
		case OPCodes::SendFileChunkHashAck:
		case OPCodes::ReceiveFile:
		case OPCodes::ReceiveFileAck:
		case OPCodes::ReceiveFileChunk:
		case OPCodes::ReceiveFileChunkHash:
		case OPCodes::ReceiveFileChunkHashAck:
		case OPCodes::CompleteSendFile:
		case OPCodes::CompleteSendFileAck:
		case OPCodes::CompleteReceiveFile:
		case OPCodes::CompleteReceiveFileAck:
			UFTSOCKET_HPP_READ_PACKET_AND_RETURN_TOTAL_BYTES();
			break;

		default:
			Close();
			return -2;
	}

#undef UFTSOCKET_HPP_READ_PACKET_AND_RETURN_TOTAL_BYTES

	return bytesRead;
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

		WriteLastError("UDT::send");

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

// @return number of bytes sent
// @return 0 on connection closed
std::uint32_t UFTSocket::SendCompressedChunk(const FileChunk& chunk, std::int32_t& bytesSaved)
{
	CompressedFileChunkHeader header = { 0 };
	header.Size = chunk.BufferSize;
	
	{
		z_stream stream = { 0 };
		deflateInit(&stream, COMPRESSION_LEVEL);

		stream.next_in = const_cast<Bytef*>(
			reinterpret_cast<const Bytef*>(
				chunk.Buffer
			)
		);
		stream.next_out = reinterpret_cast<Bytef*>(
			&(*lpCompressedFileChunkBuffer)[0]
		);
		stream.avail_in = static_cast<uInt>(
			chunk.BufferSize
		);
		stream.avail_out = static_cast<uInt>(
			COMPRESSED_FILE_CHUNK_SIZE
		);

		deflate(&stream, Z_FINISH);

		header.CompressedSize = stream.total_out;

		deflateEnd(&stream);
	}

	auto headerCompressedSize = header.CompressedSize;;

	header.Size = BitConverter::HostToNetwork(header.Size);
	header.CompressedSize = BitConverter::HostToNetwork(header.CompressedSize);

	std::uint32_t bytesSent;
	std::uint32_t totalBytesSent = 0;

	if ((bytesSent = SendAll(&header, sizeof(CompressedFileChunkHeader))) == 0)
	{
		WriteError("Error sending CompressedFileChunkHeader");

		return 0;
	}

	totalBytesSent += bytesSent;

	if ((bytesSent = SendAll(&(*lpCompressedFileChunkBuffer)[0], headerCompressedSize)) == 0)
	{
		WriteError("Error sending compressed FileChunk");

		return 0;
	}

	totalBytesSent += bytesSent;

	bytesSaved += (chunk.BufferSize - headerCompressedSize);

	return totalBytesSent;
}

// @return number of bytes read
// @return 0 on connection closed
std::uint32_t UFTSocket::ReceiveCompressedChunk(FileChunk& chunk)
{
	std::uint32_t bytesRead;
	std::uint32_t totalBytesRead = 0;

	CompressedFileChunkHeader header;

	if ((bytesRead = ReceiveAll(&header, sizeof(CompressedFileChunkHeader))) == 0)
	{
		WriteError("Error reading CompressedFileChunkHeader");

		return 0;
	}

	totalBytesRead += bytesRead;

	header.Size = BitConverter::NetworkToHost(header.Size);
	header.CompressedSize = BitConverter::NetworkToHost(header.CompressedSize);

	if ((bytesRead = ReceiveAll(&(*lpCompressedFileChunkBuffer)[0], header.CompressedSize)) == 0)
	{
		WriteError("Error reading compressed FileChunk");

		return 0;
	}

	totalBytesRead += bytesRead;

	chunk.BufferSize = header.Size;

	{
		z_stream stream = { 0 };
		inflateInit(&stream);

		stream.next_in = reinterpret_cast<Bytef*>(
			&(*lpCompressedFileChunkBuffer)[0]
		);
		stream.next_out = reinterpret_cast<Bytef*>(
			chunk.Buffer
		);
		stream.avail_in = static_cast<uInt>(
			header.CompressedSize
		);
		stream.avail_out = static_cast<uInt>(
			header.Size
		);
		
		inflate(&stream, Z_FINISH);
		inflateEnd(&stream);
	}
	
	return totalBytesRead;
}

std::uint32_t UFTSocket::GetChunkCount(UFTSocket_FileSize fileSize)
{
	std::uint32_t count = 0;

	if (fileSize)
	{
		count = static_cast<std::uint32_t>(
			fileSize / FILE_CHUNK_SIZE
		);

		if (fileSize % FILE_CHUNK_SIZE)
		{

			++count;
		}
	}

	return count;
}

// @return 0 on error
// @return -1 if not found
int UFTSocket::GetFileInfo(const char* path, FileInfo& info)
{
#if defined(WIN32)
	struct _stat64 stat;
#else
	struct stat64 stat;
#endif

#if defined(WIN32)
	if (_stat64(path, &stat) == -1)
#else
	if (stat64(path, &stat) == -1)
#endif
	{
		auto lastError = errno;

		if ((lastError != ENOENT) && (lastError != ENOTDIR))
		{
#if defined(WIN32)
			WriteLastErrorOS("_stat64");
#else
			WriteLastErrorOS("stat64");
#endif

			return 0;
		}

		info.Size = 0;
		info.LastAccessed = 0;
		info.LastModified = 0;

		return -1;
	}

	info.Size = static_cast<UFTSocket_FileSize>(stat.st_size);
	info.LastAccessed = static_cast<UFTSocket_FileTimestamp>(stat.st_atime);
	info.LastModified = static_cast<UFTSocket_FileTimestamp>(stat.st_mtime);

	return 1;
}

// @return 0 on error
// @return -1 if not found
int UFTSocket::GetFilesInPath(const char* path, UFTSocket_FileInfoList& files)
{
#if !defined(WIN32)
	DIR* lpDIR;
	
	if ((lpDIR = opendir(path)) == NULL)
	{

		return -1;
	}

	dirent* lpEntry;

	FileInfo fileInfo;
	UFTSocket_FileInfo fileInfoEntry;

	char fileEntryPath[258];

	while ((lpEntry = readdir(lpDIR)) != NULL)
	{
		if (lpEntry->d_type == DT_DIR)
		{

			continue;
		}

		fileInfoEntry.Name.assign(
			lpEntry->d_name
		);

		snprintf(
			fileEntryPath,
			sizeof(fileEntryPath) - 1,
			"%s/%s",
			path,
			lpEntry->d_name
		);

		if (GetFileInfo(fileEntryPath, fileInfo) <= 0)
		{

			return 0;
		}

		fileInfoEntry.Size = fileInfo.Size;
		fileInfoEntry.Timestamp = fileInfo.LastModified;

		files.push_back(
			std::move(fileInfoEntry)
		);
	}

	closedir(lpDIR);
#else
	HANDLE hFind;
	WIN32_FIND_DATA fd;

	std::stringstream ss;
	ss << path << "/*";

	if ((hFind = FindFirstFile(ss.str().c_str(), &fd)) == INVALID_HANDLE_VALUE)
	{
		if (GetLastError() == ERROR_FILE_NOT_FOUND)
		{

			return -1;
		}

		return 0;
	}

	FileInfo fileInfo;
	UFTSocket_FileInfo fileInfoEntry;

	char fileEntryPath[256];

	do
	{
		if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
		{
			fileInfoEntry.Name.assign(
				fd.cFileName
			);

			snprintf(
				fileEntryPath,
				sizeof(fileEntryPath) - 1,
				"%s/%s",
				path,
				fd.cFileName
			);

			if (GetFileInfo(fileEntryPath, fileInfo) <= 0)
			{
				FindClose(hFind);
				
				return 0;
			}

			fileInfoEntry.Size = fileInfo.Size;
			fileInfoEntry.Timestamp = fileInfo.LastModified;

			files.push_back(
				std::move(fileInfoEntry)
			);
		}
	} while (FindNextFile(hFind, &fd));

	FindClose(hFind);
#endif

	return 1;
}

void UFTSocket::WriteError(const char* message)
{
	std::cerr << message << std::endl;
}

void UFTSocket::WriteLastError(const char* function)
{
	std::cerr << "Error calling " << function << ": (" << UDT::getlasterror().getErrorCode() << ") " << UDT::getlasterror().getErrorMessage() << std::endl;
}

void UFTSocket::WriteLastErrorOS(const char* function)
{
#if defined(WIN32)
	std::cerr << "Error calling " << function << ": " << errno << std::endl;
//	std::cerr << "Error calling " << function << ": " << GetLastError() << std::endl;
#else
	std::cerr << "Error calling " << function << ": " << errno << std::endl;
#endif
}
