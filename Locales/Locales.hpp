#pragma once
#include "English.hpp"
#include "German.hpp"
#include "Polish.hpp"
#include "Russian.hpp"

inline void RegisterLocales()
{
	OmegaLocalization::LoadLocale(g_stLocaleEnglish);
	OmegaLocalization::LoadLocale(g_stLocaleGerman);
	OmegaLocalization::LoadLocale(g_stLocalePolish);
	OmegaLocalization::LoadLocale(g_stLocaleRussian);
}
