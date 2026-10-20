#pragma once

#include "OmegaLocalization/OmegaLocalization.hpp"

#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace OmegaLocalization
{
	struct stStringHash
	{
		using is_transparent = void;
		[[nodiscard]] std::size_t operator()(std::string_view strText) const noexcept { return std::hash<std::string_view>{}(strText); }
	};

	using StringMap = std::unordered_map<std::string, std::string, stStringHash, std::equal_to<>>;

	struct stLocale
	{
		std::string strCode;
		std::string strName;
		std::vector<stGlyphRange> aGlyphRanges;
		StringMap mapStrings;
		StringMap mapLabels;
	};

	struct stState
	{
		std::recursive_mutex mutexState;
		std::vector<std::unique_ptr<stLocale>> aLocales;
		std::vector<std::unique_ptr<stLocale>> aRetired;
		stLocale* pCurrent = nullptr;
		std::string strRequested;
		std::string strFallback = "en";
		std::unordered_set<std::string, stStringHash, std::equal_to<>> setUnresolved;
		std::unordered_map<std::size_t, std::function<void()>> mapSubscribers;
		std::size_t uNextSubscriber = 0;
		std::uint64_t uRevision = 1;
	};

	[[nodiscard]] stState& State();
	[[nodiscard]] std::string EscapeValue(std::string_view strValue);
	[[nodiscard]] std::string FormatRanges(std::span<const stGlyphRange> aRanges);
}
