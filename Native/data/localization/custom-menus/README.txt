Custom menu languages

Settings > Gameplay > Custom Menu Language: English / Japanese / Simplified Chinese.
Apply changes the custom menus immediately and saves the preference on this PC.
This is separate from Arcade Text Language. It never redirects an original
menu texture, changes race artwork, alters simulation or translates player data.

The Unity translation table lives in Assets/Resources/Localization/CustomMenus.txt.
The native save screen uses the relevant subset generated into custom_menu_text.h
and these glyph atlases by Native/tools/make_custom_menu_fonts.py. Dependencies:
Python, Pillow, fontTools. Fonts come from the bundled unmodified OFL Noto Sans
JP and SC fonts; provenance and hashes are in Assets/Resources/Fonts/README.txt.
The generated atlases are also covered by the accompanying LICENSE.txt.

Validation: native custom_menu_language_tests renders save and delete screens
and verifies that original mode artwork and saved names stay unchanged.
Idas3CustomMenuLanguageChecks checks settings, rollback, templates and font
packaging. The opt-in -idas3-custom-menu-language-smoke player check captures
the production custom menus and checks glyphs and song identity in all languages.
