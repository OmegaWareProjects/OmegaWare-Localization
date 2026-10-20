#pragma once

#include "OmegaGUI/OmegaGUI.hpp"
#include "OmegaLocalization/OmegaLocalization.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace OmegaLocalization::GUI
{
	inline void SyncGlyphRanges()
	{
		std::vector<OmegaGUI::stGlyphRange> aRanges;
		for (const stGlyphRange& rRange : GlyphRanges())
			aRanges.push_back({ rRange.uFirst, rRange.uLast });
		OmegaGUI::SetGlyphRanges(aRanges);
	}

	inline std::size_t Attach(std::initializer_list<std::reference_wrapper<const stLocaleData>> aLocales = {}, std::string_view strFallback = "en", const std::filesystem::path& pathOverrides = {}, std::string* pError = nullptr)
	{
		SetFallbackLocale(strFallback);
		for (const stLocaleData& rLocale : aLocales)
			LoadLocale(rLocale);
		std::string strErrors;
		std::error_code errCode;
		if (!pathOverrides.empty() && std::filesystem::is_directory(pathOverrides, errCode))
			LoadDirectory(pathOverrides, &strErrors);
		if (pError)
			*pError = strErrors;
		if (!SetLocaleFromSystem())
			SetLocale(strFallback);
		SyncGlyphRanges();
		return Subscribe(SyncGlyphRanges);
	}

	inline bool LanguageSelector(std::string_view strLabel = "language")
	{
		const std::vector<stLocaleInfo> aLocales = GetLocales();
		const std::string_view strCurrent = GetLocale();
		std::string strPreview(strCurrent);
		for (const stLocaleInfo& rLocale : aLocales)
			if (rLocale.strCode == strCurrent)
				strPreview = rLocale.strName;

		bool bChanged = false;
		if (OmegaGUI::BeginCombo(strLabel, strPreview))
		{
			for (const stLocaleInfo& rLocale : aLocales)
			{
				const bool bSelected = rLocale.strCode == strCurrent;
				if (OmegaGUI::Selectable(rLocale.strName + "###" + rLocale.strCode, bSelected) && !bSelected)
					bChanged = SetLocale(rLocale.strCode);
			}
			OmegaGUI::EndCombo();
		}
		return bChanged;
	}
}
