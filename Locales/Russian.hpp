#pragma once
#include "OmegaLocalization/OmegaLocalization.hpp"

inline const OmegaLocalization::stLocaleData g_stLocaleRussian{
	.strCode = "ru",
	.strName = "Русский",
	.aGlyphRanges = { { 0x0400, 0x04FF } },
	.aStrings = {
		{ "settings.title", "настройки" },
		{ "settings.appearance", "внешний вид" },
		{ "settings.theme", "тема" },
		{ "settings.language", "язык" },
		{ "settings.input", "ввод" },
		{ "settings.notifications", "уведомления" },
	},
};
