#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <functional>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace OmegaLocalization
{
	struct stGlyphRange
	{
		char32_t uFirst = 0;
		char32_t uLast = 0;

		constexpr bool operator==(const stGlyphRange&) const = default;
	};

	struct stLocaleInfo
	{
		std::string strCode;
		std::string strName;
		std::vector<stGlyphRange> aGlyphRanges;
		std::size_t uStringCount = 0;
	};

	struct stLocaleData
	{
		std::string_view strCode;
		std::string_view strName;
		std::vector<stGlyphRange> aGlyphRanges;
		std::vector<std::pair<std::string_view, std::string_view>> aStrings;
	};

	enum class EPluralCategory : std::uint8_t
	{
		One,
		Few,
		Many,
		Other,
	};

	bool AddLocale(std::string_view strCode, std::string_view strName, std::span<const stGlyphRange> aGlyphRanges = {});
	void AddString(std::string_view strCode, std::string_view strKey, std::string_view strText);
	void AddStrings(std::string_view strCode, std::initializer_list<std::pair<std::string_view, std::string_view>> aStrings);

	void LoadLocale(const stLocaleData& stLocale);
	bool LoadFromMemory(std::string_view strContents, std::string* pError = nullptr);
	bool LoadFile(const std::filesystem::path& pathFile, std::string* pError = nullptr);
	std::size_t LoadDirectory(const std::filesystem::path& pathDirectory, std::string* pError = nullptr);
	void Clear();

	bool SetLocale(std::string_view strCode);
	bool SetLocaleFromSystem();
	void SetFallbackLocale(std::string_view strCode);
	[[nodiscard]] std::string_view GetLocale();
	[[nodiscard]] std::string_view GetFallbackLocale();
	[[nodiscard]] std::vector<stLocaleInfo> GetLocales();
	[[nodiscard]] bool HasLocale(std::string_view strCode);

	[[nodiscard]] std::string_view Get(std::string_view strKey);
	[[nodiscard]] const char* GetCString(std::string_view strKey);
	[[nodiscard]] std::string_view Label(std::string_view strKey);
	[[nodiscard]] bool Has(std::string_view strKey);
	[[nodiscard]] EPluralCategory PluralCategory(std::string_view strCode, long long llCount);
	[[nodiscard]] std::string_view Plural(std::string_view strKey, long long llCount);

	template <typename... Args>
	[[nodiscard]] std::string Format(std::string_view strKey, Args&&... args)
	{
		const std::string_view strPattern = Get(strKey);
		try
		{
			return std::vformat(strPattern, std::make_format_args(args...));
		}
		catch (const std::format_error&)
		{
			return std::string(strPattern);
		}
	}

	template <typename... Args>
	[[nodiscard]] std::string FormatPlural(std::string_view strKey, long long llCount, Args&&... args)
	{
		const std::string_view strPattern = Plural(strKey, llCount);
		try
		{
			return std::vformat(strPattern, std::make_format_args(llCount, args...));
		}
		catch (const std::format_error&)
		{
			return std::string(strPattern);
		}
	}

	[[nodiscard]] std::vector<stGlyphRange> GlyphRanges();
	[[nodiscard]] std::uint64_t Revision();
	std::size_t Subscribe(std::function<void()> fnChanged);
	void Unsubscribe(std::size_t uHandle);

	[[nodiscard]] std::vector<std::string> MissingKeys(std::string_view strCode);
	bool ExportTemplate(const std::filesystem::path& pathFile, std::string_view strCode, std::string_view strSourceCode = {});

	namespace Literals
	{
		[[nodiscard]] inline std::string_view operator""_t(const char* szKey, std::size_t uLength)
		{
			return Get({ szKey, uLength });
		}

		[[nodiscard]] inline std::string_view operator""_l(const char* szKey, std::size_t uLength)
		{
			return Label({ szKey, uLength });
		}
	}
}
