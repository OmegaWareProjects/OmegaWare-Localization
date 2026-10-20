#include "OmegaLocalization/OmegaLocalization.hpp"
#include "Locales/Locales.hpp"

#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <source_location>
#include <string>

namespace
{
	int g_iFailures = 0;
	int g_iChecks = 0;

	void Check(bool bCondition, std::string_view strName, const std::source_location stLocation = std::source_location::current())
	{
		++g_iChecks;
		if (bCondition)
			return;
		++g_iFailures;
		std::printf("[FAIL] %.*s (line %u)\n", static_cast<int>(strName.size()), strName.data(), static_cast<unsigned>(stLocation.line()));
	}

	void CheckEqual(std::string_view strActual, std::string_view strExpected, std::string_view strName, const std::source_location stLocation = std::source_location::current())
	{
		++g_iChecks;
		if (strActual == strExpected)
			return;
		++g_iFailures;
		std::printf("[FAIL] %.*s (line %u): got '%.*s', expected '%.*s'\n", static_cast<int>(strName.size()), strName.data(), static_cast<unsigned>(stLocation.line()),
			static_cast<int>(strActual.size()), strActual.data(), static_cast<int>(strExpected.size()), strExpected.data());
	}

	constexpr std::string_view strEnglish = R"(# comment
@code = en
@name = English
menu.title = OmegaWare
aim.enabled = Enabled
greeting = Hello, {}!
items.one = {} item
items.other = {} items
padded = "  spaced  "
multi = line one\nline two
)";

	constexpr std::string_view strPolish = "\xEF\xBB\xBF@code = pl\n@name = Polski\n@ranges = 0100-017F, U+2026\naim.enabled = W\xC5\x82\xC4\x85\x63zony\nitems.one = {} przedmiot\nitems.few = {} przedmioty\nitems.many = {} przedmiot\xC3\xB3w\n";
}

int main()
{
	using namespace OmegaLocalization;
	using namespace OmegaLocalization::Literals;

	CheckEqual(Get("missing.before.load"), "missing.before.load", "unknown key returns the key itself");

	std::string strError;
	Check(LoadFromMemory(strEnglish, &strError), "english parses");
	CheckEqual(strError, "", "english has no errors");
	Check(LoadFromMemory(strPolish, &strError), "polish parses with BOM");

	CheckEqual(GetLocale(), "en", "fallback locale is active by default");
	CheckEqual(Get("menu.title"), "OmegaWare", "basic lookup");
	CheckEqual("aim.enabled"_t, "Enabled", "_t literal");
	CheckEqual("aim.enabled"_l, "Enabled###aim.enabled", "_l literal builds a stable-id label");
	CheckEqual(Get("padded"), "  spaced  ", "quoted values keep spaces");
	CheckEqual(Get("multi"), "line one\nline two", "escapes are decoded");
	CheckEqual(Format("greeting", "Omega"), "Hello, Omega!", "format substitutes arguments");
	CheckEqual(FormatPlural("items", 1), "1 item", "english plural one");
	CheckEqual(FormatPlural("items", 5), "5 items", "english plural other");

	int iNotifications = 0;
	const std::size_t uHandle = Subscribe([&] { ++iNotifications; });
	const std::uint64_t uRevision = Revision();
	Check(SetLocale("pl-PL"), "region code falls back to base language");
	CheckEqual(GetLocale(), "pl", "polish active");
	Check(iNotifications == 1, "subscribers are notified on locale change");
	Check(Revision() > uRevision, "revision increases on locale change");
	CheckEqual(Get("aim.enabled"), "W\xC5\x82\xC4\x85\x63zony", "utf-8 polish string");
	CheckEqual("aim.enabled"_l, "W\xC5\x82\xC4\x85\x63zony###aim.enabled", "label id stays the same across locales");
	CheckEqual(Get("menu.title"), "OmegaWare", "missing polish key falls back to english");
	CheckEqual(FormatPlural("items", 1), "1 przedmiot", "polish one");
	CheckEqual(FormatPlural("items", 3), "3 przedmioty", "polish few");
	CheckEqual(FormatPlural("items", 12), "12 przedmiot\xC3\xB3w", "polish many for teens");
	CheckEqual(FormatPlural("items", 22), "22 przedmioty", "polish few for 22");
	CheckEqual(FormatPlural("items", 25), "25 przedmiot\xC3\xB3w", "polish many for 25");

	const auto aRanges = GlyphRanges();
	Check(std::ranges::find(aRanges, stGlyphRange{ 0x0100, 0x017F }) != aRanges.end(), "polish glyph range reported");
	Check(std::ranges::find(aRanges, stGlyphRange{ 0x2026, 0x2026 }) != aRanges.end(), "single codepoint range reported");

	const std::array<stGlyphRange, 1> aCyrillic = { { { 0x0400, 0x04FF } } };
	const std::array<stGlyphRange, 1> aCjk = { { { 0x4E00, 0x9FFF } } };
	Check(AddLocale("ru", "\xD0\xA0\xD1\x83\xD1\x81\xD1\x81\xD0\xBA\xD0\xB8\xD0\xB9", aCyrillic), "russian locale added");
	Check(AddLocale("zh", "\xE4\xB8\xAD\xE6\x96\x87", aCjk), "chinese locale added");
	const auto aOtherRanges = GlyphRanges();
	Check(std::ranges::find(aOtherRanges, stGlyphRange{ 0x0400, 0x04FF }) != aOtherRanges.end(), "other locales' small scripts stay in the atlas so their names render");
	Check(std::ranges::find(aOtherRanges, stGlyphRange{ 0x4E00, 0x9FFF }) == aOtherRanges.end(), "a large script is not added for an inactive locale");
	Check(std::ranges::find(aOtherRanges, stGlyphRange{ 0x4E2D, 0x4E2D }) != aOtherRanges.end() && std::ranges::find(aOtherRanges, stGlyphRange{ 0x6587, 0x6587 }) != aOtherRanges.end(), "an inactive locale with a large script still gets the characters of its name");

	Check(PluralCategory("ru", 21) == EPluralCategory::One, "russian 21 is one");
	Check(PluralCategory("ru", 11) == EPluralCategory::Many, "russian 11 is many");
	Check(PluralCategory("fr", 0) == EPluralCategory::One, "french 0 is one");
	Check(PluralCategory("ja", 1) == EPluralCategory::Other, "japanese has no plural");

	const auto aMissing = MissingKeys("pl");
	Check(std::ranges::find(aMissing, "menu.title") != aMissing.end(), "missing keys lists untranslated key");
	Check(std::ranges::find(aMissing, "aim.enabled") == aMissing.end(), "missing keys skips translated key");

	const std::filesystem::path pathTemp = std::filesystem::temp_directory_path() / "OmegaLocalizationTests";
	std::filesystem::create_directories(pathTemp);
	Check(ExportTemplate(pathTemp / "pl.lang", "pl"), "template export");
	Clear();
	CheckEqual(Get("aim.enabled"), "aim.enabled", "clear removes strings");
	Check(LoadFile(pathTemp / "pl.lang", &strError), "exported template loads back");
	CheckEqual(Get("aim.enabled"), "W\xC5\x82\xC4\x85\x63zony", "round trip keeps translations");
	Check(HasLocale("pl"), "round trip keeps locale");
	CheckEqual(GetLocale(), "pl", "clear and reload keeps the selected locale");

	{
		std::ofstream(pathTemp / "de.lang") << "@code = de\n@name = Deutsch\naim.enabled = Aktiviert\n";
		std::ofstream(pathTemp / "broken.lang") << "@code = xx\nthis line is broken\n";
		std::ofstream(pathTemp / "ignored.txt") << "@code = zz\n";
		const std::size_t uLoaded = LoadDirectory(pathTemp, &strError);
		Check(uLoaded == 2, "directory loads valid .lang files only");
		Check(strError.find("broken.lang") != std::string::npos && strError.find("line 2") != std::string::npos, "directory reports file and line of errors");
		Check(HasLocale("de") && !HasLocale("zz"), "only .lang files are read");
	}
	Check(SetLocale("de"), "switch to german");
	CheckEqual("aim.enabled"_t, "Aktiviert", "german string");
	Check(!SetLocale("klingon"), "unknown locale is rejected");
	CheckEqual(GetLocale(), "de", "rejected switch keeps current locale");

	std::string strBadError;
	Check(!LoadFromMemory("key = value\n", &strBadError), "strings before @code are an error");
	Check(!LoadFromMemory("@code = en\n@ranges = nope\n", &strBadError), "bad ranges are an error");

	const int iBeforeUnsubscribe = iNotifications;
	Unsubscribe(uHandle);
	SetLocale("pl");
	Check(iNotifications == iBeforeUnsubscribe, "unsubscribed callback is not called again");
	std::filesystem::remove_all(pathTemp);

	Clear();
	RegisterLocales();
	CheckEqual(GetLocale(), "pl", "in-memory reload keeps the selected locale");
	Check(GetLocales().size() == 4, "all in-memory locales load");
	Check(SetLocale("ru"), "in-memory russian is available");
	CheckEqual(Get("settings.title"), "\xD0\xBD\xD0\xB0\xD1\x81\xD1\x82\xD1\x80\xD0\xBE\xD0\xB9\xD0\xBA\xD0\xB8", "in-memory utf-8 survives");
	const auto aMemoryRanges = GlyphRanges();
	Check(std::ranges::find(aMemoryRanges, stGlyphRange{ 0x0400, 0x04FF }) != aMemoryRanges.end(), "in-memory glyph ranges reported");
	CheckEqual(Format("settings.strings", 3, 7), "3 of 7 strings translated", "missing in-memory key falls back to english");
	Check(LoadFromMemory("@code = ru\nsettings.title = override\n"), "disk style override loads over in-memory locale");
	CheckEqual(Get("settings.title"), "override", "override wins over in-memory text");
	CheckEqual(Get("settings.theme"), "\xD1\x82\xD0\xB5\xD0\xBC\xD0\xB0", "override keeps untouched in-memory keys");

	std::printf("%d checks, %d failure(s)\n", g_iChecks, g_iFailures);
	return g_iFailures == 0 ? 0 : 1;
}
