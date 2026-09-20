# Original art assets: what exists, what is imported, and what each one is

Surveyed 2026-09-09 against the game's own files under
`driveA/HOSTFS/`. "Imported" means an asset under
`data/original_assets/` names it as its source.

Every function below was read off the extracted textures, not inferred from the
folder name. Where a bank could not be extracted that is stated instead of
guessed.

## Two traps, both of which caught me

**1. Not everything on the disc belongs to this game.** `model/select` holds a
fourteen-entry BGM track list -- Space Boy, Night of Fire, Don't Stop the Music,
Love Is In Danger, Killing My Love, Running in the 90's, Grand Prix, Heartbeat,
Beat of the Rising Sun, Rock Me to the Top, Station to Station, No BGM, Gamble
Rumble, The Race Is Over. **Arcade Stage 3 ships thirteen songs and shares only
one title with that list**, at a different number (Gamble Rumble is 13 there and
01 here). It is a leftover from an earlier Arcade Stage. Chris spotted this;
extracting art proves what a bank *contains*, never that this game *uses* it.
Cross-check against something the game itself ships before treating a bank as
current.

The naming carries the same signal: the **`v3s*` banks are this version's**
(`v3sS04maker`, `v3sS05cars`, `v3sS11mode`, `v3sT01course`, `v3sT02route`,
`v3sT03weather`, `v3sT04time`, ...), and several unprefixed `select*` / `sel*` /
`opt*` banks cover the same screens. Treat an unprefixed bank whose job a `v3s`
bank also does as suspect until something proves otherwise. It is not a blanket
rule -- the name-entry screen genuinely draws `select` and `select0302` beside
`v3sS00common` and `v3sS08name`.

**2. `model/` and `sprite/` share folder names.** `sprite/select` (45 sprites,
used by name entry) is a different asset from `model/select` (the stale track
list). Matching an import to a source by bare name counted six banks as imported
that are not: `ejectcard`, `password`, `select01`, `select04`, `selta`,
`selvswait`. Always compare full source paths.

## Where the art lives

| Directory | Files | Size | Status |
|---|---|---|---|
| `model` | 1534 | 495 MB | **54** of 102 banks imported |
| `font` | 168 | 25 MB | 5 of 42 banks imported |
| `sprite` | 86 | 4.8 MB | **all 21 imported** |
| `envtex` | 60 | 1.5 MB | course environment maps, imported with courses |
| `parts` | 35 | 152 KB | per-car tuning part tables |
| `path` | 102 | 1.6 MB | course path data (not art) |
| `tree` | 20 | 240 KB | roadside tree placement |
| `vtxcolor` | 11 | 2.3 MB | car vertex colour sets |
| `env` | 9 | 120 KB | per-course environment |
| `lod`, `carmdllist` | 2 | 5 KB | one LOD area, one model list |

## The 48 model banks not imported, and what they are

Screens and overlays:

| Bank | What its art shows |
|---|---|
| `adv_intr` | "ACCEPTING BATTLE RACE CHALLENGERS" attract banner |
| `adv_replay` / `adv_replay2` | replay overlay: `DRIVER:`, `BATTLE LEVEL:`, `LIVE` |
| `adv_wait` | large red digits 0-9, the attract wait countdown |
| `cardcheck` / `cardcheck2` | card check screen: colour bars, rank icons, `WIN(S) LOSS(ES) %`, `BATTLE(S)`, `NORMAL`, plus its own A-Z/0-9 font |
| `cardcheck_car` | card check car detail, dense Japanese text plates |
| `cardclean` | single plate, card cleaning prompt |
| `carderror` | card fault messages: "CARDS MAY NOT BE USED AT THIS TIME", "THE MACHINE IS CURRENTLY UNABLE TO ISSUE NEW CARDS", "PLEASE ASK A STORE ATTENDANT..." |
| `interrupt` | "CHALLENGE RECEIVED!" banner with an eye close-up |
| `v3sV01challenge` | **`ACCEPT` / `REFUSE` / `I WIN` / `NOT READY`** -- the challenge prompt |
| `v3sV00wait` | VS wait screen: `A B C course D Lv`, win/loss/rate columns |
| `v3sV10boost` | `RACE 1 2 3 BATTLE` labels |
| `v3sV10loadwait` | load-wait banner |
| `v3sS01card` | card screen, `OK` plate |
| `v3sS09conf` | **extraction fails** (invalid polygon offset table) |
| `v3sS10conf` | control configuration: Wheel / Accel / Brake / Shift labels |
| `v3sT00differase` | time-difference erase screen, with the full course/direction name list |
| `selcrs`, `optcrs`, `optcmn` | course select furniture; `OPTION D` / `NORMAL` badges; `A/B/C/D course` labels |
| `selreceive` | receive screen banners |
| `s_ranking` | ranking board, `DAY/NIGHT` and digit sets |
| `rating` | CERO rating card, "SUITABLE FOR ALL AGES" |
| `ver_msg` | version message banner |
| `gameover_v3` | `GAME OVER` |
| `raceover_v3` | `FINISH!!`, `You Loose...`, `TIME IS UP...` |
| `levelup` | `RACE LEVEL UP`, `BATTLE LEVEL UP`, `BATTLE LEVEL DOWN`, `BUNTA LEVEL UP` |
| `ending` | ending credits: "Thank you for your playing !!", `STAFF` |
| `lecture` + `lecmap_*` | the tutorial screen and its eight course maps (akagi, akina, happo, iroha, myogi, syomaru, tuchizaka, usui) |
| `conquer00`..`08` | nine night-road photographs, one per course -- course conquest imagery |
| `moji` | large Japanese characters |
| `dc2color`, `color_cell` | body-colour selection swatch grids and prompt |
| `hilight` | glow / lens-flare sprites |
| `leaf`, `mangasmoke`, `cupwater`, `effect`, `shadow` | in-world effects: leaves, manga speed-smoke, water, generic effects, shadow set (144 files) |
| `crscmn`, `common`, `navi`, `cardfont_*` | **extraction fails**, see below |

## Texture formats across the whole game

5375 textures live in `model`. **62 are not plain direct-16-bit**, and the flag
byte above the format says which is which:

| flags | Meaning | Count | State |
|---|---|---|---|
| 1, 13 | direct 16-bit, the ordinary case | 5313 | decoded |
| 9 | the 33 car banks and `smoke` | 34 | decoded; already imported |
| 2 | `sdwsub_gc8s5` | 1 | decoded |
| **3** | **vector quantised** | **10** | **now decoded, see below** |
| **7 / format 7** | **8bpp palettised** | **17** | **not decoded, palette is not in HOSTFS** |

### Vector quantised (solved)

`texture_bank.decode_vq` now handles these: a 256-entry codebook of 2x2 blocks
followed by one index byte per block, both in the same twiddled order the direct
path uses, so the payload is exactly `2048 + (w/2)*(h/2)`. All ten decode:

- `navi_df`, `navi_ez`, `navi_hd`, `navi_nm`, `navi_sy`, `navi_tu`, `navi_uh`,
  `navi_vh` -- 512x512 route lines, the in-race course navigation map, one per
  course.
- `select` (256x256) -- **the BGM track list**: Space Boy, Night of Fire, Don't
  Stop the Music, Love Is In Danger, Killing My Love, Running in the 90's, Grand
  Prix, Heartbeat, Beat of the Rising Sun, Rock Me to the Top, Station to
  Station, No BGM, Gamble Rumble, The Race Is Over.
- `select04` (256x256) -- car detail labels: maker, model, name, residence, body
  colour, network record, transmission, points, front spoiler, muffler, bonnet,
  side skirt, mirror, rear bumper, power-up tune, rear spoiler, wheel.

### 8bpp palettised: artwork recovered in structure, colour still blocked

17 textures, one byte per pixel with no palette in the payload:
`loading/load00`-`load09`, `load11`, `load12` (twelve loading paintings that
have never been shown -- `load10` is the odd one out at RGB565, which is why it
is the only one the loading screen can draw), plus `hd_etc_r`, `df_etc_r`,
`ez_etc_r`, `nm_etc_r`, `uh_etc_r`.

**The indices are luminance-ordered, so the paintings are legible without the
palette.** Rendering the index byte straight to grey shows twelve finished
illustrations -- character portraits, cars, night street scenes -- in the same
series as `load10`. Only colour is missing; nothing is corrupt or absent.
`load10` decodes to a full-colour watercolour of two characters with
`initial d version 3` set down the left border, and averages 43/255
saturation, so **these are colour illustrations, not monochrome** -- do not
ship them as greyscale and call it done.

Note the sheets are stored **upside down**, the same as the menu art.

**These are live assets, not leftovers.** All thirteen are named in a
contiguous string table at **0x0c28b6b8** (32 bytes per entry,
`/driveA/model/loading/loadNN`), followed by `loadcmn` and `loadwait`; there is
also a `loadvs` bank in the same directory. This was worth checking given the
Arcade Stage 2 art elsewhere on the disc.

**Where the palette is not.** Four independent searches came back empty:

- **Not a per-bank file.** The bank loader's only filename formats are
  `%s/%s_pol.tbl`, `_pol.bin`, `_tex.tbl`, `_tex.bin` (at 0x0c260a74) -- there
  is no palette suffix. And a 512x512 8bpp payload is exactly 262144 bytes,
  which is the whole file, so nothing is appended.
- **Not a standalone file anywhere in `HOSTFS`**, by size rather than by name:
  no file is a bare 512/1024/2048-byte palette.
- **Not in `idas3_main_0C020000.bin`** as a 256-entry block in any plausible
  encoding. Tested RGB565, ARGB1555, ARGB4444 and ARGB8888, ranking every
  offset by how smooth the ramp is relative to its spread (the correct palette
  for luminance-ordered indices must be a smooth, wide-gamut ramp). Every top
  hit is ASCII text or a table of increasing offsets, and the same addresses
  win under all formats -- the mark of a byte ramp, not a palette.
- **No PVR palette RAM writes.** The image contains no reference to 0x005F9000
  or 0xA05F9000; the only two hits on the PVR register base 0xA05F8000
  (0x0c21b434, 0x0c21b4d4) are cache-line routines.

So the palette is either in a segment other than the one extracted, or built by
code at runtime. **Do not derive a palette by eye; that would be inventing
artwork.** An objective search harness is in
`tools/palette_scan.py` (score = weighted adjacent-colour difference over
window spread) and can be pointed at any new blob.

## The last four groups, now resolved

- **`common`** -- its *polygon* table uses a layout the reader rejects, but its
  textures decode fine: a red disc marker, a small emblem, and two gradient
  strips. A shared miscellaneous bank.
- **`v3sS09conf`** -- **all four files are 0 bytes.** Empty in the game itself,
  not a reader problem.
- **`cardcheck/cardfont_a|h|k`** -- these carry *only* polygon tables and share
  one atlas, `cardfont_tex` (2 x 256x512), which holds katakana and Latin
  glyphs. So they are three layouts (alphabet / hiragana / katakana) over one
  character set, for the card check screen.
- **`font/msg_<rival>` (35) and `font/lecture`** -- solved by the sidecar. A
  font bank carries either a `_spr.tht` (a *text* header, e.g. `alphabet`) or a
  **`_spr.thd`, which is the real table: 12 bytes per cell, dim / flags /
  offset**, and it is the `.thd` that must be read, not the `.tbl`. With that,
  `msg_takumi` reads as 459 uniform 32x32 ARGB4444 cells.

  These are **per-rival dialogue glyph atlases**: punctuation, numerals and
  Latin letters shared as a core, plus rival-specific characters, sized by how
  much that rival says (takumi 237 cells, miki 257, itsuki 277, ryosuke 320,
  bunta_challenge 389; takumi and itsuki share 109 cells byte for byte). The
  remake draws its dialogue from `dialog.idasdialog` through the `alphabet`
  font instead, so these are only needed to reproduce the original's own glyph
  rendering.

**Beware the `.nz` extension.** It does not mean compressed. `read_payload`
only inflates files beginning `NMZIP`; `msg_takumi_spr.bin.nz` starts with raw
`0x0fff` ARGB4444 texels. A payload that "decompresses" to exactly its file
size was never compressed, and a table that seems to overrun it is usually
being read with the wrong record layout.

## Font banks

Imported: `alphabet`, `namekana`, `gamekana`, `gasstand`, and the tuning UI's.

Not imported, with function:

- `kana` (150 sprites), `partskana` (82, katakana for tuning parts),
  `m_s10_font` (36, alphanumeric), `carderror` (39, digits and punctuation),
  `lecture` -- per-purpose glyph sets.
- `msg_<rival>` x35, ~485 KB each with ~300 table entries. Per-rival sprite
  atlases. Consistent with each rival's Japanese dialogue needing its own kanji
  subset; the remake renders the English text from `dialog.idasdialog` with the
  `alphabet` font, so these are not needed unless Japanese text is wanted.

## Reproducing this

The survey used `tools/extract_original_models.py` and `tools/texture_bank.py`
directly against `HOSTFS/model` and `HOSTFS/font`, writing PNGs to a scratch
directory and reading them. Nothing here was imported into `data/`.
