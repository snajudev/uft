// -----------------------------------------------------------------------------
// Written by: F. Barney
// Date: 7/23/2020
// -----------------------------------------------------------------------------

#ifndef BITCONVERTER_HPP
#define BITCONVERTER_HPP

#include <cstdint>
#include <type_traits>

class BitConverter
{
	template<typename T>
	struct Is_Enum_Or_Integer
	{
		static constexpr bool Value = std::is_enum<T>::value || std::is_integral<T>::value;
	};

	template<typename T, std::size_t SIZE = sizeof(T)>
	struct Get_Enum_Or_Integer_Base;
	template<typename T>
	struct Get_Enum_Or_Integer_Base<T, 1>
	{
		typedef typename std::conditional<std::is_signed<T>::value, std::int8_t, std::uint8_t>::type Type;
	};
	template<typename T>
	struct Get_Enum_Or_Integer_Base<T, 2>
	{
		typedef typename std::conditional<std::is_signed<T>::value, std::int16_t, std::uint16_t>::type Type;
	};
	template<typename T>
	struct Get_Enum_Or_Integer_Base<T, 4>
	{
		typedef typename std::conditional<std::is_signed<T>::value, std::int32_t, std::uint32_t>::type Type;
	};
	template<typename T>
	struct Get_Enum_Or_Integer_Base<T, 8>
	{
		typedef typename std::conditional<std::is_signed<T>::value, std::int64_t, std::uint64_t>::type Type;
	};

	template<typename T, std::size_t BIT_COUNT = sizeof(T) * 8, bool IS_ENUM_OR_INTEGER = Is_Enum_Or_Integer<T>::Value>
	struct Flip_The_Bytes;
	template<typename T, bool IS_ENUM_OR_INTEGER>
	struct Flip_The_Bytes<T, 8, IS_ENUM_OR_INTEGER>
	{
		static constexpr T Flip(T value)
		{
			return value;
		}
	};
	template<typename T>
	struct Flip_The_Bytes<T, 16, true>
	{
		static constexpr T Flip(T value)
		{
			typedef typename Get_Enum_Or_Integer_Base<T>::Type Base;

			return static_cast<T>(
				((static_cast<Base>(value) & 0x00FF) << 8) |
				((static_cast<Base>(value) & 0xFF00) >> 8)
			);
		}
	};
	template<typename T>
	struct Flip_The_Bytes<T, 32, true>
	{
		static constexpr T Flip(T value)
		{
			typedef typename Get_Enum_Or_Integer_Base<T>::Type Base;

			return static_cast<T>(
				((static_cast<Base>(value) & 0xFF000000) >> 24) |
				((static_cast<Base>(value) & 0x00FF0000) >> 8) |
				((static_cast<Base>(value) & 0x0000FF00) << 8) |
				((static_cast<Base>(value) & 0x000000FF) << 24)
			);
		}
	};
	template<typename T>
	struct Flip_The_Bytes<T, 64, true>
	{
		static constexpr T Flip(T value)
		{
			typedef typename Get_Enum_Or_Integer_Base<T>::Type Base;

			return static_cast<T>(
				((static_cast<Base>(value) & 0xFF00000000000000) >> 56) |
				((static_cast<Base>(value) & 0x00FF000000000000) >> 40) |
				((static_cast<Base>(value) & 0x0000FF0000000000) >> 24) |
				((static_cast<Base>(value) & 0x000000FF00000000) >> 8) |
				((static_cast<Base>(value) & 0x00000000FF000000) << 8) |
				((static_cast<Base>(value) & 0x0000000000FF0000) << 24) |
				((static_cast<Base>(value) & 0x000000000000FF00) << 40) |
				((static_cast<Base>(value) & 0x00000000000000FF) << 56)
			);
		}
	};

	BitConverter() = delete;

public:
	static constexpr bool IsLittleEndian()
	{
		return (0x00000001 & 0xFFFFFFFF) == 0x00000001;
	}

	template<typename T>
	static constexpr T HostToNetwork(T value)
	{
		return IsLittleEndian() ? Flip_The_Bytes<T>::Flip(value) : value;
	}

	template<typename T>
	static constexpr T NetworkToHost(T value)
	{
		return IsLittleEndian() ? Flip_The_Bytes<T>::Flip(value) : value;
	}
};

#endif // !BITCONVERTER_HPP
