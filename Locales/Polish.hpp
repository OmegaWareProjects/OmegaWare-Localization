#pragma once
#include "OmegaLocalization/OmegaLocalization.hpp"

inline const OmegaLocalization::stLocaleData g_stLocalePolish{
	.strCode = "pl",
	.strName = "Polski",
	.aGlyphRanges = { { 0x0100, 0x017F } },
	.aStrings = {
		{ "settings.title", "ustawienia" },
		{ "settings.appearance", "wygląd" },
		{ "settings.theme", "motyw" },
		{ "settings.language", "język" },
		{ "settings.input", "sterowanie" },
		{ "settings.notifications", "powiadomienia" },
		{ "settings.strings", "przetłumaczono {} z {} tekstów" },
	},
};
