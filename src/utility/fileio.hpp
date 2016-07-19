#pragma once

#include "error_chain.hpp"

#include <cstdio>
#include <memory>

typedef std::shared_ptr<std::FILE> SharedFileHandle;

std::uint32_t openFile(ErrorChain& errorChain, SharedFileHandle& file, const char* filename, const char* mode)
{
	const auto closeFile = [](std::FILE* handle)
	{
		if (handle != nullptr)
		{
			std::fclose(handle);
		}
	};

	file = SharedFileHandle(std::fopen(filename, mode), closeFile);

	if (file.get() == nullptr)
	{
		errorChain.addMessage("Failed to open file '%s' with mode '%s'.", filename, mode);
		return 1;
	}

	return 0;
}

struct WriteDescriptor
{
	const void* data;
	std::size_t size;
};

class FileWriter
{
public:
	static constexpr std::size_t BUFFER_SIZE = 8 * 1024;

private:
	std::array<char, BUFFER_SIZE> _buffer;
	std::size_t _written = 0;
	SharedFileHandle _file;

public:
	~FileWriter()
	{
		flush();
	}

	FileWriter(SharedFileHandle file)
		: _file(std::move(file))
	{}

	const SharedFileHandle& handle()
	{
		return _file;
	}

	void flush()
	{
		if (_written != 0)
		{
			std::fwrite(_buffer.data(), 1, _written, _file.get());
			_written = 0;
		}
	}

	void write(ErrorChain& errorChain, const void* data, std::size_t size)
	{
		if (size >= _buffer.size())
		{
			flush();
			std::fwrite(data, 1, size, _file.get());
		}
		else
		{
			if (size + _written > _buffer.size())
			{
				flush();
			}

			std::memcpy(&_buffer[_written], data, size);
			_written += size;
		}
	}

	template <typename... Args>
	std::uint32_t combinedWrite(ErrorChain& errorChain, Args... writeDescriptors)
	{
		const auto size = partialWriteSize(writeDescriptors...);

		if (size <= _buffer.size())
		{
			if (size + _written > _buffer.size())
			{
				flush();
			}

			partialWrite(_written, writeDescriptors...);
			_written += size;
		}
		else
		{
			slowWrite(writeDescriptors...);
		}
	}

private:
	static std::size_t partialWriteSize(PartialWrite writeDescriptor)
	{
		return writeDescriptor.size;
	}

	template <typename... Args>
	static std::size_t partialWriteSize(PartialWrite writeDescriptor, Args... writeDescriptors)
	{
		return writeDescriptor.size + partialWriteSize(writeDescriptors...);
	}

	void partialWrite(std::size_t offset, PartialWrite writeDescriptor)
	{
		std::memcpy(&_buffer[offset], writeDescriptor.data, writeDescriptor.size);
	}

	template <typename... Args>
	void partialWrite(std::size_t offset, PartialWrite writeDescriptor, Args... writeDescriptors)
	{
		std::memcpy(&_buffer[offset], writeDescriptor.data, writeDescriptor.size);
		partialWrite(offset + writeDescriptor.size, writeDescriptors...);
	}

	void slowWrite(PartialWrite writeDescriptor)
	{
		write(writeDescriptor.data, writeDescriptor.size);
	}

	template <typename... Args>
	void slowWrite(PartialWrite writeDescriptor, Args... writeDescriptors)
	{
		write(writeDescriptor.data, writeDescriptor.size);
		slowWrite(writeDescriptors...);
	}
};

inline std::vector<char> readFileBinary(const std::string& filename)
{
	using namespace std::experimental;

	std::vector<char> v;
	std::error_code ec;
	const auto size = filesystem::file_size(filename, ec);

	if (!ec)
	{
		v.resize(static_cast<std::size_t>(size));
		FileHandle file(std::fopen(filename.c_str(), "rb"));
		std::fread(v.data(), 1, v.size(), file.get());
	}

	return v;
}

inline std::string readFileBinaryAsString(const std::string& filename)
{
	using namespace std::experimental;

	std::string s;
	std::error_code ec;
	const auto size = filesystem::file_size(filename, ec);

	if (!ec)
	{
		s.resize(static_cast<std::size_t>(size));
		FileHandle file(std::fopen(filename.c_str(), "rb"));
		std::fread(&s[0], 1, s.size(), file.get()); // &s[0] is always valid since C++11.
	}

	return s;
}
