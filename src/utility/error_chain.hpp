#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>

class ErrorChain
{
public:
	static constexpr std::size_t BUFFER_SIZE = 4 * 1024;

private:
	char _messageChain[BUFFER_SIZE];
	std::uint16_t _written;

public:
	ErrorChain()
		: _written(0)
	{
		_messageChain[0] = '\0';
	}

	template <std::size_t N, typename... Args>
	void addMessage(const char(&format)[N], Args&&... args)
	{
		_written += static_cast<std::uint16_t>(1 + std::snprintf(&_messageChain[1 + _written],
			BUFFER_SIZE - 1 - _written, format, args...));
	}

	bool hasMessages() const
	{
		return _written != 0;
	}

	void clear()
	{
		_written = 0;
	}

	const char* firstMessage() const
	{
		return _written > 0 ? &_messageChain[1] : nullptr;
	}

	const char* lastMessage() const
	{
		if (_written > 0)
		{
			auto ptr = &_messageChain[_written];
			while (*--ptr != '\0');
			return ptr + 1;
		}

		return nullptr;
	}

	const char* nextMessage(const char* ptr) const
	{
		while (*ptr++ != '\0');
		return ptr - _messageChain >= _written ? nullptr : ptr;
	}

	const char* prevMessage(const char* ptr) const
	{
		if (--ptr > _messageChain)
		{
			while (*--ptr != '\0');
			return ptr + 1;
		}

		return nullptr;
	}

	void printMessages()
	{
		auto ptr = lastMessage();

		for (std::size_t i = 0; ptr != nullptr; ++i, ptr = prevMessage(ptr))
		{
			std::printf("%*s -> %s\n", static_cast<int>(i), "", ptr);
		}
	}
};
