#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <stdexcept>

class MemoryReader
{
	const std::uint8_t* _bytePtr;
	std::size_t _bytes;

public:
	MemoryReader(const void* ptr, std::size_t bytes)
		: _bytePtr(static_cast<const std::uint8_t*>(ptr))
		, _bytes(bytes)
	{}

	std::size_t remaining() const
	{
		return _bytes;
	}

	bool read(void* buffer, std::size_t bytes)
	{
		if (remaining() >= bytes)
		{
			std::memcpy(buffer, _bytePtr, bytes);
			_bytePtr += bytes;
			_bytes -= bytes;

			return true;
		}

		return false;
	}
};
