// -----------------------------------------------------------------------------
// Written by: F. Barney
// Date: 6/14/2020
// -----------------------------------------------------------------------------

#ifndef UFTSOCKET_HPP
#define UFTSOCKET_HPP

#include <list>
#include <array>
#include <string>
#include <cstdint>

#include <zlib.h>

class ByteBuffer;

typedef std::uint64_t UFTSocket_FileSize;

typedef std::uint64_t UFTSocket_FileTimestamp;

struct UFTSocket_FileInfo
{
	std::string             Name;
	UFTSocket_FileSize      Size;
	UFTSocket_FileTimestamp Timestamp;
};

typedef std::list<UFTSocket_FileInfo> UFTSocket_FileInfoList;

typedef void(*UFTSocket_OnGetFileList)(const UFTSocket_FileInfoList& files, void* lpParam);

typedef void(*UFTSocket_OnSendProgress)(UFTSocket_FileSize bytesSent, UFTSocket_FileSize fileSize, void* lpParam);

typedef void(*UFTSocket_OnReceiveProgress)(UFTSocket_FileSize bytesReceived, UFTSocket_FileSize fileSize, void* lpParam);

class UFTSocket
{
	static constexpr std::uint64_t FILE_CHUNK_SIZE = 1 * (1024 * 1024); // 1MB
	static constexpr std::int32_t  COMPRESSION_LEVEL = Z_DEFAULT_COMPRESSION;
	static constexpr std::uint64_t COMPRESSED_FILE_CHUNK_SIZE = FILE_CHUNK_SIZE * 2;

	struct Context;

	struct FileInfo
	{
		UFTSocket_FileSize      Size;
		UFTSocket_FileTimestamp LastAccessed;
		UFTSocket_FileTimestamp LastModified;
	};

	struct FileChunk
	{
		std::uint8_t  Buffer[FILE_CHUNK_SIZE];
		std::uint32_t BufferSize;
	};

	typedef std::uint64_t FileChunkHash;

	enum class OPCodes : std::uint8_t
	{
		GetFileList,			// std::string Path

		FileList,				// std::uint32 Count
		FileListEntry,			// std::string Name			std::uint64 Size			std::uint64 Timestamp

		SendFile,				// std::string Destination	std::uint64 Size			std::uint64 Timestamp
		SendFileAck,			// std::uint64 Size			std::uint64 Timestamp
		SendFileChunk,			// std::uint64 Offset		std::uint64 Size
		SendFileChunkHash,		// std::uint64 Offset		std::uint64 Size			std::uint64 Hash
		SendFileChunkHashAck,	// std::uint64 Offset		std::uint64 Size			std::uint64 Hash

		ReceiveFile,			// std::string Source		std::uint64 Size			std::uint64 Timestamp
		ReceiveFileAck,			// std::uint64 Size			std::uint64 Timestamp
		ReceiveFileChunk,		// std::uint64 Offset		std::uint64 Size
		ReceiveFileChunkHash,	// std::uint64 Offset		std::uint64 Size			std::uint64 Hash
		ReceiveFileChunkHashAck,// std::uint64 Offset		std::uint64 Size			std::uint64 Hash

		CompleteSendFile,
		CompleteSendFileAck,

		CompleteReceiveFile,
		CompleteReceiveFileAck,

		COUNT
	};

	enum class ErrorCodes : std::uint8_t
	{
		Success,

		FileInfoFailed,
		FileOpenFailed
	};

#pragma pack(push, 1)
	struct PacketHeader
	{
		OPCodes       OPCode;
		std::uint64_t DataSize;
	};

	struct CompressedFileChunkHeader
	{
		std::uint32_t Size;
		std::uint32_t CompressedSize;
	};
#pragma pack(pop)

	typedef std::array<std::uint8_t, COMPRESSED_FILE_CHUNK_SIZE> CompressedFileChunkBuffer;

	Context* lpContext;
	FileChunk* lpFileChunk;
	CompressedFileChunkBuffer* lpCompressedFileChunkBuffer;

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

	bool Open();

	void Close();

	bool SetBlocking(bool set);

	bool SetTimeout(std::int32_t milliseconds);

	bool Listen(std::uint32_t host, std::uint16_t port, std::uint32_t backlog);

	bool Accept(UFTSocket& socket);

	bool Connect(std::uint32_t remoteHost, std::uint16_t remotePort);

	void Disconnect();

	// @return false on connection closed
	bool Update();

	// @return false on connection closed
	bool GetFileList(const char* lpSource, UFTSocket_OnGetFileList onGetFileList, void* lpParam);

	// @return false on connection closed
	bool SendFile(const char* lpSource, const char* lpDestination)
	{
		UFTSocket_OnSendProgress onProgress(
			[](UFTSocket_FileSize _bytesSent, UFTSocket_FileSize _fileSize, void* _lpParam)
			{
			}
		);

		return SendFile(
			lpSource,
			lpDestination,
			onProgress,
			nullptr
		);
	}
	// @return false on connection closed
	bool SendFile(const char* lpSource, const char* lpDestination, UFTSocket_OnSendProgress onProgress, void* lpParam);

	// @return false on connection closed
	bool ReceiveFile(const char* lpSource, const char* lpDestination)
	{
		UFTSocket_OnReceiveProgress onProgress(
			[](UFTSocket_FileSize _bytesReceived, UFTSocket_FileSize _fileSize, void* _lpParam)
			{
			}
		);

		return ReceiveFile(
			lpSource,
			lpDestination,
			onProgress,
			nullptr
		);
	}
	// @return false on connection closed
	bool ReceiveFile(const char* lpSource, const char* lpDestination, UFTSocket_OnReceiveProgress onProgress, void* lpParam);

	UFTSocket& operator = (UFTSocket&&) = delete;

private:
	// @return number of bytes received
	// @return 0 on connection closed
	// @return -1 if would block
	// @return -2 on api error
	std::int32_t ReadPacket(OPCodes opcode, ByteBuffer& buffer, bool block);

	// @return number of bytes received
	// @return 0 on connection closed
	// @return -1 if would block
	// @return -2 on api error
	std::int32_t ReadNextPacket(PacketHeader& header, ByteBuffer& buffer, bool block);

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

	// @return number of bytes sent
	// @return 0 on connection closed
	std::uint32_t SendCompressedChunk(const FileChunk& chunk, std::int32_t& bytesSaved);

	// @return number of bytes read
	// @return 0 on connection closed
	std::uint32_t ReceiveCompressedChunk(FileChunk& chunk);

	static std::uint32_t GetChunkCount(UFTSocket_FileSize fileSize);

	// @return 0 on error
	// @return -1 if not found
	static int GetFileInfo(const char* path, FileInfo& info);

	// @return 0 on error
	// @return -1 if not found
	static int GetFilesInPath(const char* path, UFTSocket_FileInfoList& files);

	static void WriteError(const char* message);

	static void WriteLastError(const char* function);

	static void WriteLastErrorOS(const char* function);

	static FileChunkHash CalculateHash(const void* lpBuffer, std::uint32_t size)
	{
		static constexpr FileChunkHash FNV_1a_64_PRIME = 0x100000001B3;
		static constexpr FileChunkHash FNV_1a_64_OFFSET = 0xCBF29CE484222325;

		FileChunkHash hash = FNV_1a_64_OFFSET;

		for (std::uint32_t i = 0; i < size; ++i)
		{
			hash ^= static_cast<FileChunkHash>(
				*(reinterpret_cast<const std::uint8_t*>(lpBuffer) + i)
			);

			hash *= FNV_1a_64_PRIME;
		}

		return hash;
	}
};

#endif // !UFTSOCKET_HPP
