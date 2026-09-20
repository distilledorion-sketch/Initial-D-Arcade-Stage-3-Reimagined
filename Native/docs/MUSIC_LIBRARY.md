# Race music library contract

The native catalog contains 30 race tracks. `src/music_catalog.h` is authoritative;
`data/original_audio/streams/music_catalog.json` is its validated data export for
Claude's Unity music-menu implementation.

The saved `settings.txt` value is a zero-based catalog index. Keep indices 0–12
fixed for the existing 13 Stage 3 tracks. Stage 1 occupies 13–18 and Stage 2
occupies 19–29. Append future tracks; do not sort or insert existing entries.
Use stable IDs such as `stage1.EZ001` for menu identity and resolve them to the
current index. Display sorting can differ from catalog order.

## Data and native APIs

Every JSON track has `index`, `id`, `title`, `sourceStage`, `relativePath`,
`assetPath`, `sampleRate`, `channels`, `frames`, `durationSeconds`, `sha256`,
`bytes`, `titleVerified`, `artist`, and `titleSources`. `relativePath` is relative to
`data/original_audio/streams`; `assetPath` is relative to the native asset root.
`durationSeconds` describes one stored stream pass, including its introduction,
not an infinitely repeating race playback session.

- `idas3::raceMusicCatalog` holds the ordered native entries.
- `idas3::clampMusicTrack(int)` clamps a saved index to the catalog bounds.
- `idas3::findMusicTrack(std::string_view id)` returns an index, or `-1` if absent.
- `EngineAudio::selectMusicTrack(int)` returns `false` for an invalid index and
  leaves selection intact. Selecting the same valid index is a no-op. A change
  during the active race music scene loads the new track from its beginning;
  a load failure restores the previous index and throws.

Selecting a race track in a menu only changes the race selection. It does not
preview the stream or replace the original sequenced menu music. Attract and
result owners retain their own music. The current player's F3 input cycles the
catalog and displays the selected label through the existing native App input
path; the Legend return owner retains its existing input gate.

These are C++ APIs. The JSON export does not create a new C ABI or Unity menu
selection API. The Unity menu implementation must wire its choice to the native
selection and existing settings-save lifecycle on the appropriate owner thread.

## Asset preservation and labels

Stage 1's six files remain unchanged under `streams/stage1`, and Stage 2's eleven
files remain unchanged under `streams/stage2`. Their native sample rates are
44,100 Hz and 29,500 Hz respectively. Playback must honor the stored rates;
the import and exporter perform no audio decoding, conversion or normalization.
Existing Stage 3 files were hash-verified unchanged during import.

All 30 entries now use original song titles, with separate artist credits in
the JSON catalog. Stage 3 also corrects `Black Out` and the full title
`Fall in the Web of Desire`. All stable IDs, native filenames and indices are
unchanged. Display `title` and `artist`; use `sourceStage` when distinguishing
different arcade versions of the same song. Do not display the internal filename
as the song title.

`music_title_metadata.json` records source links and the evidence for each file
mapping. Stage 2 identities are inferred from native course-prefix taxonomy and
Sega's official course/time soundtrack assignments. Stage 1 files were matched
to Stage 2 by decoded PCM correlation; the existing Stage 3 named-file
associations were retained. `titleVerified: true` means the original title has
been checked against the cited soundtrack sources; see the per-file evidence
for the identification method and its limits. The import manifest is a historical
record of extraction and retains its original pre-identification labels.

## Regenerating the export

From the Unity project directory, using Python 3.9 or later:

```powershell
python Tools/Export-MusicCatalog.py
python Tools/Export-MusicCatalog.py --check
```

The script also works from another directory and supports `--project-root`.
It reads explicit C++ entry literals, verifies unique IDs and portable paths,
preserves the first 13 indices, validates SPSD headers and sample/loop bounds,
and checks all imported hashes against `extra_music_manifest.json`. It writes
only `music_catalog.json` after all checks pass. It also checks every title and
ID against `music_title_metadata.json` and exports its artist/source metadata.
`--check` writes nothing.
