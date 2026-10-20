#include "OmegaLocalization/Internal.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <iterator>

namespace OmegaLocalization
{
	namespace
	{
		[[nodiscard]] std::string_view Trim(std::string_view strText)
		{
			while (!strText.empty() && (strText.front() == ' ' || strText.front() == '\t'))
				strText.remove_prefix(1);
			while (!strText.empty() && (strText.back() == ' ' || strText.back() == '\t' || strText.back() == '\r'))
				strText.remove_suffix(1);
			return strText;
		}

		[[nodiscard]] std::string UnescapeValue(std::string_view strValue)
		{
			if (strValue.size() >= 2 && strValue.front() == '"' && strValue.back() == '"')
				strValue = strValue.substr(1, strValue.size() - 2);
			std::string strResult;
			strResult.reserve(strValue.size());
			for (std::size_t i = 0; i < strValue.size(); ++i)
			{
				if (strValue[i] != '\\' || i + 1 >= strValue.size())
				{
					strResult.push_back(strValue[i]);
					continue;
				}
				switch (strValue[++i])
				{
				case 'n': strResult.push_back('\n'); break;
				case 't': strResult.push_back('\t'); break;
				case '"': strResult.push_back('"'); break;
				case '\\': strResult.push_back('\\'); break;
				default:
					strResult.push_back('\\');
					strResult.push_back(strValue[i]);
					break;
				}
			}
			return strResult;
		}

		[[nodiscard]] bool ParseCodepoint(std::string_view strText, char32_t& rCodepoint)
		{
			strText = Trim(strText);
			if (strText.starts_with("U+") || strText.starts_with("u+") || strText.starts_with("0x") || strText.starts_with("0X"))
				strText.remove_prefix(2);
			std::uint32_t uValue = 0;
			const auto stResult = std::from_chars(strText.data(), strText.data() + strText.size(), uValue, 16);
			if (stResult.ec != std::errc() || stResult.ptr != strText.data() + strText.size() || uValue > 0x10FFFF)
				return false;
			rCodepoint = static_cast<char32_t>(uValue);
			return true;
		}

		[[nodiscard]] bool ParseRanges(std::string_view strText, std::vector<stGlyphRange>& rRanges)
		{
			while (!strText.empty())
			{
				const std::size_t uComma = strText.find(',');
				const std::string_view strPart = Trim(strText.substr(0, uComma));
				strText = uComma == std::string_view::npos ? std::string_view() : strText.substr(uComma + 1);
				if (strPart.empty())
					continue;
				const std::size_t uDash = strPart.find('-');
				stGlyphRange stRange;
				if (!ParseCodepoint(strPart.substr(0, uDash), stRange.uFirst))
					return false;
				stRange.uLast = stRange.uFirst;
				if (uDash != std::string_view::npos && !ParseCodepoint(strPart.substr(uDash + 1), stRange.uLast))
					return false;
				if (stRange.uLast < stRange.uFirst)
					std::swap(stRange.uFirst, stRange.uLast);
				rRanges.push_back(stRange);
			}
			return true;
		}
	}

	std::string EscapeValue(std::string_view strValue)
	{
		std::string strResult;
		const bool bQuote = !strValue.empty() && (strValue.front() == ' ' || strValue.back() == ' ' || strValue.front() == '"');
		if (bQuote)
			strResult.push_back('"');
		for (const char chLetter : strValue)
		{
			switch (chLetter)
			{
			case '\n': strResult += "\\n"; break;
			case '\t': strResult += "\\t"; break;
			case '\\': strResult += "\\\\"; break;
			case '"': strResult += bQuote ? "\\\"" : "\""; break;
			default: strResult.push_back(chLetter); break;
			}
		}
		if (bQuote)
			strResult.push_back('"');
		return strResult;
	}

	std::string FormatRanges(std::span<const stGlyphRange> aRanges)
	{
		std::string strResult;
		for (const stGlyphRange& rRange : aRanges)
		{
			if (!strResult.empty())
				strResult += ", ";
			strResult += rRange.uFirst == rRange.uLast ? std::format("{:04X}", static_cast<std::uint32_t>(rRange.uFirst))
				: std::format("{:04X}-{:04X}", static_cast<std::uint32_t>(rRange.uFirst), static_cast<std::uint32_t>(rRange.uLast));
		}
		return strResult;
	}

	bool LoadFromMemory(std::string_view strContents, std::string* pError)
	{
		if (strContents.starts_with("\xEF\xBB\xBF"))
			strContents.remove_prefix(3);

		std::string strCode;
		std::string strName;
		std::vector<stGlyphRange> aRanges;
		std::vector<std::pair<std::string, std::string>> aPending;
		std::string strErrors;
		bool bAnyLocale = false;

		auto Flush = [&] {
			if (strCode.empty())
				return;
			AddLocale(strCode, strName, aRanges);
			for (const auto& [strKey, strText] : aPending)
				AddString(strCode, strKey, strText);
			aPending.clear();
			aRanges.clear();
			strName.clear();
			bAnyLocale = true;
		};

		std::size_t uLineNumber = 0;
		while (!strContents.empty())
		{
			++uLineNumber;
			const std::size_t uEnd = strContents.find('\n');
			const std::string_view strLine = Trim(strContents.substr(0, uEnd));
			strContents = uEnd == std::string_view::npos ? std::string_view() : strContents.substr(uEnd + 1);
			if (strLine.empty() || strLine.front() == '#' || strLine.front() == ';')
				continue;

			const std::size_t uEquals = strLine.find('=');
			if (uEquals == std::string_view::npos)
			{
				strErrors += std::format("line {}: expected 'key = value'\n", uLineNumber);
				continue;
			}
			const std::string_view strKey = Trim(strLine.substr(0, uEquals));
			const std::string_view strValue = Trim(strLine.substr(uEquals + 1));
			if (strKey.empty())
			{
				strErrors += std::format("line {}: empty key\n", uLineNumber);
				continue;
			}

			if (strKey == "@code")
			{
				Flush();
				strCode = std::string(strValue);
				continue;
			}
			if (strCode.empty())
			{
				strErrors += std::format("line {}: '{}' appears before any '@code ='\n", uLineNumber, strKey);
				continue;
			}
			if (strKey == "@name")
				strName = UnescapeValue(strValue);
			else if (strKey == "@ranges")
			{
				if (!ParseRanges(strValue, aRanges))
					strErrors += std::format("line {}: bad glyph range '{}'\n", uLineNumber, strValue);
			}
			else if (strKey.front() == '@')
				strErrors += std::format("line {}: unknown directive '{}'\n", uLineNumber, strKey);
			else
				aPending.emplace_back(std::string(strKey), UnescapeValue(strValue));
		}
		Flush();

		if (!bAnyLocale && strErrors.empty())
			strErrors = "no '@code =' line found\n";
		if (pError)
			*pError = strErrors;
		return strErrors.empty();
	}

	bool LoadFile(const std::filesystem::path& pathFile, std::string* pError)
	{
		std::ifstream fileIn(pathFile, std::ios::binary);
		if (!fileIn)
		{
			if (pError)
				*pError = std::format("cannot open '{}'\n", pathFile.string());
			return false;
		}
		const std::string strContents{ std::istreambuf_iterator<char>(fileIn), std::istreambuf_iterator<char>() };
		std::string strError;
		const bool bResult = LoadFromMemory(strContents, &strError);
		if (pError && !strError.empty())
			*pError = std::format("{}:\n{}", pathFile.filename().string(), strError);
		return bResult;
	}

	std::size_t LoadDirectory(const std::filesystem::path& pathDirectory, std::string* pError)
	{
		std::error_code errCode;
		std::vector<std::filesystem::path> aFiles;
		for (const auto& rEntry : std::filesystem::directory_iterator(pathDirectory, errCode))
			if (rEntry.is_regular_file() && rEntry.path().extension() == ".lang")
				aFiles.push_back(rEntry.path());
		std::ranges::sort(aFiles);

		std::size_t uLoaded = 0;
		std::string strErrors;
		for (const auto& pathFile : aFiles)
		{
			std::string strError;
			if (LoadFile(pathFile, &strError))
				++uLoaded;
			strErrors += strError;
		}
		if (errCode)
			strErrors += std::format("cannot read '{}': {}\n", pathDirectory.string(), errCode.message());
		if (pError)
			*pError = strErrors;
		return uLoaded;
	}
}
