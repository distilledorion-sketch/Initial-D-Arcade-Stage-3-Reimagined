# The game's files: what exists, what the remake uses, what is still on the table

Surveyed 2026-09-09 across every file the disc carries, not just art. For art
specifically see `ART_ASSETS.md`.

## The extraction is provably complete

`game files/.idas3_extraction_complete.json` lists **2196 files** with sizes and
SHA-256s, from `GDS-0033 + 317-0384-COM`. Checked all of them:

- 2196 present, **0 missing, 0 size mismatches**
- **0 files on disk the manifest does not list**

So nothing is missing from the extraction. Everything below is about what the
remake does not yet *use*.

## Camera tables: format solved, purpose still open

**19 files, ~1.2 MB, referenced by nothing in the remake.** Each is exactly
64 KB. `tools/camera_tables.py` parses all of them; use it rather than redoing
this by hand.

### Container

    u32 count
    count times:
        u32 size            # payload bytes, always a multiple of 4
        u8  payload[size]   # payload[0..4) is the record kind

**All 19 files parse cleanly to exactly `count` records** with no leftover.
Earlier attempts failed because the records are variable length and a fixed
stride was assumed; the size prefix is what makes it work.

Record sizes are a fixed function of kind: kind 0 -> 44, 1 -> 40, 2 -> 68,
3 -> 48, 4 -> 52, 5 -> 72, 6 -> 52 or 60, 8 -> 68, 9 -> 72, 10 -> 76, 11 -> 80,
12 -> 84, 13 -> 80.

### Record prefix, and exactly how far it is proven

Over all 819 records:

| Word | Meaning | Evidence |
|---|---|---|
| w0 | kind | -- |
| w1 | smoothJoint | always 0 or 1; 1 in only 6 records |
| w2 | isShake | always 0 or 1; 1 in 104 records |
| w3..w5 | world position | true for every kind **except 0 and 6**, whose values here never exceed 12 and so are parameters, not coordinates |
| w6 | field of view | always in FOV range; **pi/3 in 360 of 819 records** |
| w7 | hold seconds | non-negative (0.2..28) in kinds 2,5,6,9,10,12,13; negative in 0,1,3,4,8,11, so it means something else there |

**Kind 5 (163 records) is byte-for-byte the 18-word `OriginalStartCameraShot`**
-- kind, smoothJoint, isShake, position, angle, wait, rotation, shake, offset,
objectSize. **Kind 13 is that struct plus two words**; `o_ending_camera_00` is a
single kind-13 record at (2553.65, 2116.26, -3945.27), fov exactly pi/3, hold
14.10 s, shake on. Fields past w7 for the other kinds are **not** decoded.

The hold-time split above is an observation about the data, not a proven
taxonomy: kind 6 sits on the non-negative side yet carries no position.

### How the game picks a file

The selector is **0x0c192280**, and it is simply:

    if (course == 9 && direction == 0) return table[18];   // the ending camera
    if (course > 8)     course = 0;
    if (direction > 1)  direction = 0;
    return table[course * 2 + direction];

The pointer table is at **0x0c33b920** (its one reference is the literal at
0x0c1922cc) and resolves to:

- `[0..15]` courses 0-7, directions 0/1 -- twelve `y_camdata_NN` then
  `o_camdata_60/61/70/71`
- `[16][17]` **course 8 (Akina Snow) reuses Akina's `y_camdata_30/31`**, which
  is why no `_80/_81` file exists
- `[18]` `o_ending_camera_00`, reachable only through the `course == 9` case
- `[19]` null terminator

**`o_camadv` and `o_camdata_30` are not reachable through this table.**
`o_camadv` has its own string reference at 0x0c262438; `o_camdata_30` has none
at all and is an orphaned duplicate of Akina.

### Provenance: these really are this game's courses

Checked, because the disc also carries Arcade Stage 2 art (see
`ART_ASSETS.md`). Every file's camera positions were tested against the
imported course paths in `data/courses`: each one best-fits a real course, in
the remake's own course order, with 78-100% of its cameras inside that course's
bounding box -- Myogi, Usui, Akagi, Akina, Happogahara, Irohazaka for `y_`, and
Shomaru, Tsuchisaka for `o_`. This is not foreign data.

### What is NOT established

- **What the system is for.** Positions march along each course with hold times
  and shake, and the image carries `iRaceCamera`, `CCarCamera`, `CSampleCamera`,
  `krCameraLookAt` RTTI names plus a "Contorol Camera Mode" debug string, which
  points at a race/replay camera. That is a reading of the evidence, not proof.
- **The meaning of kinds 0, 1, 3, 4, 8, 11**, and every field past w7.
- **These are not the start-line camera table.** Decoding all 163 kind-5
  records and comparing against `original_start_camera_data.inc` gives **0 of 36
  matches**. An earlier note in this file guessed these tables were the likely
  home of the VS screen's unrecovered kind-5 shot; that guess is now disproven.

### Notes for whoever continues

The `y_` names are also built at runtime by the format string
`/driveA/binary/y_camdata_%1d%1d.bin` at 0x0c26de0c (via sprintf 0x0c226980,
load 0x0c04e480, buffer pointer stored at +56 of the context at 0x0c31c99c) --
that is a second, dev-style path that omits the `.nz`. The shipping path is the
pointer table above. Capstone disassembles this image with `CS_ARCH_SH` /
`CS_MODE_SH4 | CS_MODE_LITTLE_ENDIAN` at base 0x0C020000.

Search these files by float **tolerance**, never exact bits: an exact search for
the start camera's pi/3 word returns nothing across all 19 files because they
differ in the last mantissa bit.

## Other data the remake never reads

| Directory | Files | Size | What it is |
|---|---|---|---|
| `envtex` | 60 | 1.5 MB | per-course environment sets: `h_eff_env_<course>_<day/night>_<b,l,r,t>`, so day and night with bottom/left/right/top faces -- the shape of a reflection environment. **Format unresolved**: faces differ in size (16384, 24576, 65536) and 24576 is not a power-of-two shape at 16bpp, and decoding as twiddled 565/1555/4444 at the implied dimensions produces noise. They are not plain direct textures |
| `tree` | 20 | 240 KB | roadside tree placement, per course and side |
| `vtxcolor` | 11 | 2.3 MB | per-colour vertex sets (blue, crimson, cyan, green, orange, purple, red, spa, spb, white, yellow). The remake's car colours were recovered from the game image into `original_models/<car>/appearance_v2/color_NN` instead, so this is an independent second source worth comparing against |
| `env` | 9 | 120 KB | per-course environment |
| `binary/j_env_dot128`, `dot256`, `select128_b` | 3 | 200 KB | environment dot textures |
| `lod/k_ez_lodarea` | 1 | 1 KB | one course's LOD areas |
| `carmdllist/test_trueno` | 1 | 4 KB | a single car model list |

## Data the remake does use

`binary/*_colli_*` (course collision, 22 references), `binary/PATH_*` and
`path/` (course paths), `binary/k_light_test*`, `binary/AICADRV.bin` (the
offline score runs), `binary/o_advcg` and `o_camadv`, `parts/` (per-car tuning
parts, 19 references), and the model/sprite/font/sound banks covered in
`ART_ASSETS.md`.

## Audio surplus

The cue table names 37 music banks; the disc carries 39. **`PANIC` and
`TAKUMI2`** are complete, valid DTPK banks -- proper sample tables and their own
sequence data -- that nothing references. Nothing referenced is missing.
Streamed audio is exactly the 13 songs plus five jingles, with no foreign
material. See `ART_ASSETS.md` for the caution about content from earlier
Arcade Stages.

## Accounting, at the level that means something

Count **banks**, not files. A bank of four files is named once by its bank name,
so a per-file tally reports `sprite` as 86 of 86 unreferenced when all 21 of its
banks are imported. That metric is noise; this is the real picture:

| Area | Used | Not used |
|---|---|---|
| `model` banks | 54 | 48, all identified (`ART_ASSETS.md`) |
| `font` banks | 5 | 37, all identified |
| `sprite` banks | 21 | 0 |
| music banks | 37 | 2 (`PANIC`, `TAKUMI2`) |
| streamed audio | 18 | 0 |
| `binary` | collision, `PATH_*`, light tests, `AICADRV`, `o_advcg`, `o_camadv` | **19 camera tables**, `j_env_dot*` |
| `parts`, `path` | yes | -- |
| `envtex`, `tree`, `vtxcolor`, `env`, `lod`, `carmdllist` | none | all |

## Elsewhere in `game files/`

- `gds-0033.chd` (237 MB) -- the source GD-ROM image the manifest was taken
  from. Already fully extracted, per the check above.
- `idas3_main_0C020000.bin` (4 MB) -- one loaded segment of the game binary.
  This is where the **8bpp texture palettes** must live; they are not in
  `HOSTFS`. It is also only a segment, so absence from it does not prove a
  string is unused.
- `317-0384-com.pic`, `initdv3e.zip` -- security PIC and a small archive; not
  content.
