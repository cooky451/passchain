#pragma once

#include <cstdint>
#include <array>

namespace base64
{
	namespace detail
	{
		static constexpr std::array<std::uint8_t, 64> BITS_TO_ASCII =
		{
			'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 
			'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 
			'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 
			'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 
			'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 
			'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 
			'w', 'x', 'y', 'z', '0', '1', '2', '3', 
			'4', '5', '6', '7', '8', '9', '+', '/', 
		};

		static constexpr std::array<std::uint8_t, 256> ASCII_TO_BITS =
		{
			0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
			0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
			0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x3e, 0x40, 0x40, 0x40, 0x3f,
			0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
			0x40, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
			0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x40, 0x40, 0x40, 0x40, 0x40,
			0x40, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
			0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x40, 0x40, 0x40, 0x40, 0x40,
			0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
			0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
			0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
			0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
			0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
			0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
			0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
			0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 
		};
	}

	constexpr std::size_t encodedLength(std::size_t bytes)
	{
		return (bytes + 2) / 3 * 4;
	}

	inline std::size_t decodedLength(const void* base64String)
	{
		auto ptr = static_cast<const std::uint8_t*>(base64String);

		for (std::size_t i = 0;; ++i)
		{
			if (detail::ASCII_TO_BITS[ptr[i]] >= 0x40)
			{
				return (i * 3 + 1) / 4;
			}
		}
	}

	inline void encode(void* buffer, const void* source, std::size_t sourceSize)
	{
		auto buf_ptr = static_cast<std::uint8_t*>(buffer);
		auto src_ptr = static_cast<const std::uint8_t*>(source);

		for (; sourceSize > 2; sourceSize -= 3, buf_ptr += 4, src_ptr += 3)
		{
			buf_ptr[0] = detail::BITS_TO_ASCII[((src_ptr[0] & 0xFC) >> 2) | 0x00];
			buf_ptr[1] = detail::BITS_TO_ASCII[((src_ptr[0] & 0x03) << 4) | (src_ptr[1] >> 4)];
			buf_ptr[2] = detail::BITS_TO_ASCII[((src_ptr[1] & 0x0F) << 2) | (src_ptr[2] >> 6)];
			buf_ptr[3] = detail::BITS_TO_ASCII[((src_ptr[2] & 0x3F) << 0) | 0x00];
		}

		if (sourceSize > 0)
		{
			*buf_ptr++ = detail::BITS_TO_ASCII[(src_ptr[0] >> 2) & 0x3F];

			if (sourceSize == 1)
			{
				*buf_ptr++ = detail::BITS_TO_ASCII[((src_ptr[0] & 0x3) << 4)];
				*buf_ptr++ = '=';
			}
			else
			{
				*buf_ptr++ = detail::BITS_TO_ASCII[((src_ptr[0] & 0x3) << 4) | ((int)(src_ptr[1] & 0xF0) >> 4)];
				*buf_ptr++ = detail::BITS_TO_ASCII[((src_ptr[1] & 0xF) << 2)];
			}

			*buf_ptr++ = '=';
		}
	}

	inline void decode(void* buffer, const void* source, std::size_t size)
	{
		auto buf_ptr = static_cast<std::uint8_t*>(buffer);
		auto src_ptr = static_cast<const std::uint8_t*>(source);

		size = (size * 4 + 2) / 3;

		for (; size >= 4; size -= 4, buf_ptr += 3, src_ptr += 4)
		{
			buf_ptr[0] = static_cast<std::uint8_t>(detail::ASCII_TO_BITS[src_ptr[0]] << 2 | detail::ASCII_TO_BITS[src_ptr[1]] >> 4);
			buf_ptr[1] = static_cast<std::uint8_t>(detail::ASCII_TO_BITS[src_ptr[1]] << 4 | detail::ASCII_TO_BITS[src_ptr[2]] >> 2);
			buf_ptr[2] = static_cast<std::uint8_t>(detail::ASCII_TO_BITS[src_ptr[2]] << 6 | detail::ASCII_TO_BITS[src_ptr[3]] >> 0);
		}

		if (size > 1)
		{
			buf_ptr[0] = (detail::ASCII_TO_BITS[*src_ptr] << 2 | detail::ASCII_TO_BITS[src_ptr[1]] >> 4);

			if (size > 2)
			{
				buf_ptr[1] = (detail::ASCII_TO_BITS[src_ptr[1]] << 4 | detail::ASCII_TO_BITS[src_ptr[2]] >> 2);
			}
		}
	}
}
