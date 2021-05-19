// -----------------------------------------------------------------------------
// Written by: F. Barney
// Date: 5/17/2021
// -----------------------------------------------------------------------------

#ifndef UFTSESSION_HPP
#define UFTSESSION_HPP

#include "UFTSocket.hpp"
#include "ByteBuffer.hpp"
#include "BitConverter.hpp"

#include <list>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

#include <zlib.h>
#include <assert.h>

#if defined(WIN32) || defined(_WIN32)
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

struct UFTSession_FileListEntry
{
	std::string   Path;
	std::uint64_t Size;
	std::uint32_t Timestamp;

	UFTSession_FileListEntry()
	{
	}

	UFTSession_FileListEntry(UFTSession_FileListEntry&& entry)
		: Path(
			std::move(entry.Path)
		),
		Size(
			entry.Size
		),
		Timestamp(
			entry.Timestamp
		)
	{
	}

	UFTSession_FileListEntry(const UFTSession_FileListEntry&) = delete;

	UFTSession_FileListEntry(std::string&& path, std::uint64_t size, std::uint32_t timestamp)
		: Path(
			std::move(path)
		),
		Size(
			size
		),
		Timestamp(
			timestamp
		)
	{
	}
};

typedef std::vector<UFTSession_FileListEntry> UFTSession_FileList;

typedef void(*UFTSession_OnSendProgress)(std::uint64_t bytesSent, std::uint64_t fileSize, void* lpParam);

typedef void(*UFTSession_OnReceiveProgress)(std::uint64_t bytesReceived, std::uint64_t fileSize, void* lpParam);

#define UFTSession_InitPacketBuffer(buffer, opcode, capacity) \
	buffer = ByteBuffer(sizeof(PacketHeader) + capacity); \
	buffer.Write(opcode); \
	buffer.Write(decltype(PacketHeader::PayloadSize)(0))

#define UFTSession_CreatePacketBuffer(buffer, opcode, capacity) \
	ByteBuffer buffer(sizeof(PacketHeader) + capacity); \
	buffer.Write(opcode); \
	buffer.Write(decltype(PacketHeader::PayloadSize)(0))

// @return number of bytes sent
// @return 0 on connection closed
#define UFTSession_SendPacketBuffer(buffer) \
	[this, &buffer]() \
	{ \
		auto bufferSize = buffer.GetSize(); \
		buffer.SetOffsetW(sizeof(OPCodes)); \
		buffer.Write(static_cast<decltype(PacketHeader::PayloadSize)>(bufferSize - sizeof(PacketHeader))); \
		buffer.SetOffsetW(bufferSize); \
		return GetSocket().SendAll(buffer.GetBuffer(), bufferSize); \
	}()

enum UFTSESSION_ERROR_CODES : std::uint32_t
{
	UFTSESSION_ERROR_CODE_SUCCESS,

	UFTSESSION_ERROR_CODE_REMOTE_ERROR,
	UFTSESSION_ERROR_CODE_ACCESS_DENIED,

	UFTSESSION_ERROR_CODE_NETWORK_API_ERROR,
	UFTSESSION_ERROR_CODE_NETWORK_WOULD_BLOCK,
	UFTSESSION_ERROR_CODE_NETWORK_NOT_CONNECTED,
	UFTSESSION_ERROR_CODE_NETWORK_CONNECTION_LOST,

	UFTSESSION_ERROR_CODE_FILESYSTEM_FILE_NOT_FOUND,
	UFTSESSION_ERROR_CODE_FILESYSTEM_OPEN_STREAM_FAILED,
};

static std::string UFTSESSION_ERROR_CODES_ToString(UFTSESSION_ERROR_CODES errorCode)
{
	switch (errorCode)
	{
		case UFTSESSION_ERROR_CODE_SUCCESS:
			return "UFTSESSION_ERROR_CODE_SUCCESS";

		case UFTSESSION_ERROR_CODE_REMOTE_ERROR:
			return "UFTSESSION_ERROR_CODE_REMOTE_ERROR";
		case UFTSESSION_ERROR_CODE_ACCESS_DENIED:
			return "UFTSESSION_ERROR_CODE_ACCESS_DENIED";

		case UFTSESSION_ERROR_CODE_NETWORK_API_ERROR:
			return "UFTSESSION_ERROR_CODE_NETWORK_API_ERROR";
		case UFTSESSION_ERROR_CODE_NETWORK_WOULD_BLOCK:
			return "UFTSESSION_ERROR_CODE_NETWORK_WOULD_BLOCK";
		case UFTSESSION_ERROR_CODE_NETWORK_NOT_CONNECTED:
			return "UFTSESSION_ERROR_CODE_NETWORK_NOT_CONNECTED";
		case UFTSESSION_ERROR_CODE_NETWORK_CONNECTION_LOST:
			return "UFTSESSION_ERROR_CODE_NETWORK_CONNECTION_LOST";

		case UFTSESSION_ERROR_CODE_FILESYSTEM_FILE_NOT_FOUND:
			return "UFTSESSION_ERROR_CODE_FILESYSTEM_FILE_NOT_FOUND";
		case UFTSESSION_ERROR_CODE_FILESYSTEM_OPEN_STREAM_FAILED:
			return "UFTSESSION_ERROR_CODE_FILESYSTEM_OPEN_STREAM_FAILED";
	}

	return std::to_string(
		errorCode
	);
}

class UFTSession
{
	static constexpr std::uint64_t FILE_CHUNK_SIZE = 10 * (1024 * 1024); // 10MB

	static constexpr std::int32_t  FILE_COMPRESSION_LEVEL = Z_BEST_SPEED;

	enum class OPCodes : std::uint8_t
	{
		GetFileList,
		GetFileListResult,

		TransmitFile,
		TransmitFileHash,
		TransmitFileChunk,
		TransmitFileChunkResult
	};

	enum class TransmitFileDirections : std::uint8_t
	{
		Up, Down
	};

	struct String8
	{
		std::uint8_t Length;
		char         Buffer[0xFF + 1];

		String8()
			: Length(
				0
			)
		{
		}

		explicit String8(const char* value)
		{
			Assign(value);
		}

		void Assign(const char* value)
		{
			Length = static_cast<std::uint8_t>(
				strlen(value)
			);

			memcpy(
				&Buffer[0],
				value,
				Length
			);

			Buffer[Length] = 0;
		}
	};

	struct FileInfo
	{
		String8       Path;
		std::uint64_t Size;
		std::uint32_t Timestamp;

		FileInfo()
			: Size(
				0
			),
			Timestamp(
				0
			)
		{
		}

		explicit FileInfo(const char* path)
			: Path(
				path
			),
			Size(
				0
			),
			Timestamp(
				0
			)
		{
		}
	};

#pragma pack(push, 1)
	struct PacketHeader
	{
		OPCodes       OPCode;
		std::uint64_t PayloadSize;
	};
#pragma pack(pop)

	typedef std::uint64_t FileChunkHash;

	typedef std::list<FileInfo> FileInfoList;

	typedef std::vector<std::uint8_t> FileChunkBuffer;

	UFTSocket socket;

	UFTSession(UFTSession&&) = delete;
	UFTSession(const UFTSession&) = delete;

public:
	UFTSession()
		: UFTSession(
			UFTSocket()
		)
	{
	}

	explicit UFTSession(UFTSocket&& socket)
		: socket(
			std::move(socket)
		)
	{
	}

	virtual ~UFTSession()
	{
	}

	bool IsConnected() const
	{
		return socket.IsConnected();
	}

	UFTSocket& GetSocket()
	{
		return socket;
	}
	const UFTSocket& GetSocket() const
	{
		return socket;
	}

	auto GetRemotePort() const
	{
		return GetSocket().GetRemotePort();
	}

	auto GetRemoteAddress() const
	{
		return GetSocket().GetRemoteAddress();
	}
	
	bool SetTimeout(std::int32_t ms)
	{
		return GetSocket().SetTimeout(
			ms
		);
	}

	UFTSESSION_ERROR_CODES Update()
	{
		if (!IsConnected())
		{

			return UFTSESSION_ERROR_CODE_NETWORK_NOT_CONNECTED;
		}

		UFTSESSION_ERROR_CODES errorCode;

		if ((errorCode = OnUpdate()) != UFTSESSION_ERROR_CODE_SUCCESS)
		{

			return errorCode;
		}

		return UFTSESSION_ERROR_CODE_SUCCESS;
	}

	UFTSESSION_ERROR_CODES GetFileList(UFTSession_FileList& files, const char* lpPath)
	{
		if (!IsConnected())
		{

			return UFTSESSION_ERROR_CODE_NETWORK_NOT_CONNECTED;
		}

		FileInfoList fileInfoList;

		{
			UFTSocket_IOLockGuard ioLock(
				GetSocket()
			);

			UFTSESSION_ERROR_CODES errorCode;

			if ((errorCode = ReceiveFileList(fileInfoList, lpPath)) != UFTSESSION_ERROR_CODE_SUCCESS)
			{

				return errorCode;
			}
		}

		files.resize(
			fileInfoList.size()
		);

		std::size_t i = 0;
		
		for (auto& fileInfo : fileInfoList)
		{
			auto lpFileListEntry = &files[i++];

			lpFileListEntry->Path.assign(
				fileInfo.Path.Buffer
			);
			lpFileListEntry->Size = fileInfo.Size;
			lpFileListEntry->Timestamp = fileInfo.Timestamp;
		}

		return UFTSESSION_ERROR_CODE_SUCCESS;
	}

	UFTSESSION_ERROR_CODES SendFile(const char* lpSource, const char* lpDestination)
	{
		UFTSession_OnSendProgress onProgress(
			[](std::uint64_t _bytesSent, std::uint64_t _fileSize, void* _lpParam)
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
	UFTSESSION_ERROR_CODES SendFile(const char* lpSource, const char* lpDestination, UFTSession_OnSendProgress onProgress, void* lpParam)
	{
		if (!IsConnected())
		{

			return UFTSESSION_ERROR_CODE_NETWORK_NOT_CONNECTED;
		}

		UFTSocket_IOLockGuard ioLock(
			GetSocket()
		);

		return TransmitFile(
			lpSource,
			lpDestination,
			TransmitFileDirections::Up,
			onProgress,
			lpParam
		);
	}

	UFTSESSION_ERROR_CODES ReceiveFile(const char* lpSource, const char* lpDestination)
	{
		UFTSession_OnReceiveProgress onProgress(
			[](std::uint64_t _bytesReceived, std::uint64_t _fileSize, void* _lpParam)
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
	UFTSESSION_ERROR_CODES ReceiveFile(const char* lpSource, const char* lpDestination, UFTSession_OnReceiveProgress onProgress, void* lpParam)
	{
		if (!IsConnected())
		{

			return UFTSESSION_ERROR_CODE_NETWORK_NOT_CONNECTED;
		}

		UFTSocket_IOLockGuard ioLock(
			GetSocket()
		);

		return TransmitFile(
			lpSource,
			lpDestination,
			TransmitFileDirections::Down,
			onProgress,
			lpParam
		);
	}

	void Disconnect()
	{
		if (GetSocket().IsOpen())
		{
			GetSocket().Close();
		}
	}

protected:
	virtual UFTSESSION_ERROR_CODES OnUpdate()
	{
		UFTSESSION_ERROR_CODES errorCode;

		// Read packet(s) and handle if available
		{
			ByteBuffer packetBuffer;
			PacketHeader packetHeader;
			std::uint32_t bytesReceived;

			UFTSocket_IOLockGuard lock(
				GetSocket()
			);

			while ((errorCode = ReadNextPacket(packetHeader, packetBuffer, bytesReceived, false)) == UFTSESSION_ERROR_CODE_SUCCESS)
			{
				if ((errorCode = HandlePacket(packetHeader, packetBuffer)) != UFTSESSION_ERROR_CODE_SUCCESS)
				{

					return errorCode;
				}
			}

			if (errorCode == UFTSESSION_ERROR_CODE_NETWORK_WOULD_BLOCK)
			{

				errorCode = UFTSESSION_ERROR_CODE_SUCCESS;
			}
		}

		return errorCode;
	}

private:
	UFTSESSION_ERROR_CODES SendFileList(const char* lpPath)
	{
		bool success = true;

		FileInfoList fileInfoList;

		if (GetFilesInPath(lpPath, fileInfoList, false) <= 0)
		{
			
			success = false;
		}

		// Send OPCodes::GetFileListResult
		{
			std::size_t getFileListResultCapacity = sizeof(bool);

			if (success)
			{
				getFileListResultCapacity += sizeof(std::uint32_t);

				for (auto& fileInfo : fileInfoList)
				{
					getFileListResultCapacity += sizeof(std::uint8_t) + (fileInfo.Path.Length * sizeof(char));
					getFileListResultCapacity += sizeof(std::uint64_t);
					getFileListResultCapacity += sizeof(std::uint32_t);
				}
			}

			UFTSession_CreatePacketBuffer(getFileListResult, OPCodes::GetFileListResult, getFileListResultCapacity);
			getFileListResult.Write(success);

			if (success)
			{
				getFileListResult.Write(
					std::uint32_t(fileInfoList.size())
				);

				for (auto& fileInfo : fileInfoList)
				{
					getFileListResult.Write(fileInfo.Path.Length);
					getFileListResult.Write(fileInfo.Path.Buffer, fileInfo.Path.Length);
					getFileListResult.Write(fileInfo.Size);
					getFileListResult.Write(fileInfo.Timestamp);
				}
			}

			if (UFTSession_SendPacketBuffer(getFileListResult) == 0)
			{

				return UFTSESSION_ERROR_CODE_NETWORK_CONNECTION_LOST;
			}
		}

		return UFTSESSION_ERROR_CODE_SUCCESS;
	}

	UFTSESSION_ERROR_CODES ReceiveFileList(FileInfoList& files, const char* lpPath)
	{
		// Send OPCodes::GetFileList
		{
			auto pathLength = strlen(
				lpPath
			);

			auto pathString8Size = sizeof(std::uint8_t) + (pathLength * sizeof(char));

			UFTSession_CreatePacketBuffer(getFileList, OPCodes::GetFileList, pathString8Size);
			getFileList.Write(std::uint8_t(pathLength));
			getFileList.Write(lpPath, pathLength);

			if (UFTSession_SendPacketBuffer(getFileList) == 0)
			{

				return UFTSESSION_ERROR_CODE_NETWORK_CONNECTION_LOST;
			}
		}

		// Receive OPCodes::GetFileListResult
		{
			UFTSESSION_ERROR_CODES errorCode;
			std::uint32_t          bytesReceived;
			ByteBuffer             getFileListResult;

			if ((errorCode = ReadPacket(OPCodes::GetFileListResult, getFileListResult, bytesReceived, true)) != UFTSESSION_ERROR_CODE_SUCCESS)
			{

				return errorCode;
			}

			bool getFileListResultSuccess;

			if (!getFileListResult.Read(getFileListResultSuccess))
			{
				Disconnect();

				return UFTSESSION_ERROR_CODE_NETWORK_API_ERROR;
			}

			// Populate files
			if (getFileListResultSuccess)
			{
				std::uint32_t getFileListResultFileInfoCount;

				if (!getFileListResult.Read(getFileListResultFileInfoCount))
				{
					Disconnect();

					return UFTSESSION_ERROR_CODE_NETWORK_API_ERROR;
				}

				FileInfo getFileListResultFileInfo;

				for (std::uint32_t i = 0; i < getFileListResultFileInfoCount; ++i)
				{
					if (!getFileListResult.Read(getFileListResultFileInfo.Path.Length) ||
						!getFileListResult.Read(getFileListResultFileInfo.Path.Buffer, getFileListResultFileInfo.Path.Length) ||
						!getFileListResult.Read(getFileListResultFileInfo.Size) ||
						!getFileListResult.Read(getFileListResultFileInfo.Timestamp))
					{
						Disconnect();

						return UFTSESSION_ERROR_CODE_NETWORK_API_ERROR;
					}

					getFileListResultFileInfo.Path.Buffer[getFileListResultFileInfo.Path.Length] = 0;

					files.push_back(
						std::move(getFileListResultFileInfo)
					);
				}
			}
			else
			{

				return UFTSESSION_ERROR_CODE_REMOTE_ERROR;
			}
		}

		return UFTSESSION_ERROR_CODE_SUCCESS;
	}

	template<typename F_ON_PROGRESS>
	UFTSESSION_ERROR_CODES TransmitFile(const char* lpSource, const char* lpDestination, TransmitFileDirections direction, F_ON_PROGRESS onProgress, void* lpParam)
	{
		FileInfo localFileInfo;
		FileInfo remoteFileInfo;

		switch (direction)
		{
			case TransmitFileDirections::Up:
			{
				if (GetFileInfo(lpSource, localFileInfo) <= 0)
				{

					return UFTSESSION_ERROR_CODE_FILESYSTEM_FILE_NOT_FOUND;
				}

				remoteFileInfo.Path.Assign(
					lpDestination
				);
			}
			break;

			case TransmitFileDirections::Down:
			{
				if (GetFileInfo(lpDestination, localFileInfo) <= 0)
				{

					localFileInfo.Path.Assign(
						lpDestination
					);
				}

				remoteFileInfo.Path.Assign(
					lpSource
				);
			}
			break;
		}

		// Send OPCodes::TransmitFile
		{
			UFTSession_CreatePacketBuffer(transmitFile, OPCodes::TransmitFile, sizeof(std::uint8_t) + (remoteFileInfo.Path.Length * sizeof(char)) + sizeof(std::uint64_t) + sizeof(std::uint32_t) + sizeof(TransmitFileDirections));
			transmitFile.Write(remoteFileInfo.Path.Length);
			transmitFile.Write(remoteFileInfo.Path.Buffer, remoteFileInfo.Path.Length);
			transmitFile.Write(localFileInfo.Size);
			transmitFile.Write(localFileInfo.Timestamp);
			transmitFile.Write(direction);

			if (UFTSession_SendPacketBuffer(transmitFile) == 0)
			{

				return UFTSESSION_ERROR_CODE_NETWORK_CONNECTION_LOST;
			}
		}

		// Receive OPCodes::TransmitFile
		{
			UFTSESSION_ERROR_CODES errorCode;
			ByteBuffer             transmitFile;
			std::uint32_t          bytesReceived;

			if ((errorCode = ReadPacket(OPCodes::TransmitFile, transmitFile, bytesReceived, true)) != UFTSESSION_ERROR_CODE_SUCCESS)
			{

				return errorCode;
			}

			if (!transmitFile.Read(remoteFileInfo.Path.Length) ||
				!transmitFile.Read(remoteFileInfo.Path.Buffer, remoteFileInfo.Path.Length) ||
				!transmitFile.Read(remoteFileInfo.Size) ||
				!transmitFile.Read(remoteFileInfo.Timestamp))
			{
				Disconnect();

				return UFTSESSION_ERROR_CODE_NETWORK_API_ERROR;
			}

			remoteFileInfo.Path.Buffer[remoteFileInfo.Path.Length] = 0;
		}

		switch (direction)
		{
			case TransmitFileDirections::Up:
				return SendFileChunks(localFileInfo, remoteFileInfo, std::move(onProgress), lpParam);

			case TransmitFileDirections::Down:
				return ReceiveFileChunks(localFileInfo, remoteFileInfo, std::move(onProgress), lpParam);
		}

		Disconnect();

		return UFTSESSION_ERROR_CODE_NETWORK_API_ERROR;
	}

	UFTSESSION_ERROR_CODES TransmitFile2(const FileInfo& remoteFileInfoLocalPath, TransmitFileDirections direction)
	{
		FileInfo localFileInfo;

		if (GetFileInfo(remoteFileInfoLocalPath.Path.Buffer, localFileInfo) <= 0)
		{
			switch (direction)
			{
				case TransmitFileDirections::Up:
					break;

				case TransmitFileDirections::Down:
				{
					Disconnect();

					// TODO: use proper error code here

					return UFTSESSION_ERROR_CODE_NETWORK_API_ERROR;
				}
				break;
			}
		}

		// Send OPCodes::TransmitFile
		{
			UFTSession_CreatePacketBuffer(transmitFile, OPCodes::TransmitFile, sizeof(std::uint8_t) + (localFileInfo.Path.Length * sizeof(char)) + sizeof(std::uint64_t) + sizeof(std::uint32_t) + sizeof(TransmitFileDirections));
			transmitFile.Write(localFileInfo.Path.Length);
			transmitFile.Write(localFileInfo.Path.Buffer, localFileInfo.Path.Length);
			transmitFile.Write(localFileInfo.Size);
			transmitFile.Write(localFileInfo.Timestamp);
			transmitFile.Write(direction);

			if (UFTSession_SendPacketBuffer(transmitFile) == 0)
			{

				return UFTSESSION_ERROR_CODE_NETWORK_CONNECTION_LOST;
			}
		}

		switch (direction)
		{
			case TransmitFileDirections::Up:
				return ReceiveFileChunks(localFileInfo, remoteFileInfoLocalPath, nullptr, nullptr);

			case TransmitFileDirections::Down:
				return SendFileChunks(localFileInfo, remoteFileInfoLocalPath, nullptr, nullptr);
		}

		Disconnect();

		return UFTSESSION_ERROR_CODE_NETWORK_API_ERROR;
	}

	template<typename F_ON_PROGRESS>
	UFTSESSION_ERROR_CODES SendFileChunks(const FileInfo& localFileInfo, const FileInfo& remoteFileInfo, F_ON_PROGRESS onProgress, void* lpParam)
	{
		UFTSESSION_ERROR_CODES errorCode;

		// Check if remote file does not exist or remote is larger than local - transmit file
		if (true || ((remoteFileInfo.Size == 0) && (remoteFileInfo.Timestamp == 0)) || (remoteFileInfo.Size > localFileInfo.Size))
		{
			std::ifstream fStream(
				localFileInfo.Path.Buffer,
				std::ios::binary
			);

			if (!fStream.is_open())
			{

				return UFTSESSION_ERROR_CODE_FILESYSTEM_OPEN_STREAM_FAILED;
			}

			FileChunkBuffer fileChunkBuffer(
				static_cast<std::size_t>(FILE_CHUNK_SIZE)
			);

			FileChunkBuffer compressedFileChunkBuffer(
				static_cast<std::size_t>(FILE_CHUNK_SIZE * 2)
			);

			for (std::uint64_t fileOffset = 0; fileOffset < localFileInfo.Size; )
			{
				fStream.seekg(
					static_cast<std::streampos>(fileOffset)
				);

				fStream.read(
					reinterpret_cast<char*>(&fileChunkBuffer[0]),
					fileChunkBuffer.size()
				);

				std::uint64_t fileChunkBufferSize = fStream.gcount();

				if ((errorCode = SendFileChunk(compressedFileChunkBuffer, fileChunkBuffer, fileOffset, fileChunkBufferSize)) != UFTSESSION_ERROR_CODE_SUCCESS)
				{

					return errorCode;
				}

				fileOffset += fileChunkBufferSize;

				if constexpr (!std::is_same<F_ON_PROGRESS, std::nullptr_t>::value)
				{

					onProgress(
						fileOffset,
						localFileInfo.Size,
						lpParam
					);
				}
			}
		}

		// Check if local file is larger or equal to remote - compare and transmit as needed
		else if (localFileInfo.Size >= remoteFileInfo.Size)
		{
			std::ifstream fStream(
				localFileInfo.Path.Buffer,
				std::ios::binary
			);

			if (!fStream.is_open())
			{

				return UFTSESSION_ERROR_CODE_FILESYSTEM_OPEN_STREAM_FAILED;
			}

			FileChunkBuffer fileChunkBuffer(
				static_cast<std::size_t>(FILE_CHUNK_SIZE)
			);

			FileChunkBuffer compressedFileChunkBuffer(
				static_cast<std::size_t>(FILE_CHUNK_SIZE * 2)
			);

			FileChunkHash localFileChunkHash;
			FileChunkHash remoteFileChunkHash;
			std::uint64_t remoteFileChunkSize;
			std::uint64_t remoteFileChunkOffset;

			for (std::uint64_t fileOffset = 0; fileOffset < localFileInfo.Size; )
			{
				fStream.seekg(
					static_cast<std::streampos>(fileOffset)
				);

				fStream.read(
					reinterpret_cast<char*>(&fileChunkBuffer[0]),
					fileChunkBuffer.size()
				);

				std::uint64_t fileChunkBufferSize = fStream.gcount();

				if ((errorCode = SendFileChunkHash(localFileChunkHash, fileChunkBuffer, fileOffset, fileChunkBufferSize)) != UFTSESSION_ERROR_CODE_SUCCESS)
				{

					return errorCode;
				}

				if ((errorCode = ReceiveFileChunkHash(remoteFileChunkHash, remoteFileChunkOffset, remoteFileChunkSize)) != UFTSESSION_ERROR_CODE_SUCCESS)
				{

					return errorCode;
				}

				if (fileOffset != remoteFileChunkOffset)
				{
					Disconnect();

					return UFTSESSION_ERROR_CODE_NETWORK_API_ERROR;
				}

				if (localFileChunkHash != remoteFileChunkHash)
				{
					if ((errorCode = SendFileChunk(compressedFileChunkBuffer, fileChunkBuffer, fileOffset, fileChunkBufferSize)) != UFTSESSION_ERROR_CODE_SUCCESS)
					{

						return errorCode;
					}
				}

				fileOffset += fileChunkBufferSize;

				if constexpr (!std::is_same<F_ON_PROGRESS, std::nullptr_t>::value)
				{

					onProgress(
						fileOffset,
						localFileInfo.Size,
						lpParam
					);
				}
			}
		}

		return UFTSESSION_ERROR_CODE_SUCCESS;
	}

	template<typename F_ON_PROGRESS>
	UFTSESSION_ERROR_CODES ReceiveFileChunks(const FileInfo& localFileInfo, const FileInfo& remoteFileInfo, F_ON_PROGRESS onProgress, void* lpParam)
	{
		UFTSESSION_ERROR_CODES errorCode;

		// Check if local file does not exist or local is larger than remote - receive file
		if (true || ((localFileInfo.Size == 0) && (localFileInfo.Timestamp == 0)) || (localFileInfo.Size > remoteFileInfo.Size))
		{
			std::ofstream fStream(
				localFileInfo.Path.Buffer,
				std::ios::binary | std::ios::trunc
			);

			if (!fStream.is_open())
			{

				return UFTSESSION_ERROR_CODE_FILESYSTEM_OPEN_STREAM_FAILED;
			}

			FileChunkBuffer fileChunkBuffer(
				static_cast<std::size_t>(FILE_CHUNK_SIZE)
			);

			FileChunkBuffer compressedFileChunkBuffer(
				static_cast<std::size_t>(FILE_CHUNK_SIZE * 2)
			);

			std::uint64_t fileChunkBufferSize;
			std::uint64_t fileChunkBufferOffset;

			auto onReceiveFileChunk = [&fStream](const FileChunkBuffer& _buffer, std::uint64_t _offset, std::uint64_t _size)
			{
				// TODO: compare offset

				fStream.seekp(
					static_cast<std::streampos>(_offset)
				);

				fStream.write(
					reinterpret_cast<const char*>(&_buffer[0]),
					static_cast<std::streamsize>(_size)
				);

				return true;
			};

			for (std::uint64_t fileOffset = 0; fileOffset < remoteFileInfo.Size; )
			{
				if ((errorCode = ReceiveFileChunk(compressedFileChunkBuffer, fileChunkBuffer, fileChunkBufferOffset, fileChunkBufferSize, onReceiveFileChunk)) != UFTSESSION_ERROR_CODE_SUCCESS)
				{

					return errorCode;
				}

				fileOffset += fileChunkBufferSize;

				if constexpr (!std::is_same<F_ON_PROGRESS, std::nullptr_t>::value)
				{

					onProgress(
						fileOffset,
						remoteFileInfo.Size,
						lpParam
					);
				}
			}
		}

		// Check if local file is smaller than or equal to remote - compare and receive as needed
		else if (localFileInfo.Size <= remoteFileInfo.Size)
		{
			std::fstream fStream(
				localFileInfo.Path.Buffer,
				std::ios::binary | std::ios::in | std::ios::out | std::ios::ate
			);

			if (!fStream.is_open())
			{

				return UFTSESSION_ERROR_CODE_FILESYSTEM_OPEN_STREAM_FAILED;
			}

			FileChunkBuffer fileChunkBuffer(
				static_cast<std::size_t>(FILE_CHUNK_SIZE)
			);

			FileChunkBuffer compressedFileChunkBuffer(
				static_cast<std::size_t>(FILE_CHUNK_SIZE * 2)
			);

			FileChunkHash localFileChunkHash;
			FileChunkHash remoteFileChunkHash;
			std::uint64_t remoteFileChunkSize;
			std::uint64_t remoteFileChunkOffset;

			auto onReceiveFileChunk = [&fStream](const FileChunkBuffer& _buffer, std::uint64_t _offset, std::uint64_t _size)
			{
				// TODO: compare offset

				fStream.seekp(
					static_cast<std::streampos>(_offset)
				);

				fStream.write(
					reinterpret_cast<const char*>(&_buffer[0]),
					static_cast<std::streamsize>(_size)
				);

				return true;
			};

			for (std::uint64_t fileOffset = 0; fileOffset < remoteFileInfo.Size; )
			{
				fStream.seekg(
					static_cast<std::streampos>(fileOffset)
				);

				fStream.read(
					reinterpret_cast<char*>(&fileChunkBuffer[0]),
					fileChunkBuffer.size()
				);

				std::uint64_t fileChunkBufferSize = fStream.gcount();

				if ((errorCode = ReceiveFileChunkHash(remoteFileChunkHash, remoteFileChunkOffset, remoteFileChunkSize)) != UFTSESSION_ERROR_CODE_SUCCESS)
				{

					return errorCode;
				}

				if ((errorCode = SendFileChunkHash(localFileChunkHash, fileChunkBuffer, fileOffset, fileChunkBufferSize)) != UFTSESSION_ERROR_CODE_SUCCESS)
				{

					return errorCode;
				}

				if (fileOffset != remoteFileChunkOffset)
				{
					Disconnect();

					return UFTSESSION_ERROR_CODE_NETWORK_API_ERROR;
				}

				if (localFileChunkHash != remoteFileChunkHash)
				{
					if ((errorCode = ReceiveFileChunk(compressedFileChunkBuffer, fileChunkBuffer, fileOffset, fileChunkBufferSize, onReceiveFileChunk)) != UFTSESSION_ERROR_CODE_SUCCESS)
					{

						return errorCode;
					}
				}

				fileOffset += fileChunkBufferSize;

				if constexpr (!std::is_same<F_ON_PROGRESS, std::nullptr_t>::value)
				{

					onProgress(
						fileOffset,
						remoteFileInfo.Size,
						lpParam
					);
				}
			}
		}

		return UFTSESSION_ERROR_CODE_SUCCESS;
	}

	UFTSESSION_ERROR_CODES SendFileChunk(FileChunkBuffer& buffer, const FileChunkBuffer& source, std::uint64_t offset, std::uint64_t size)
	{
		std::uint64_t compressedSize = CompressFileChunk(
			buffer,
			source,
			size
		);

		// Send OPCodes::TransmitFileChunk
		{
			UFTSession_CreatePacketBuffer(transmitFileChunk, OPCodes::TransmitFileChunk, sizeof(std::uint64_t) + sizeof(std::uint64_t) + sizeof(std::uint64_t) + static_cast<std::size_t>(compressedSize));
			transmitFileChunk.Write(offset);
			transmitFileChunk.Write(size);
			transmitFileChunk.Write(compressedSize);
			transmitFileChunk.Write(&buffer[0], static_cast<std::size_t>(compressedSize));

			if (UFTSession_SendPacketBuffer(transmitFileChunk) == 0)
			{

				return UFTSESSION_ERROR_CODE_NETWORK_CONNECTION_LOST;
			}
		}

		// Receive OPCodes::TransmitFileChunkResult
		{
			UFTSESSION_ERROR_CODES errorCode;
			std::uint32_t          bytesReceived;
			ByteBuffer             transmitFileChunkResult;

			if ((errorCode = ReadPacket(OPCodes::TransmitFileChunkResult, transmitFileChunkResult, bytesReceived, true)) != UFTSESSION_ERROR_CODE_SUCCESS)
			{

				return errorCode;
			}

			bool success;

			if (!transmitFileChunkResult.Read(success))
			{
				Disconnect();

				return UFTSESSION_ERROR_CODE_NETWORK_API_ERROR;
			}

			if (!success)
			{

				return UFTSESSION_ERROR_CODE_REMOTE_ERROR;
			}
		}

		return UFTSESSION_ERROR_CODE_SUCCESS;
	}

	// F = bool(*)(const FileChunkBuffer& buffer, std::uint64_t offset, std::uint64_t size)
	template<typename F>
	UFTSESSION_ERROR_CODES ReceiveFileChunk(FileChunkBuffer& buffer, FileChunkBuffer& destination, std::uint64_t& offset, std::uint64_t& size, F&& callback)
	{
		// Receive OPCodes::TransmitFileChunk
		{
			UFTSESSION_ERROR_CODES errorCode;
			std::uint32_t          bytesReceived;
			ByteBuffer             transmitFileChunk;

			if ((errorCode = ReadPacket(OPCodes::TransmitFileChunk, transmitFileChunk, bytesReceived, true)) != UFTSESSION_ERROR_CODE_SUCCESS)
			{

				return errorCode;
			}

			std::uint64_t compressedSize;

			if (!transmitFileChunk.Read(offset) ||
				!transmitFileChunk.Read(size) ||
				!transmitFileChunk.Read(compressedSize) ||
				!transmitFileChunk.Read(&buffer[0], static_cast<std::size_t>(compressedSize)))
			{
				Disconnect();

				return UFTSESSION_ERROR_CODE_NETWORK_API_ERROR;
			}

			/*auto decompressedSize = */DecompressFileChunk(
				destination,
				buffer,
				compressedSize
			);
		}
		
		bool success = callback(
			destination,
			offset,
			size
		);
		
		// Send OPCodes::TransmitFileChunkResult
		{
			UFTSession_CreatePacketBuffer(transmitFileChunkResult, OPCodes::TransmitFileChunkResult, sizeof(bool));
			transmitFileChunkResult.Write(success);

			if (UFTSession_SendPacketBuffer(transmitFileChunkResult) == 0)
			{

				return UFTSESSION_ERROR_CODE_NETWORK_CONNECTION_LOST;
			}
		}

		return UFTSESSION_ERROR_CODE_SUCCESS;
	}

	UFTSESSION_ERROR_CODES SendFileChunkHash(FileChunkHash& hash, const FileChunkBuffer& buffer, std::uint64_t offset, std::uint64_t size)
	{
		hash = CalculateFileChunkHash(
			buffer,
			size
		);

		// Send OPCodes::TransmitFileChunkHash
		{
			UFTSession_CreatePacketBuffer(transmitFileChunkHash, OPCodes::TransmitFileHash, sizeof(std::uint64_t) + sizeof(std::uint64_t) + sizeof(FileChunkHash));
			transmitFileChunkHash.Write(offset);
			transmitFileChunkHash.Write(size);
			transmitFileChunkHash.Write(hash);

			if (UFTSession_SendPacketBuffer(transmitFileChunkHash) == 0)
			{

				return UFTSESSION_ERROR_CODE_NETWORK_CONNECTION_LOST;
			}
		}

		return UFTSESSION_ERROR_CODE_SUCCESS;
	}

	UFTSESSION_ERROR_CODES ReceiveFileChunkHash(FileChunkHash& hash, std::uint64_t& offset, std::uint64_t& size)
	{
		// Receive OPCodes::TransmitFileChunkHash
		{
			UFTSESSION_ERROR_CODES errorCode;
			std::uint32_t          bytesReceived;
			ByteBuffer             transmitFileChunkHash;

			if ((errorCode = ReadPacket(OPCodes::TransmitFileHash, transmitFileChunkHash, bytesReceived, true)) != UFTSESSION_ERROR_CODE_SUCCESS)
			{

				return errorCode;
			}

			if (!transmitFileChunkHash.Read(offset) ||
				!transmitFileChunkHash.Read(size) ||
				!transmitFileChunkHash.Read(hash))
			{
				Disconnect();

				return UFTSESSION_ERROR_CODE_NETWORK_API_ERROR;
			}
		}

		return UFTSESSION_ERROR_CODE_SUCCESS;
	}

	UFTSESSION_ERROR_CODES HandlePacket(const PacketHeader& header, ByteBuffer& buffer)
	{
		switch (header.OPCode)
		{
			case OPCodes::GetFileList:
			{
				String8 path;

				if (!buffer.Read(path.Length) ||
					!buffer.Read(path.Buffer, path.Length))
				{
					Disconnect();

					return UFTSESSION_ERROR_CODE_NETWORK_API_ERROR;
				}

				path.Buffer[path.Length] = 0;

				UFTSESSION_ERROR_CODES errorCode;

				if ((errorCode = SendFileList(path.Buffer)) != UFTSESSION_ERROR_CODE_SUCCESS)
				{

					return errorCode;
				}
			}
			return UFTSESSION_ERROR_CODE_SUCCESS;

			case OPCodes::GetFileListResult:
				break;

			case OPCodes::TransmitFile:
			{
				FileInfo file;
				TransmitFileDirections direction;

				if (!buffer.Read(file.Path.Length) ||
					!buffer.Read(file.Path.Buffer, file.Path.Length) ||
					!buffer.Read(file.Size) ||
					!buffer.Read(file.Timestamp) ||
					!buffer.Read(direction))
				{
					Disconnect();

					return UFTSESSION_ERROR_CODE_NETWORK_API_ERROR;
				}

				file.Path.Buffer[file.Path.Length] = 0;

				UFTSESSION_ERROR_CODES errorCode;

				if ((errorCode = TransmitFile2(file, direction)) != UFTSESSION_ERROR_CODE_SUCCESS)
				{

					return errorCode;
				}
			}
			return UFTSESSION_ERROR_CODE_SUCCESS;
			
			case OPCodes::TransmitFileHash:
			case OPCodes::TransmitFileChunk:
			case OPCodes::TransmitFileChunkResult:
				break;
		}

		return UFTSESSION_ERROR_CODE_NETWORK_API_ERROR;
	}

	UFTSESSION_ERROR_CODES ReadPacket(OPCodes opcode, ByteBuffer& buffer, std::uint32_t& bytesReceived, bool block)
	{
		UFTSESSION_ERROR_CODES errorCode;

		PacketHeader packetHeader;
		
		if ((errorCode = ReadNextPacket(packetHeader, buffer, bytesReceived, block)) == UFTSESSION_ERROR_CODE_SUCCESS)
		{
			if (packetHeader.OPCode != opcode)
			{
				Disconnect();

				return UFTSESSION_ERROR_CODE_NETWORK_API_ERROR;
			}

			return UFTSESSION_ERROR_CODE_SUCCESS;
		}

		return errorCode;
	}

	UFTSESSION_ERROR_CODES ReadNextPacket(PacketHeader& header, ByteBuffer& buffer, std::uint32_t& bytesReceived, bool block)
	{
		std::int32_t _bytesReceived;

		if (!block)
		{
			if ((_bytesReceived = GetSocket().TryReceiveAll(&header, sizeof(PacketHeader))) <= 0)
			{
				switch (_bytesReceived)
				{
					case 0:  return UFTSESSION_ERROR_CODE_NETWORK_CONNECTION_LOST;
					case -1: return UFTSESSION_ERROR_CODE_NETWORK_WOULD_BLOCK;
				}
			}
		}
		else
		{
			if ((_bytesReceived = GetSocket().ReceiveAll(&header, sizeof(PacketHeader))) == 0)
			{

				return UFTSESSION_ERROR_CODE_NETWORK_CONNECTION_LOST;
			}
		}

		header.OPCode = BitConverter::NetworkToHost(
			header.OPCode
		);
		header.PayloadSize = BitConverter::NetworkToHost(
			header.PayloadSize
		);

		switch (header.OPCode)
		{
			case OPCodes::GetFileList:
			case OPCodes::GetFileListResult:
			case OPCodes::TransmitFile:
			case OPCodes::TransmitFileHash:
			case OPCodes::TransmitFileChunk:
			case OPCodes::TransmitFileChunkResult:
			{
				buffer = ByteBuffer(
					static_cast<std::size_t>(header.PayloadSize)
				);

				if (header.PayloadSize && ((_bytesReceived = GetSocket().ReceiveAll(buffer.GetBuffer(), buffer.GetCapacity())) == 0))
				{

					return UFTSESSION_ERROR_CODE_NETWORK_CONNECTION_LOST;
				}

				buffer.SetOffsetW(
					_bytesReceived
				);
			}
			break;

			default:
				Disconnect();
				return UFTSESSION_ERROR_CODE_NETWORK_API_ERROR;
		}

		bytesReceived = static_cast<std::uint32_t>(
			_bytesReceived
		);

		return UFTSESSION_ERROR_CODE_SUCCESS;
	}

	// @return 0 on error
	// @return -1 if not found
	static int GetFileInfo(const char* lpPath, FileInfo& info, bool setInfoPath = true)
	{
#if defined(WIN32) || defined(_WIN32)
		struct _stat64 stat;
#else
		struct stat64 stat;
#endif

#if defined(WIN32) || defined(_WIN32)
		if (_stat64(lpPath, &stat) == -1)
#else
		if (stat64(lpPath, &stat) == -1)
#endif
		{
			auto lastError = errno;

			if ((lastError != ENOENT) && (lastError != ENOTDIR))
			{
#if defined(WIN32) || defined(_WIN32)
//				WriteLastErrorOS("_stat64");
#else
//				WriteLastErrorOS("stat64");
#endif

				return 0;
			}

			if (setInfoPath)
			{
				info.Path.Length = 0;
				info.Path.Buffer[0] = 0;
			}
			
			info.Size = 0;
			info.Timestamp = 0;

			return -1;
		}

		if (setInfoPath)
		{

			info.Path.Assign(
				lpPath
			);
		}

		info.Size = static_cast<std::uint64_t>(stat.st_size);
		info.Timestamp = static_cast<std::uint32_t>(stat.st_mtime);

		return 1;
	}

	// @return 0 on error
	// @return -1 if not found
	static int GetFilesInPath(const char* path, FileInfoList& files, bool includePathInFileInfo)
	{
		FileInfo fileInfo;

#if !defined(WIN32)
		DIR* lpDIR;

		if ((lpDIR = opendir(path)) == NULL)
		{

			return -1;
		}

		dirent* lpEntry;

		while ((lpEntry = readdir(lpDIR)) != NULL)
		{
			if (lpEntry->d_type == DT_DIR)
			{

				continue;
			}

			std::stringstream fileEntryPath;
			fileEntryPath << path << '/' << lpEntry->d_name;

			if (GetFileInfo(fileEntryPath.str().c_str(), fileInfo, !includePathInFileInfo) <= 0)
			{

				continue;
			}

			if (!includePathInFileInfo)
			{

				fileInfo.Path.Assign(
					lpEntry->d_name
				);
			}

			files.push_back(
				std::move(fileInfo)
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

		do
		{
			if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			{
				std::stringstream fileEntryPath;
				fileEntryPath << path << '/' << fd.cFileName;

				if (GetFileInfo(fileEntryPath.str().c_str(), fileInfo, !includePathInFileInfo) <= 0)
				{

					continue;
				}

				if (!includePathInFileInfo)
				{

					fileInfo.Path.Assign(
						fd.cFileName
					);
				}

				files.push_back(
					std::move(fileInfo)
				);
			}
		} while (FindNextFile(hFind, &fd));

		FindClose(hFind);
#endif

		return 1;
	}

	static FileChunkHash CalculateFileChunkHash(const FileChunkBuffer& buffer, std::uint64_t size)
	{
		static constexpr FileChunkHash FNV_1a_64_PRIME = 0x100000001B3;
		static constexpr FileChunkHash FNV_1a_64_OFFSET = 0xCBF29CE484222325;

		FileChunkHash hash = FNV_1a_64_OFFSET;

		auto lpBuffer = reinterpret_cast<const std::uint8_t*>(
			&buffer[0]
		);

		for (std::uint64_t i = 0; i < size; ++i, ++lpBuffer)
		{
			hash ^= static_cast<FileChunkHash>(
				*lpBuffer
			);

			hash *= FNV_1a_64_PRIME;
		}

		return hash;
	}

	// @return compressed chunk size
	static std::uint64_t CompressFileChunk(FileChunkBuffer& buffer, const FileChunkBuffer& source, std::uint64_t size)
	{
		z_stream stream = { 0 };
		deflateInit(&stream, FILE_COMPRESSION_LEVEL);

		stream.next_in = const_cast<Bytef*>(
			reinterpret_cast<const Bytef*>(
				&source[0]
			)
		);
		stream.next_out = reinterpret_cast<Bytef*>(
			&buffer[0]
		);
		stream.avail_in = static_cast<uInt>(
			size
		);
		stream.avail_out = static_cast<uInt>(
			buffer.size()
		);

		deflate(&stream, Z_FINISH);

		auto deflatedSize = stream.total_out;

		deflateEnd(&stream);

		return deflatedSize;
	}

	// @return deflated chunk size
	static std::uint64_t DecompressFileChunk(FileChunkBuffer& buffer, const FileChunkBuffer& source, std::uint64_t size)
	{
		z_stream stream = { 0 };
		inflateInit(&stream);

		stream.next_in = reinterpret_cast<Bytef*>(
			const_cast<std::uint8_t*>(&source[0])
		);
		stream.next_out = reinterpret_cast<Bytef*>(
			&buffer[0]
		);
		stream.avail_in = static_cast<uInt>(
			size
		);
		stream.avail_out = static_cast<uInt>(
			buffer.size()
		);

		inflate(&stream, Z_FINISH);

		auto inflatedSize = stream.total_out;

		inflateEnd(&stream);

		return inflatedSize;
	}
};

#undef UFTSession_InitPacketBuffer
#undef UFTSession_CreatePacketBuffer
#undef UFTSession_SendPacketBuffer

#endif
