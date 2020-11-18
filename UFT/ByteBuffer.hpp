// -----------------------------------------------------------------------------
// Written by: F. Barney
// Date: 10/13/2020
// -----------------------------------------------------------------------------

#ifndef BYTEBUFFER_HPP
#define BYTEBUFFER_HPP

#include "BitConverter.hpp"

#include <limits>
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <type_traits>

class ByteBuffer
{
	template<typename T>
	struct is_enum
	{
		static constexpr bool Value = std::is_enum<T>::value;
	};

	template<typename T>
	struct is_integer_or_decimal
	{
		static constexpr bool Value = std::numeric_limits<T>::is_integer || std::is_floating_point<T>::value;
	};

	std::vector<std::uint8_t> buffer;

	std::size_t offset_r = 0;
	std::size_t offset_w = 0;

public:
	ByteBuffer()
		: buffer(
			0
		)
	{
	}

	explicit ByteBuffer(std::size_t capacity)
		: buffer(
			capacity
		)
	{
	}

	ByteBuffer(const void* lpBuffer, std::size_t bufferSize)
		: buffer(
			bufferSize
		),
		offset_w(
			bufferSize
		)
	{
		std::memcpy(
			&buffer[0],
			lpBuffer,
			bufferSize
		);
	}

	void* GetBuffer()
	{
		return &buffer[0];
	}
	const void* GetBuffer() const
	{
		return &buffer[0];
	}

	std::size_t GetSize() const
	{
		return offset_w;
	}

	std::size_t GetCapacity() const
	{
		return buffer.size();
	}

	void SetOffsetR(std::size_t value)
	{
		if (value > GetSize())
		{

			value = GetSize();
		}

		offset_r = value;
	}

	void SetOffsetW(std::size_t value)
	{
		if (value > GetCapacity())
		{

			value = GetCapacity();
		}

		offset_w = value;
	}

	bool Read(bool& value)
	{
		return Read(
			&value,
			sizeof(bool)
		);
	}
	template<typename T>
	bool Read(std::string& value)
	{
		T valueLength;

		if (!Read<T>(valueLength))
		{

			return false;
		}

		if ((offset_r + (valueLength * sizeof(typename std::string::value_type))) > GetSize())
		{
			offset_r -= sizeof(T);

			return false;
		}

		value.resize(valueLength);
		std::memcpy(&value[0], &buffer[offset_r], valueLength);
		offset_r += valueLength * sizeof(typename std::string::value_type);

		return true;
	}
	template<typename T>
	typename std::enable_if<is_enum<T>::Value || is_integer_or_decimal<T>::Value, bool>::type Read(T& value)
	{
		if ((offset_r + sizeof(T)) > GetSize())
		{

			return false;
		}

		value = BitConverter::NetworkToHost<T>(
			*reinterpret_cast<T*>(&buffer[offset_r])
		);

		offset_r += sizeof(T);

		return true;
	}
	bool Read(void* lpBuffer, std::size_t size)
	{
		if ((offset_r + size) > GetSize())
		{

			return false;
		}

		std::memcpy(
			lpBuffer,
			&buffer[offset_r],
			size
		);

		offset_r += size;

		return true;
	}

	bool Write(bool value)
	{
		return Write(
			&value,
			sizeof(bool)
		);
	}
	template<typename T>
	bool Write(const std::string& value)
	{
		auto valueLength = static_cast<T>(
			value.length()
		);

		if ((offset_w + sizeof(T) + (valueLength * sizeof(typename std::string::value_type))) > GetCapacity())
		{

			return false;
		}

		Write<T>(valueLength);

		std::memcpy(&buffer[offset_w], value.c_str(), valueLength);
		offset_w += sizeof(typename std::string::value_type) * valueLength;

		return true;
	}
	template<typename T>
	typename std::enable_if<is_enum<T>::Value || is_integer_or_decimal<T>::Value, bool>::type Write(T value)
	{
		if ((offset_w + sizeof(T)) > GetCapacity())
		{

			return false;
		}

		*reinterpret_cast<T*>(&buffer[offset_w]) = BitConverter::HostToNetwork<T>(
			value
		);

		offset_w += sizeof(T);

		return true;
	}
	bool Write(const void* lpBuffer, std::size_t size)
	{
		if ((offset_w + size) > GetCapacity())
		{

			return false;
		}

		std::memcpy(
			&buffer[offset_w],
			lpBuffer,
			size
		);

		offset_w += size;

		return true;
	}
};

#endif // !BYTEBUFFER_HPP
