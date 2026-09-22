# Original driving effects and headlight controls

## Confirmed exhaust flame assets

Arcade Stage 3 HOSTFS contains `model/effect/bkfire/bkfire_pol.tbl`,
`bkfire_pol.bin.nz`, `bkfire_tex.tbl`, and `bkfire_tex.bin.nz`.
There are **nine model chunks and eight 32x32 ARGB4444 textures**.
The model contains 17 batches, 492 submitted vertices and 402 triangles.
Original batch texture-control words are `0x10000000`: twiddled ARGB4444.
The extractor checks every decoded 16-bit texel by repacking it exactly.

In `idas3_main_0C020000.bin` (load address `0x0C020000`), strings
at `0x0C28DF4C` and `0x0C28DF74` name the polygon and texture banks.
The loader at `0x0C17BF66`–`0x0C17BF72` calls `0x0C057920` with
those string pointers, loaded from `0x0C17C068` and `0x0C17C06C`.
Image SHA-256: `efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335`.
These observations establish the original flame assets, not their frame order,
exhaust attachment transform, or complete visual effect controller.
This change extracts the assets; it does not add flame rendering to races.

Reproduce with Python from the project root:

```text
python Native/tools/extract_original_driving_effects.py --hostfs "PATH/TO/HOSTFS" --out "PRIVATE/OUTPUT"
```

The output includes original banks, decoded PNGs, texture packs, separate model
chunks and a manifest with source hashes and per-texture layout evidence.
Original asset bytes remain private; the extraction code can be shared.

## Skid-mark search result: original rubber decal not confirmed

- `model/effect/smoke`: four textures and eight chunks. Texture 1 uses linear
  ARGB4444 (`0x14000000`); the other textures use twiddled ARGB4444. Decoding
  texture 1 as twiddled produces a misleading pattern; it is a smoke cloud.
- `model/effect/rainmark`: three textures and three chunks. These contain wet
  tire trails/spray, not confirmed dry rubber skid marks.
- The original `[tire effect set]` string is at `0x0C24390C`; the race setup
  invokes `0x0C0661A0` before logging it. That owner calls `0x0C13E960`, within
  the smoke implementation. This is evidence for tire effects, not proof of a
  persistent road decal.
- Common/course-common texture banks were inspected too; no dedicated rubber
  skid texture was confirmed. A negative filename search does not prove that
  marks are absent: they could be generated geometry or use a shared texture.
- The current port's `Native/src/main.cpp` contains a procedural `skids` buffer.
  It records wheel positions only under `!originalHandling`, slip > .18 and
  speed > 7, and draws untextured dark quads. Therefore this prototype code does
  not create skid marks in the normal original-handling races. It is not a
  recovered original arcade asset and has not been enabled by this change.

Do not label smoke, rainmark, or the prototype quads as a recovered original
rubber skid decal without tracing their original draw owner.

## Confirmed headlight binding

Settings > Controls > Toggle headlights defaults to **H** and **right-stick
click**. It uses the normal binding mapper and native edge-triggered command.
The existing projected-light, car-light and popup-headlight owners perform the
transition. Race input is gated during menus, pauses and replay playback.
The existing online snapshot and replay records carry the light state.

Controls schema 3 appends the action to old nine-action saves. Existing actions
and controller profiles are preserved; an occupied H/right-stick click remains
assigned to its old action and the conflicting new slot stays unbound. Generic
wheels receive no guessed controller button. Rebinding persists through APPLY.

Verification: 110 binding/migration checks, 59 controller-menu checks, 33 live
headlight checks with nine captures (Akina popup/fixed lamps and imported Enna),
and 278 pause/options checks with an actual Controls screenshot. Inputs in
these tests are injected through the normal mapper; no physical-controller or
two-player headlight visibility test is claimed.

Local proof: `Verification/effects-headlights-20260922`, with canonical extracted
assets under `recovered/`, headlight captures under `run1/`, and menu captures
under `pause/`. Earlier candidate decode images are research, not validated assets.
