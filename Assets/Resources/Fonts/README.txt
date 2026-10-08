Custom menu fonts

Noto Sans JP Regular (Japanese, Latin and menu symbols).
Unmodified language-specific OTF from the Noto CJK project:
https://github.com/notofonts/noto-cjk
Commit: f8d157532fbfaeda587e826d4cd5b21a49186f7c
Path: Sans/SubsetOTF/JP/NotoSansJP-Regular.otf
SHA-256: dff723ba59d57d136764a04b9b2d03205544f7cd785a711442d6d2d085ac5073

Distributed under the SIL Open Font License 1.1 in LICENSE.txt.
Font copyright/author information is retained in the unmodified font file.

Keep Include Font Data enabled. Sound Room must not depend on fonts installed
on Windows or in a Wine/Proton prefix. The build validates this setting.

Noto Sans SC Regular (Simplified Chinese, Latin and menu symbols).
Unmodified language-specific OTF from the same Noto CJK commit above:
Path: Sans/SubsetOTF/SC/NotoSansSC-Regular.otf
SHA-256: faa6c9df652116dde789d351359f3d7e5d2285a2b2a1f04a2d7244df706d5ea9
Also covered by LICENSE.txt. Used by the independent Custom Menu Language
option, without changing any original arcade texture or race HUD artwork.

Native save-menu subsets are reproducibly baked by
Native/tools/make_custom_menu_fonts.py from these bundled font files and the
shared CustomMenus.txt translation table. Generated atlases retain this license.
