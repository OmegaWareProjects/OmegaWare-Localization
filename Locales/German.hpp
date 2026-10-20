#pragma once
#include "OmegaLocalization/OmegaLocalization.hpp"

inline const OmegaLocalization::stLocaleData g_stLocaleGerman{
	.strCode = "de",
	.strName = "Deutsch",
	.aStrings = {
		{ "settings.title", "einstellungen" },
		{ "settings.appearance", "darstellung" },
		{ "settings.theme", "thema" },
		{ "settings.language", "sprache" },
		{ "settings.input", "eingabe" },
		{ "settings.notifications", "benachrichtigungen" },
		{ "settings.strings", "{} von {} texten übersetzt" },
	},
};
