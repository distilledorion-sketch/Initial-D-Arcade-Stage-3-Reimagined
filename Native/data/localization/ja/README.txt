Japanese arcade text artwork

Source: the supplied Initial D Arcade Stage 3 Japan Rev C (GDS-0032C) UI banks.
This package contains mapped UI models and decoded textures only. It does not
contain a disc image, game program, decryption keys, cars, courses or audio.

Select Settings > Gameplay > Arcade Text Language > Japanese, Apply, then
restart. English is the default. Replay playback uses the same saved setting.
Language is fixed before native menus preload; changing the setting during
play takes effect on the next launch.

The 45 banks cover original selection screens, race labels, results, tuning,
continue prompts and several attract/loading screens. Original Japanese
artwork sometimes still uses English words. Custom remake menus have a separate
Custom Menu Language option (English, Japanese, Simplified Chinese). Custom
track names, dialogue scripts and the input keyboard retain their existing
presentation. This artwork package is not a complete text translation.

The original English chunk IDs and textures are retained. Japanese artwork
is explicitly mapped to those IDs, fitted to their anchors without changing
aspect ratio, and uses appended texture slots. Unmapped content falls back
to English. The manifest records the source mapping for every changed chunk.

Rebuild with Native/tools/import_japanese_ui.py and an exported Japanese UI
directory. Original raw disc/program files belong outside distributable data.
Native japanese_ui_tests verifies all mapped banks, unchanged fallback pixels,
chunk/texture bounds and native composition. Idas3JapaneseUiChecks checks
settings persistence. The isolated player flag -idas3-scene-language 0 or 1
along with -idas3-scene-smoke <new directory> checks the actual menu flow.
