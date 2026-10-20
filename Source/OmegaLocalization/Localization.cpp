#include "OmegaLocalization/OmegaLocalization.hpp"
#include "OmegaLocalization/Internal.hpp"

#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>

namespace OmegaLocalization
{
	namespace
	{
		constexpr stGlyphRange stBasicLatin{ 0x0020, 0x007E };
		constexpr char32_t kSmallScript = 0x400;

		[[nodiscard]] std::vector<char32_t> DecodeCodepoints(std::string_view strText)
		{
			std::vector<char32_t> aCodepoints;
			for (std::size_t i = 0; i < strText.size();)
			{
				const auto byLead = static_cast<unsigned char>(strText[i]);
				const std::size_t uLength = byLead < 0x80 ? 1 : (byLead >> 5) == 0x6 ? 2 : (byLead >> 4) == 0xE ? 3 : (byLead >> 3) == 0x1E ? 4 : 1;
				char32_t uCodepoint = uLength == 1 ? byLead : byLead & (0x7F >> uLength);
				for (std::size_t j = 1; j < uLength && i + j < strText.size(); ++j)
					uCodepoint = (uCodepoint << 6) | (static_cast<unsigned char>(strText[i + j]) & 0x3F);
				aCodepoints.push_back(uCodepoint);
				i += uLength;
			}
			return aCodepoints;
		}

		[[nodiscard]] std::string Lowercase(std::string_view strText)
		{
			std::string strResult(strText);
			std::ranges::transform(strResult, strResult.begin(), [](unsigned char chLetter) { return static_cast<char>(std::tolower(chLetter)); });
			std::ranges::replace(strResult, '_', '-');
			return strResult;
		}

		[[nodiscard]] std::string_view BaseLanguage(std::string_view strCode)
		{
			const std::size_t uSplit = strCode.find_first_of("-_");
			return uSplit == std::string_view::npos ? strCode : strCode.substr(0, uSplit);
		}

		[[nodiscard]] stLocale* FindLocaleExact(stState& rState, std::string_view strCode)
		{
			const std::string strLower = Lowercase(strCode);
			for (const auto& pLocale : rState.aLocales)
				if (Lowercase(pLocale->strCode) == strLower)
					return pLocale.get();
			return nullptr;
		}

		[[nodiscard]] stLocale* FindLocale(stState& rState, std::string_view strCode)
		{
			if (strCode.empty())
				return nullptr;
			if (stLocale* pLocale = FindLocaleExact(rState, strCode))
				return pLocale;
			const std::string strBase = Lowercase(BaseLanguage(strCode));
			for (const auto& pLocale : rState.aLocales)
				if (Lowercase(BaseLanguage(pLocale->strCode)) == strBase)
					return pLocale.get();
			return nullptr;
		}

		stLocale& EnsureLocale(stState& rState, std::string_view strCode)
		{
			if (stLocale* pLocale = FindLocaleExact(rState, strCode))
				return *pLocale;
			auto pLocale = std::make_unique<stLocale>();
			pLocale->strCode = std::string(strCode);
			pLocale->strName = std::string(strCode);
			rState.aLocales.push_back(std::move(pLocale));
			if (!rState.pCurrent && !rState.strRequested.empty())
				rState.pCurrent = FindLocale(rState, rState.strRequested);
			return *rState.aLocales.back();
		}

		[[nodiscard]] const std::string* FindString(const stLocale* pLocale, std::string_view strKey)
		{
			if (!pLocale)
				return nullptr;
			const auto itString = pLocale->mapStrings.find(strKey);
			return itString != pLocale->mapStrings.end() ? &itString->second : nullptr;
		}

		[[nodiscard]] stLocale* Effective(stState& rState)
		{
			return rState.pCurrent ? rState.pCurrent : FindLocale(rState, rState.strFallback);
		}

		[[nodiscard]] const std::string& Resolve(stState& rState, std::string_view strKey)
		{
			if (const std::string* pString = FindString(rState.pCurrent, strKey))
				return *pString;
			if (const std::string* pString = FindString(FindLocale(rState, rState.strFallback), strKey))
				return *pString;
			return *rState.setUnresolved.emplace(strKey).first;
		}

		[[nodiscard]] std::vector<std::function<void()>> CollectSubscribers(stState& rState)
		{
			std::vector<std::function<void()>> aCallbacks;
			aCallbacks.reserve(rState.mapSubscribers.size());
			for (const auto& [uHandle, fnCallback] : rState.mapSubscribers)
				aCallbacks.push_back(fnCallback);
			return aCallbacks;
		}

		void Notify(std::vector<std::function<void()>> aCallbacks)
		{
			for (const auto& fnCallback : aCallbacks)
				if (fnCallback)
					fnCallback();
		}

		void MergeRanges(std::vector<stGlyphRange>& rTarget, std::span<const stGlyphRange> aRanges)
		{
			for (const stGlyphRange& rRange : aRanges)
				if (rRange.uFirst <= rRange.uLast && std::ranges::find(rTarget, rRange) == rTarget.end())
					rTarget.push_back(rRange);
		}
	}

	stState& State()
	{
		static stState s_stState;
		return s_stState;
	}

	bool AddLocale(std::string_view strCode, std::string_view strName, std::span<const stGlyphRange> aGlyphRanges)
	{
		if (strCode.empty())
			return false;
		std::vector<std::function<void()>> aCallbacks;
		{
			stState& rState = State();
			std::scoped_lock lockState(rState.mutexState);
			stLocale& rLocale = EnsureLocale(rState, strCode);
			if (!strName.empty())
				rLocale.strName = std::string(strName);
			MergeRanges(rLocale.aGlyphRanges, aGlyphRanges);
			++rState.uRevision;
			aCallbacks = CollectSubscribers(rState);
		}
		Notify(std::move(aCallbacks));
		return true;
	}

	void AddString(std::string_view strCode, std::string_view strKey, std::string_view strText)
	{
		stState& rState = State();
		std::scoped_lock lockState(rState.mutexState);
		stLocale& rLocale = EnsureLocale(rState, strCode);
		auto itString = rLocale.mapStrings.find(strKey);
		if (itString == rLocale.mapStrings.end())
			rLocale.mapStrings.emplace(std::string(strKey), std::string(strText));
		else
			itString->second = std::string(strText);
		for (const auto& pLocale : rState.aLocales)
			pLocale->mapLabels.erase(std::string(strKey));
		++rState.uRevision;
	}

	void AddStrings(std::string_view strCode, std::initializer_list<std::pair<std::string_view, std::string_view>> aStrings)
	{
		for (const auto& [strKey, strText] : aStrings)
			AddString(strCode, strKey, strText);
	}

	void LoadLocale(const stLocaleData& stLocale)
	{
		AddLocale(stLocale.strCode, stLocale.strName, stLocale.aGlyphRanges);
		for (const auto& [strKey, strText] : stLocale.aStrings)
			AddString(stLocale.strCode, strKey, strText);
	}

	void Clear()
	{
		stState& rState = State();
		std::scoped_lock lockState(rState.mutexState);
		rState.aRetired.insert(rState.aRetired.end(), std::make_move_iterator(rState.aLocales.begin()), std::make_move_iterator(rState.aLocales.end()));
		rState.aLocales.clear();
		rState.pCurrent = nullptr;
		++rState.uRevision;
	}

	bool SetLocale(std::string_view strCode)
	{
		std::vector<std::function<void()>> aCallbacks;
		{
			stState& rState = State();
			std::scoped_lock lockState(rState.mutexState);
			stLocale* pLocale = FindLocale(rState, strCode);
			if (!pLocale)
				return false;
			rState.strRequested = std::string(strCode);
			if (pLocale == Effective(rState) && rState.pCurrent)
				return true;
			rState.pCurrent = pLocale;
			++rState.uRevision;
			aCallbacks = CollectSubscribers(rState);
		}
		Notify(std::move(aCallbacks));
		return true;
	}

	bool SetLocaleFromSystem()
	{
		wchar_t szLocale[LOCALE_NAME_MAX_LENGTH]{};
		if (GetUserDefaultLocaleName(szLocale, LOCALE_NAME_MAX_LENGTH) <= 0)
			return false;
		std::string strCode;
		for (const wchar_t* pChar = szLocale; *pChar; ++pChar)
			strCode.push_back(static_cast<char>(*pChar));
		return SetLocale(strCode);
	}

	void SetFallbackLocale(std::string_view strCode)
	{
		stState& rState = State();
		std::scoped_lock lockState(rState.mutexState);
		rState.strFallback = std::string(strCode);
		for (const auto& pLocale : rState.aLocales)
			pLocale->mapLabels.clear();
		++rState.uRevision;
	}

	std::string_view GetLocale()
	{
		stState& rState = State();
		std::scoped_lock lockState(rState.mutexState);
		const stLocale* pLocale = Effective(rState);
		return pLocale ? std::string_view(pLocale->strCode) : std::string_view();
	}

	std::string_view GetFallbackLocale()
	{
		stState& rState = State();
		std::scoped_lock lockState(rState.mutexState);
		return rState.strFallback;
	}

	std::vector<stLocaleInfo> GetLocales()
	{
		stState& rState = State();
		std::scoped_lock lockState(rState.mutexState);
		std::vector<stLocaleInfo> aResult;
		for (const auto& pLocale : rState.aLocales)
			aResult.push_back({ pLocale->strCode, pLocale->strName, pLocale->aGlyphRanges, pLocale->mapStrings.size() });
		return aResult;
	}

	bool HasLocale(std::string_view strCode)
	{
		stState& rState = State();
		std::scoped_lock lockState(rState.mutexState);
		return FindLocale(rState, strCode) != nullptr;
	}

	std::string_view Get(std::string_view strKey)
	{
		stState& rState = State();
		std::scoped_lock lockState(rState.mutexState);
		return Resolve(rState, strKey);
	}

	const char* GetCString(std::string_view strKey)
	{
		stState& rState = State();
		std::scoped_lock lockState(rState.mutexState);
		return Resolve(rState, strKey).c_str();
	}

	std::string_view Label(std::string_view strKey)
	{
		stState& rState = State();
		std::scoped_lock lockState(rState.mutexState);
		stLocale* pOwner = Effective(rState);
		if (!pOwner)
		{
			const std::string strLabel = std::string(strKey) + "###" + std::string(strKey);
			return *rState.setUnresolved.emplace(strLabel).first;
		}
		auto itLabel = pOwner->mapLabels.find(strKey);
		if (itLabel != pOwner->mapLabels.end())
			return itLabel->second;
		const std::string& rText = Resolve(rState, strKey);
		return pOwner->mapLabels.emplace(std::string(strKey), rText + "###" + std::string(strKey)).first->second;
	}

	bool Has(std::string_view strKey)
	{
		stState& rState = State();
		std::scoped_lock lockState(rState.mutexState);
		return FindString(rState.pCurrent, strKey) || FindString(FindLocale(rState, rState.strFallback), strKey);
	}

	EPluralCategory PluralCategory(std::string_view strCode, long long llCount)
	{
		const std::string strBase = Lowercase(BaseLanguage(strCode));
		const long long llAbsolute = llCount < 0 ? -llCount : llCount;
		const long long llMod10 = llAbsolute % 10;
		const long long llMod100 = llAbsolute % 100;
		if (strBase == "ja" || strBase == "zh" || strBase == "ko" || strBase == "th" || strBase == "vi" || strBase == "id")
			return EPluralCategory::Other;
		if (strBase == "pl")
		{
			if (llAbsolute == 1)
				return EPluralCategory::One;
			if (llMod10 >= 2 && llMod10 <= 4 && (llMod100 < 12 || llMod100 > 14))
				return EPluralCategory::Few;
			return EPluralCategory::Many;
		}
		if (strBase == "ru" || strBase == "uk" || strBase == "be")
		{
			if (llMod10 == 1 && llMod100 != 11)
				return EPluralCategory::One;
			if (llMod10 >= 2 && llMod10 <= 4 && (llMod100 < 12 || llMod100 > 14))
				return EPluralCategory::Few;
			return EPluralCategory::Many;
		}
		if (strBase == "cs" || strBase == "sk")
		{
			if (llAbsolute == 1)
				return EPluralCategory::One;
			if (llAbsolute >= 2 && llAbsolute <= 4)
				return EPluralCategory::Few;
			return EPluralCategory::Other;
		}
		if (strBase == "fr")
			return llAbsolute <= 1 ? EPluralCategory::One : EPluralCategory::Other;
		return llAbsolute == 1 ? EPluralCategory::One : EPluralCategory::Other;
	}

	std::string_view Plural(std::string_view strKey, long long llCount)
	{
		static constexpr std::string_view s_aSuffixes[] = { ".one", ".few", ".many", ".other" };
		stState& rState = State();
		std::scoped_lock lockState(rState.mutexState);
		const stLocale* aCandidates[] = { Effective(rState), FindLocale(rState, rState.strFallback) };
		for (const stLocale* pLocale : aCandidates)
		{
			if (!pLocale)
				continue;
			const EPluralCategory eCategory = PluralCategory(pLocale->strCode, llCount);
			const std::string strExact = std::string(strKey) + std::string(s_aSuffixes[static_cast<std::size_t>(eCategory)]);
			if (const std::string* pString = FindString(pLocale, strExact))
				return *pString;
			if (const std::string* pString = FindString(pLocale, std::string(strKey) + ".other"))
				return *pString;
			if (const std::string* pString = FindString(pLocale, strKey))
				return *pString;
		}
		return *rState.setUnresolved.emplace(strKey).first;
	}

	std::vector<stGlyphRange> GlyphRanges()
	{
		stState& rState = State();
		std::scoped_lock lockState(rState.mutexState);
		std::vector<stGlyphRange> aResult{ stBasicLatin };
		const stLocale* pFallback = FindLocale(rState, rState.strFallback);
		const stLocale* pCurrent = Effective(rState);
		if (pFallback)
			MergeRanges(aResult, pFallback->aGlyphRanges);
		if (pCurrent)
			MergeRanges(aResult, pCurrent->aGlyphRanges);
		for (const auto& pLocale : rState.aLocales)
		{
			if (pLocale.get() == pFallback || pLocale.get() == pCurrent)
				continue;
			std::vector<stGlyphRange> aNameRanges;
			for (const stGlyphRange& rRange : pLocale->aGlyphRanges)
				if (rRange.uLast - rRange.uFirst < kSmallScript)
					aNameRanges.push_back(rRange);
			for (const char32_t uCodepoint : DecodeCodepoints(pLocale->strName))
				if (uCodepoint > stBasicLatin.uLast)
					aNameRanges.push_back({ uCodepoint, uCodepoint });
			MergeRanges(aResult, aNameRanges);
		}
		return aResult;
	}

	std::uint64_t Revision()
	{
		stState& rState = State();
		std::scoped_lock lockState(rState.mutexState);
		return rState.uRevision;
	}

	std::size_t Subscribe(std::function<void()> fnChanged)
	{
		stState& rState = State();
		std::scoped_lock lockState(rState.mutexState);
		const std::size_t uHandle = ++rState.uNextSubscriber;
		rState.mapSubscribers.emplace(uHandle, std::move(fnChanged));
		return uHandle;
	}

	void Unsubscribe(std::size_t uHandle)
	{
		stState& rState = State();
		std::scoped_lock lockState(rState.mutexState);
		rState.mapSubscribers.erase(uHandle);
	}

	std::vector<std::string> MissingKeys(std::string_view strCode)
	{
		stState& rState = State();
		std::scoped_lock lockState(rState.mutexState);
		const stLocale* pTarget = FindLocale(rState, strCode);
		std::vector<std::string> aMissing;
		for (const auto& pLocale : rState.aLocales)
		{
			if (pLocale.get() == pTarget)
				continue;
			for (const auto& [strKey, strText] : pLocale->mapStrings)
				if (!FindString(pTarget, strKey) && std::ranges::find(aMissing, strKey) == aMissing.end())
					aMissing.push_back(strKey);
		}
		std::ranges::sort(aMissing);
		return aMissing;
	}

	bool ExportTemplate(const std::filesystem::path& pathFile, std::string_view strCode, std::string_view strSourceCode)
	{
		stState& rState = State();
		std::scoped_lock lockState(rState.mutexState);
		const stLocale* pSource = FindLocale(rState, strSourceCode.empty() ? std::string_view(rState.strFallback) : strSourceCode);
		if (!pSource)
			return false;
		const stLocale* pTarget = FindLocale(rState, strCode);

		std::ofstream fileOut(pathFile, std::ios::binary | std::ios::trunc);
		if (!fileOut)
			return false;
		fileOut << "@code = " << strCode << "\n";
		fileOut << "@name = " << (pTarget ? pTarget->strName : std::string(strCode)) << "\n";
		if (pTarget && !pTarget->aGlyphRanges.empty())
			fileOut << "@ranges = " << FormatRanges(pTarget->aGlyphRanges) << "\n";
		fileOut << "\n";

		const std::map<std::string, std::string> mapSorted(pSource->mapStrings.begin(), pSource->mapStrings.end());
		for (const auto& [strKey, strText] : mapSorted)
		{
			const std::string* pExisting = FindString(pTarget, strKey);
			if (!pExisting)
				fileOut << "# " << pSource->strCode << ": " << EscapeValue(strText) << "\n";
			fileOut << strKey << " = " << (pExisting ? EscapeValue(*pExisting) : std::string()) << "\n";
		}
		return true;
	}
}
