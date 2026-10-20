#pragma once
#include "OmegaLocalization/OmegaLocalization.hpp"

inline const OmegaLocalization::stLocaleData g_stLocaleEnglish{
	.strCode = "en",
	.strName = "English",
	.aStrings = {
		{ "settings.title", "settings" },
		{ "settings.appearance", "appearance" },
		{ "settings.theme", "theme" },
		{ "settings.language", "language" },
		{ "settings.input", "input" },
		{ "settings.notifications", "notifications" },
		{ "settings.strings", "{} of {} strings translated" },
	},
};
