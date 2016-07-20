#pragma once

#include "utility/utility.hpp"

#include <cctype>
#include <cstdint>
#include <algorithm>
#include <string>
#include <vector>

inline std::uint32_t levenshteinDistance(const std::string& s0, const std::string& s1)
{
	constexpr std::uint16_t colSize = 4096;

	if (s0.size() > colSize - 1 || s1.size() > colSize - 1)
	{
		return 0;
	}

	std::uint16_t buf0[colSize];
	std::uint16_t buf1[colSize];

	auto col = buf0;
	auto prev_col = buf1;

	for (std::uint16_t i = 0; i < colSize; i++)
	{
		prev_col[i] = i;
	}

	for (std::uint16_t i = 0; i < s0.size(); i++)
	{
		col[0] = i + 1;

		for (std::uint16_t j = 0; j < s1.size(); j++)
		{
			col[j + 1] = std::min({ prev_col[1 + j] + 1, col[j] + 1, prev_col[j] + (s0[i] == s1[j] ? 0 : 1) });
		}

		std::swap(col, prev_col);
	}

	return prev_col[s1.size()];
}

inline std::uint32_t wordBasedEditDistance(const std::string& searchString, const std::string& dataString)
{
	const auto convertToLowerCase = [](std::vector<std::string> words)
	{
		for (auto& word : words)
		{
			for (auto& c : word)
			{
				if (std::isalpha(c))
				{
					c = std::tolower(c);
				}
			}
		}

		return words;
	};

	auto searchWords = convertToLowerCase(parseWords(searchString));
	auto dataWords = convertToLowerCase(parseWords(dataString));

	std::uint32_t distance = 0;

	for (const auto& searchWord : searchWords)
	{
		auto minDistance = std::numeric_limits<std::uint32_t>::max();

		for (auto dataWord : dataWords)
		{
			for (std::size_t i = 0; i < dataWord.size(); ++i)
			{
				minDistance = std::min(minDistance, 
					levenshteinDistance(searchWord, dataWord.substr(i, searchWord.size())));
			}
		}

		distance += minDistance;
	}

	return distance;
}
