# OmegaWare-Localization

A small C++26 localization library for the [OmegaWare Framework](https://github.com/Omega172/OmegaWare-Framework)
and [OmegaWare-GUI](https://github.com/OmegaWareProjects/OmegaWare-GUI). Languages are compiled into the
binary as C++ headers, with optional `.lang` files on disk for translators. Fallback to a base
language, plurals, `std::format` arguments, stable widget IDs across languages and automatic
font glyph ranges so non-Latin scripts render without any manual font setup.

## Adding it to an OmegaGUI menu

```cpp
#include "OmegaLocalization/GUI.hpp"
#include "Locales/Locales.hpp"
using namespace OmegaLocalization::Literals;

OmegaGUI::Initialize(&g_clBackend, hWindow, stConfig);
OmegaLocalization::GUI::Attach({ g_stLocaleEnglish, g_stLocalePolish });

if (OmegaGUI::Begin("omegaware"))
{
	OmegaGUI::Title("menu.title"_t);
	OmegaGUI::Toggle("aim.enabled"_l, &bEnabled);
	OmegaLocalization::GUI::LanguageSelector("settings.language"_l);
}
OmegaGUI::End();
```

That is the whole integration. `Attach` loads the given languages from memory, picks the
Windows UI language (falling back to `en`) and keeps the GUI font atlas in sync with the
glyph ranges the active language needs. Switching languages rebuilds the font on the next
`NewFrame`. The atlas also keeps the scripts of every other loaded language (ranges under 1024
codepoints, plus the characters of each language's own name for larger scripts like CJK), so
the language selector shows every name correctly whichever language is active.

- `"key"_t` returns the translated text. Use it for anything that is only displayed.
- `"key"_l` returns `text###key`. Use it for widget labels: OmegaGUI hashes only the part after
  `###`, so window positions, open tabs and widget state survive a language change.
- A missing key renders as the key itself, so untranslated strings are easy to spot.

## Defining a language

Each language is one header, the same way the Framework's `Locales/` folder works:

```cpp
#pragma once
#include "OmegaLocalization/OmegaLocalization.hpp"

inline const OmegaLocalization::stLocaleData g_stLocalePolish{
	.strCode = "pl",
	.strName = "Polski",
	.aGlyphRanges = { { 0x0100, 0x017F } },
	.aStrings = {
		{ "menu.title", "OmegaWare" },
		{ "aim.enabled", "Włączony" },
		{ "greeting", "Cześć, {}!" },
		{ "items.one", "{} przedmiot" },
		{ "items.few", "{} przedmioty" },
		{ "items.many", "{} przedmiotów" },
	},
};
```

Pass it to `GUI::Attach({ ... })`, or call `OmegaLocalization::LoadLocale(g_stLocalePolish)`
yourself (see `Locales/Locales.hpp` for a `RegisterLocales()` that loads them all).
`aGlyphRanges` lists extra Unicode ranges the font needs; Basic Latin, Latin-1 and the GUI's
symbol ranges are always included.

## Translating from disk

Translators do not need a compiler. Pass a folder as the third argument and every `.lang` file
in it is loaded on top of the built-in languages, overriding their keys or adding new languages:

```cpp
OmegaLocalization::GUI::Attach({ g_stLocaleEnglish, g_stLocalePolish }, "en", "OmegaWare/Locales");
```

A missing folder is ignored. `ExportTemplate("OmegaWare/Locales/fr.lang", "fr")` writes a ready
to fill file with every key, the English text as a comment above each one.

```ini
# comments start with # or ;
@code = pl
@name = Polski
@ranges = 0100-017F, U+2026

aim.enabled = Włączony
padded = "  quoted keeps outer spaces  "
multi = first line\nsecond line
```

Files are UTF-8 (a BOM is fine) and one file can hold several locales, each starting at `@code`.
Once a translation is done it can be turned into a header and compiled in.

## API

| Call | Purpose |
|---|---|
| `LoadLocale(stLocaleData)` | load a language defined in code |
| `AddLocale(code, name, ranges)` / `AddStrings(code, {{key, text}, ...})` | register strings piecemeal |
| `LoadDirectory(path)` / `LoadFile(path)` / `LoadFromMemory(text)` | load `.lang` data, errors report file and line |
| `SetLocale("pl-PL")` | exact match first, then the base language (`pl`) |
| `SetLocaleFromSystem()` | use the Windows user locale |
| `SetFallbackLocale("en")` | where missing keys are looked up (default `en`) |
| `Get(key)` / `GetCString(key)` / `Label(key)` | lookups, the returned views stay valid until that key is reloaded or the fallback locale changes |
| `Format(key, args...)` | `std::format` with the translated pattern |
| `Plural(key, n)` / `FormatPlural(key, n, args...)` | picks `key.one`, `key.few`, `key.many` or `key.other` (`n` is the first format argument) |
| `GetLocales()` / `GetLocale()` / `HasLocale(code)` | enumerate and query |
| `Subscribe(fn)` / `Unsubscribe(handle)` / `Revision()` | react to locale changes |
| `Clear()` | drop all strings (for hot reload); the selected language is re-selected when it is loaded again |
| `MissingKeys(code)` | keys other locales have that this one lacks |
| `ExportTemplate(path, code)` | write a `.lang` file for translators |

Plural rules are built in for English-style languages, French, Polish, Russian, Ukrainian,
Belarusian, Czech, Slovak and the languages without plurals (Japanese, Chinese, Korean, Thai,
Vietnamese, Indonesian).

`OmegaLocalization/GUI.hpp` is header only and is the only file that depends on OmegaGUI, so the
core library works without the GUI.

Locale switches and loads should happen on the render thread (the same thread that calls
`OmegaGUI::NewFrame`).

## Fonts

The default OmegaGUI font is a Windows monospace font (Cascadia Mono, Consolas, ...), which
covers Latin, Greek and Cyrillic. For CJK or other scripts set `stConfig::pathFont` to a font
that contains them; glyph ranges only tell the atlas which glyphs to bake.

## Building

```bat
Build.bat debug x64
Test.bat
```

| Script | Purpose |
|---|---|
| `Build.bat` / `.sh` | configure and build (`mode`, `arch`) |
| `Test.bat` / `.sh` | build and run the tests (`mode`, `arch`) |
| `UpdateCompileCommands.bat` / `.sh` | write `.vscode/compile_commands.json` |

To use it from another xmake project:

```lua
includes("../OmegaWare-Localization/Source/OmegaLocalization/xmake.lua")

target("MyMod")
    add_deps("OmegaGUI", "OmegaLocalization")
```

OmegaWare-GUI does this automatically when this repository sits next to it
(`xmake f --localization=<path>` to point elsewhere, `--localization=` to turn it off); its demo
then has a language selector on the settings tab, accepts `--lang=<code>` and loads overrides
from a `Locales` folder in its working directory.

## Layout

```
Locales/                     example language headers used by the tests and the OmegaGUI demo
Source/
  OmegaLocalization/         the library (static) and GUI.hpp bridge
  Tests/                     test runner
```
