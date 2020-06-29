// -----------------------------------------------------------------------------
// Program: Materials ISS Experiment (MISSE)
// File: UFTSocket.cpp
// Purpose: Socket wrapper for UDT file and buffer IO
// Written by: F. Barney
// Date: 6/14/2020
// -----------------------------------------------------------------------------

#include "UFTSocket.hpp"

#include <udt.h>
#include <zlib.h>

#include <assert.h>
#include <string.h>
#include <iostream>
#include <utime.h>
#include <fstream>
#include <mutex>

#include <sys/stat.h>

typedef std::uint64_t UFTSocket_FileTimestamp;

static constexpr std::uint64_t FILE_CHUNK_SIZE = 256 * 1024; // 256kb
static constexpr std::int32_t  COMPRESSION_LEVEL = Z_DEFAULT_COMPRESSION;
static constexpr std::uint64_t COMPRESSED_FILE_CHUNK_SIZE = FILE_CHUNK_SIZE * 2;

struct UFTSocket::Context
{
	bool         IsOpen      = false;
	bool         IsBlocking  = true;
	bool         IsConnected = false;
	bool         IsListening = false;

	std::int32_t Timeout     = 15 * 1000;
	
	std::mutex   Mutex;

	UDTSOCKET    Socket;
};

struct UFTSocket::FileInfo
{
	UFTSocket_FileSize      Size;
	UFTSocket_FileTimestamp LastAccessed;
	UFTSocket_FileTimestamp LastModified;
};

struct UFTSocket::FileState
{
	UFTSocket_FileSize      Size;
	UFTSocket_FileTimestamp Timestamp;
	char                    Path[255];
} __attribute__((__packed__));

struct UFTSocket::FileChunk
{
	std::uint8_t  Buffer[FILE_CHUNK_SIZE];
	std::uint32_t BufferSize;
} __attribute__((__packed__));

struct UFTSocket::CompressedFileChunkHeader
{
	std::uint32_t Size;
	std::uint32_t CompressedSize;
} __attribute__((__packed__));

enum class UFTSocket::ErrorCodes : std::uint8_t
{
	Success,

	FileInfoFailed,
	FileOpenFailed
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

	if (UDT::connect(lpContext->Socket, (sockaddr*)&addr, sizeof(addr)) == UDT::ERROR)
	{
		WriteLastError("UDT::connect");

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

// @return file size in bytes
// @return -2 on api error
// @return 0 on connection closed
std::int64_t UFTSocket::SendFile(const char* lpSource, const char* lpDestination, UFTSocket_OnSendProgress onProgress, void* lpParam)
{
	assert(IsOpen());
	assert(IsConnected());

	std::lock_guard<std::mutex> lock(
		lpContext->Mutex
	);

	FileInfo fileInfo;
	int fileInfoStatus;

	if ((fileInfoStatus = GetFileInfo(lpSource, fileInfo)) <= 0)
	{
		switch (fileInfoStatus)
		{
			case 0:
				WriteError("Error getting FileInfo");
				break;

			case -1:
				WriteError("File not found");
				break;
		}

		return -2;
	}

	FileState localFileState;
	localFileState.Size = fileInfo.Size;
	localFileState.Timestamp = fileInfo.LastModified;
	snprintf(localFileState.Path, sizeof(FileState::Path), lpDestination);

	if (SendAll(&localFileState, sizeof(FileState)) == 0)
	{
		WriteError("Error sending local FileState");

		return 0;
	}

	{
		ErrorCodes errorCode;

		if (ReceiveAll(&errorCode, sizeof(ErrorCodes)) == 0)
		{
			WriteError("Error reading ErrorCodes");

			return 0;
		}

		if (errorCode != ErrorCodes::Success)
		{

			return -2;
		}
	}

	FileState remoteFileState;
	
	if (ReceiveAll(&remoteFileState, sizeof(FileState)) == 0)
	{
		WriteError("Error reading remote FileState");

		return 0;
	}

	auto localChunkCount = GetChunkCount(localFileState.Size);
//	std::cout << "Local chunk count: " << localChunkCount << std::endl;
	auto remoteChunkCount = GetChunkCount(remoteFileState.Size);
//	std::cout << "Remote chunk count: " << remoteChunkCount << std::endl;

	{
		ErrorCodes errorCode;

		if (ReceiveAll(&errorCode, sizeof(ErrorCodes)) == 0)
		{
			WriteError("Error reading ErrorCodes");

			return 0;
		}

		if (errorCode != ErrorCodes::Success)
		{

			return -2;
		}
	}

	std::ifstream fStream(
		lpSource,
		std::ios::binary
	);

	{
		auto errorCode = ErrorCodes::Success;

		if (!fStream.is_open())
		{

			errorCode = ErrorCodes::FileOpenFailed;
		}

		if (SendAll(&errorCode, sizeof(ErrorCodes)) == 0)
		{
			WriteError("Error sending ErrorCodes");

			return 0;
		}

		if (errorCode != ErrorCodes::Success)
		{

			return -2;
		}
	}

	std::int32_t bytesSavedFromCompression = 0;

	if ((remoteFileState.Size != 0) && (localFileState.Size >= remoteFileState.Size))
	{
		FileChunk localChunk;

//		std::cout << "Verifying " << remoteChunkCount << " remote chunks" << std::endl;

		for (std::uint32_t i = 0; i < remoteChunkCount; ++i)
		{
			fStream.seekg(
				i * FILE_CHUNK_SIZE,
				std::ios::beg
			);

			fStream.read(
				reinterpret_cast<char*>(localChunk.Buffer),
				FILE_CHUNK_SIZE
			);

			localChunk.BufferSize = fStream.gcount();

			auto localChunkHash = CalculateHash(
				localChunk.Buffer,
				localChunk.BufferSize
			);

			if (SendAll(&localChunkHash, sizeof(FileChunkHash)) == 0)
			{
				WriteError("Error sending local FileChunkHash");

				return 0;
			}

			FileChunkHash remoteChunkHash;

			if (ReceiveAll(&remoteChunkHash, sizeof(FileChunkHash)) == 0)
			{
				WriteError("Error reading remote FileChunkHash");

				return 0;
			}

//			std::cout << "Comparing local chunk hash (" << localChunkHash << ") against remote (" << remoteChunkHash << ")" << std::endl;

			if (localChunkHash != remoteChunkHash)
			{
//				std::cout << "Resending chunk #" << i << std::endl;

				if (SendCompressedChunk(localChunk, bytesSavedFromCompression) == 0)
				{
					WriteError("Error sending local FileChunk");

					return 0;
				}
			}

			onProgress(
				(i * FILE_CHUNK_SIZE) + localChunk.BufferSize,
				localFileState.Size,
				lpParam
			);
		}

//		std::cout << "Sending " << localChunkCount - remoteChunkCount << " local chunks" << std::endl;

		for (std::uint32_t i = remoteChunkCount; i < localChunkCount; ++i)
		{
			fStream.seekg(
				i * FILE_CHUNK_SIZE,
				std::ios::beg
			);

			fStream.read(
				reinterpret_cast<char*>(localChunk.Buffer),
				FILE_CHUNK_SIZE
			);

			localChunk.BufferSize = fStream.gcount();

//			std::cout << "Sending " << localChunk.BufferSize << " bytes" << std::endl;

			if (SendAll(&localChunk, sizeof(FileChunk)) == 0)
			{
				WriteError("Error sending local FileChunk");

				return 0;
			}

			onProgress(
				(i * FILE_CHUNK_SIZE) + localChunk.BufferSize,
				localFileState.Size,
				lpParam
			);
		}
	}
	else
	{
		FileChunk localChunk;

//		std::cout << "Sending " << localChunkCount << " local chunks" << std::endl;

		for (std::uint32_t i = 0; i < localChunkCount; ++i)
		{
			fStream.seekg(
				i * FILE_CHUNK_SIZE,
				std::ios::beg
			);

			fStream.read(
				reinterpret_cast<char*>(localChunk.Buffer),
				FILE_CHUNK_SIZE
			);

			localChunk.BufferSize = fStream.gcount();

//			std::cout << "Sending " << localChunk.BufferSize << " bytes" << std::endl;

			if (SendCompressedChunk(localChunk, bytesSavedFromCompression) == 0)
			{
				WriteError("Error sending local FileChunk");

				return 0;
			}

			onProgress(
				(i * FILE_CHUNK_SIZE) + localChunk.BufferSize,
				localFileState.Size,
				lpParam
			);
		}
	}

//	std::cout << "Saved " << bytesSavedFromCompression << " bytes with compression" << std::endl;

	return static_cast<std::int64_t>(
		localFileState.Size
	);
}

// @return file size in bytes
// @return -1 if would block
// @return -2 on api error
// @return 0 on connection closed
std::int64_t UFTSocket::ReceiveFile(char(&path)[255], UFTSocket_OnReceiveProgress onProgress, void* lpParam)
{
	assert(IsOpen());
	assert(IsConnected());

	std::lock_guard<std::mutex> lock(
		lpContext->Mutex
	);

	FileState remoteFileState;

	switch (TryReceiveAll(&remoteFileState, sizeof(FileState)))
	{
		case 0: return 0;
		case -1: return -1;
	}

	FileInfo fileInfo;
	int fileInfoStatus;
	
	if ((fileInfoStatus = GetFileInfo(remoteFileState.Path, fileInfo)) == 0)
	{
		WriteError("Error getting local FileInfo");

		auto errorCode = ErrorCodes::FileInfoFailed;

		if (SendAll(&errorCode, sizeof(ErrorCodes)) == 0)
		{
			WriteError("Error sending ErrorCodes");

			return 0;
		}

		return -2;
	}

	{
		auto errorCode = ErrorCodes::Success;

		if (SendAll(&errorCode, sizeof(ErrorCodes)) == 0)
		{
			WriteError("Error sending ErrorCodes");

			return 0;
		}
	}

	FileState localFileState;
	localFileState.Size = fileInfo.Size;
	localFileState.Timestamp = fileInfo.LastModified;
	snprintf(localFileState.Path, sizeof(FileState::Path), remoteFileState.Path);

	if (SendAll(&localFileState, sizeof(FileState)) == 0)
	{
		WriteError("Error sending local FileState");

		return 0;
	}

	auto localChunkCount = GetChunkCount(localFileState.Size);
//	std::cout << "Local chunk count: " << localChunkCount << std::endl;
	auto remoteChunkCount = GetChunkCount(remoteFileState.Size);
//	std::cout << "Remote chunk count: " << remoteChunkCount << std::endl;

	auto SyncFileModificationTime = [&fileInfo, &localFileState, &remoteFileState]()
	{
		fileInfo.LastModified = remoteFileState.Timestamp;

		if (!SetFileModificationTime(localFileState.Path, fileInfo))
		{
			WriteError("Error syncing file modification time");

			return false;
		}

		return true;
	};

	if ((fileInfoStatus != -1) && (localFileState.Size <= remoteFileState.Size))
	{
		std::fstream fStream(
			localFileState.Path,
			std::ios::in | std::ios::out | std::ios::binary | std::ios::ate
		);

		{
			auto errorCode = ErrorCodes::Success;

			if (!fStream.is_open())
			{

				errorCode = ErrorCodes::FileOpenFailed;
			}

			if (SendAll(&errorCode, sizeof(ErrorCodes)) == 0)
			{
				WriteError("Error sending ErrorCodes");

				return 0;
			}

			if (errorCode != ErrorCodes::Success)
			{

				return -2;
			}
		}

		{
			ErrorCodes errorCode;

			if (ReceiveAll(&errorCode, sizeof(ErrorCodes)) == 0)
			{
				WriteError("Error reading ErrorCodes");

				return 0;
			}

			if (errorCode != ErrorCodes::Success)
			{

				return -2;
			}
		}

		FileChunk localChunk;
		FileChunk remoteChunk;

//		std::cout << "Verifying " << localChunkCount << " local chunks" << std::endl;

		for (std::uint32_t i = 0; i < localChunkCount; ++i)
		{
			fStream.seekg(
				i * FILE_CHUNK_SIZE,
				std::ios::beg
			);

			fStream.read(
				reinterpret_cast<char*>(localChunk.Buffer),
				FILE_CHUNK_SIZE
			);

			localChunk.BufferSize = fStream.gcount();

			auto localChunkHash = CalculateHash(
				localChunk.Buffer,
				localChunk.BufferSize
			);

			FileChunkHash remoteChunkHash;

			if (ReceiveAll(&remoteChunkHash, sizeof(FileChunkHash)) == 0)
			{
				WriteError("Error reading remote FileChunkHash");

				fStream.close();
				SyncFileModificationTime();

				return 0;
			}

			if (SendAll(&localChunkHash, sizeof(FileChunkHash)) == 0)
			{
				WriteError("Error sending local FileChunkHash");

				fStream.close();
				SyncFileModificationTime();

				return 0;
			}

//			std::cout << "Comparing local chunk hash (" << localChunkHash << ") against remote (" << remoteChunkHash << ")" << std::endl;

			if (localChunkHash != remoteChunkHash)
			{
//				std::cout << "Rewriting chunk #" << i << std::endl;

				if (ReceiveCompressedChunk(remoteChunk) == 0)
				{
					WriteError("Error reading remote FileChunk");

					fStream.close();
					SyncFileModificationTime();

					return 0;
				}

				fStream.seekp(
					i * FILE_CHUNK_SIZE,
					std::ios::beg
				);

				fStream.write(
					reinterpret_cast<const char*>(remoteChunk.Buffer),
					remoteChunk.BufferSize
				);
			}

			onProgress(
				(i * FILE_CHUNK_SIZE) + localChunk.BufferSize,
				remoteFileState.Size,
				lpParam
			);
		}

		for (std::uint32_t i = localChunkCount; i < remoteChunkCount; ++i)
		{
			if (ReceiveCompressedChunk(remoteChunk) == 0)
			{
				WriteError("Error reading remote FileChunk");

				fStream.close();
				SyncFileModificationTime();

				return 0;
			}

//			std::cout << "Writing " << remoteChunk.BufferSize << " bytes" << std::endl;

			fStream.seekp(
				i * FILE_CHUNK_SIZE,
				std::ios::beg
			);

			fStream.write(
				reinterpret_cast<const char*>(remoteChunk.Buffer),
				remoteChunk.BufferSize
			);

			onProgress(
				(i * FILE_CHUNK_SIZE) + remoteChunk.BufferSize,
				remoteFileState.Size,
				lpParam
			);
		}
	}
	else
	{
		std::ofstream fStream(
			localFileState.Path,
			std::ios::binary | std::ios::trunc
		);

		{
			auto errorCode = ErrorCodes::Success;

			if (!fStream.is_open())
			{

				errorCode = ErrorCodes::FileOpenFailed;
			}

			if (SendAll(&errorCode, sizeof(ErrorCodes)) == 0)
			{
				WriteError("Error sending ErrorCodes");

				return 0;
			}

			if (errorCode != ErrorCodes::Success)
			{

				return -2;
			}
		}

		{
			ErrorCodes errorCode;

			if (ReceiveAll(&errorCode, sizeof(ErrorCodes)) == 0)
			{
				WriteError("Error reading ErrorCodes");

				return 0;
			}

			if (errorCode != ErrorCodes::Success)
			{

				return -2;
			}
		}

		FileChunk remoteChunk;

//		std::cout << "Reading " << remoteChunkCount << " remote chunks" << std::endl;

		for (std::uint32_t i = 0; i < remoteChunkCount; ++i)
		{
			if (ReceiveCompressedChunk(remoteChunk) == 0)
			{
				WriteError("Error reading remote FileChunk");

				fStream.close();
				SyncFileModificationTime();

				return 0;
			}

//			std::cout << "Writing " << remoteChunk.BufferSize << " bytes" << std::endl;

			fStream.write(
				reinterpret_cast<const char*>(remoteChunk.Buffer),
				remoteChunk.BufferSize
			);

			onProgress(
				(i * FILE_CHUNK_SIZE) + remoteChunk.BufferSize,
				remoteFileState.Size,
				lpParam
			);
		}
	}

	if (!SyncFileModificationTime())
	{

		return -2;
	}

	snprintf(
		path,
		sizeof(path),
		localFileState.Path
	);

	return static_cast<std::int64_t>(
		remoteFileState.Size
	);
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
		if (UDT::getlasterror().getErrorCode() == CUDTException::EASYNCSND)
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
	
	std::uint8_t buffer[COMPRESSED_FILE_CHUNK_SIZE];

	{
		z_stream stream = { 0 };
		deflateInit(&stream, COMPRESSION_LEVEL);

		stream.next_in = const_cast<Bytef*>(
			reinterpret_cast<const Bytef*>(
				chunk.Buffer
			)
		);
		stream.next_out = reinterpret_cast<Bytef*>(
			buffer
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

	std::uint32_t bytesSent;
	std::uint32_t totalBytesSent = 0;

	if ((bytesSent = SendAll(&header, sizeof(CompressedFileChunkHeader))) == 0)
	{
		WriteError("Error sending CompressedFileChunkHeader");

		return 0;
	}

	totalBytesSent += bytesSent;

	if ((bytesSent = SendAll(buffer, header.CompressedSize)) == 0)
	{
		WriteError("Error sending compressed FileChunk");

		return 0;
	}

	totalBytesSent += bytesSent;

	bytesSaved += (chunk.BufferSize - header.CompressedSize);

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

	std::uint8_t buffer[COMPRESSED_FILE_CHUNK_SIZE];

	if ((bytesRead = ReceiveAll(buffer, header.CompressedSize)) == 0)
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
			buffer
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
		count = fileSize / FILE_CHUNK_SIZE;

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
	struct stat64 stat;

	if (stat64(path, &stat) == -1)
	{
		auto lastError = errno;

		if ((lastError != ENOENT) && (lastError != ENOTDIR))
		{
			WriteLastErrorOS("stat64");

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

bool UFTSocket::SetFileModificationTime(const char* path, const FileInfo& info)
{
	utimbuf times;
	times.actime = static_cast<__time_t>(
		info.LastAccessed
	);
	times.modtime = static_cast<__time_t>(
		info.LastModified
	);

	if (utime(path, &times) == -1)
	{
		WriteLastErrorOS("utime");

		return false;
	}

	return true;
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
	std::cerr << "Error calling " << function << ": " << errno << std::endl;
}
