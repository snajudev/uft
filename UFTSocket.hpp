// -----------------------------------------------------------------------------
// Program: Materials ISS Experiment (MISSE)
// File: UFTSocket.hpp
// Purpose: Socket wrapper for UDT file and buffer IO
// Written by: F. Barney
// Date: 6/14/2020
// -----------------------------------------------------------------------------

#ifndef UFTSOCKET_HPP
#define UFTSOCKET_HPP

#include <cstdint>

typedef std::uint64_t UFTSocket_FileSize;

typedef void(*UFTSocket_OnSendProgress)(UFTSocket_FileSize bytesSent, UFTSocket_FileSize fileSize, void* lpParam);

typedef void(*UFTSocket_OnReceiveProgress)(UFTSocket_FileSize bytesReceived, UFTSocket_FileSize fileSize, void* lpParam);

class UFTSocket
{
	struct Context;
	struct FileInfo;
	struct FileState;
	struct FileChunk;
	typedef std::uint64_t FileChunkHash;

	struct CompressedFileChunkHeader;
	
	enum class ErrorCodes : std::uint8_t;

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

	std::int32_t GetTimeout() const;

	bool Open();

	void Close();

	bool SetBlocking(bool set);

	bool SetTimeout(std::int32_t milliseconds);

	bool Listen(std::uint32_t host, std::uint16_t port, std::uint32_t backlog);

	bool Accept(UFTSocket& socket);

	bool Connect(std::uint32_t remoteHost, std::uint16_t remotePort);

	void Disconnect();

	// @return file size in bytes
	// @return -2 on api error
	// @return 0 on connection closed
	std::int64_t SendFile(const char* lpSource, const char* lpDestination)
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
	// @return file size in bytes
	// @return -2 on api error
	// @return 0 on connection closed
	std::int64_t SendFile(const char* lpSource, const char* lpDestination, UFTSocket_OnSendProgress onProgress, void* lpParam);

	// @return file size in bytes
	// @return -1 if would block
	// @return -2 on api error
	// @return 0 on connection closed
	std::int64_t ReceiveFile(char(&path)[255])
	{
		UFTSocket_OnReceiveProgress onProgress(
			[](UFTSocket_FileSize _bytesReceived, UFTSocket_FileSize _fileSize, void* _lpParam)
			{
			}
		);

		return ReceiveFile(
			path,
			onProgress,
			nullptr
		);
	}
	// @return file size in bytes
	// @return -1 if would block
	// @return -2 on api error
	// @return 0 on connection closed
	std::int64_t ReceiveFile(char(&path)[255], UFTSocket_OnReceiveProgress onProgress, void* lpParam);

private:
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

			if ((bytesSent = Send(&((const char*)lpBuffer)[i], size - i)) == 0)
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
			if ((bytesRead = Receive(&((char*)lpBuffer)[i], size - i)) == 0)
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

		if ((bytesRead = Receive(lpBuffer, size)) == 0)
		{

			return 0;
		}

		if (bytesRead == -1)
		{

			return -1;
		}

		for (std::uint32_t i = bytesRead; i < size; )
		{
			if ((bytesRead = Receive(&((char*)lpBuffer)[i], size - i)) == 0)
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

	static bool SetFileModificationTime(const char* path, const FileInfo& info);

	static void WriteError(const char* message);

	static void WriteLastError(const char* function);

	static void WriteLastErrorOS(const char* function);

	static FileChunkHash CalculateHash(const void* lpBuffer, std::size_t size)
	{
		static constexpr FileChunkHash FNV_1a_64_PRIME = 0x100000001B3;
		static constexpr FileChunkHash FNV_1a_64_OFFSET = 0xCBF29CE484222325;

		FileChunkHash hash = FNV_1a_64_OFFSET;

		for (std::size_t i = 0; i < size; ++i)
		{
			hash ^= *reinterpret_cast<const FileChunkHash*>(
				reinterpret_cast<const std::uint8_t*>(lpBuffer) + i
			);

			hash *= FNV_1a_64_PRIME;
		}

		return hash;
	}
};

#endif // !UFTSOCKET_HPP
