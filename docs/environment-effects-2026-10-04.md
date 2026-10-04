# Missing course environment effects

Local follow-up to the report that foliage and sun rays were missing. Work is
on `fixes/discord-bugs-20261004`, separate from the R35 work. It has not been
published or installed over a desktop player.

## Findings and scope

The original course model, separately selected tree placements, spectator
cards and Myogi crows were already connected to race rendering. The original
`model/leaf` and `model/effect/flare` banks were absent from runtime loading.

This change restores roadside leaf debris and the original lens-flare artwork
on base AS3 courses. It does not establish that every report of absent trees
is resolved. The reporter has not yet identified a course or condition.
Imported courses retain their own scenery; AS3 material numbers, sun vectors
and visibility tables are not applied to an unrelated imported course.
There is no recovered volumetric light-shaft renderer in this change.

## Recovered evidence

`Native/tools/extract_original_environment_effects.py` verifies the canonical
source image SHA-256 before exporting its sun positions, lens-piece table and
eight path visibility arrays. Texture formats and scan order come from each
source batch's TCW, with the existing exact texel decode/repack checks.
No CHD or executable image is included in the outputs.

- Leaf loader `0C0D44C0`: original pool capacity 80.
- Wheel admission `0C06A66A`: low material nibble 1, 2 or 11.
- Leaf emitter `0C0D4B60`: chunks 4/5/6 at X offsets 0/+0.1/-0.1.
- Particle initialization `0C020940`: 90-frame lifetime, random orientation
  and X/Z spin amplitudes approximately pi/10 and pi/50.
- Particle step `0C020D40`: 0.95 damping and a floor at the starting height.
- Flare creation `0C17D9EA`: day/dry admission.
- Flare loader `0C180860`, sun vectors `0C28E650`.
- Flare path gating `0C180B00`, pointers `0C337358`, last indices `0C337378`.
- Flare drawing `0C180C60`: forward-facing threshold 0.2, intensity
  `min(1,(0.2+facing)/1.2)*0.8`, lens table entries 1 through 11 at `0C28E5C0`.

The host adapts leaf wake velocity and lens placement to its renderer. These
are not instruction-identical ports of the complete original effect owners.
Visual randomness is private; handling, race timing and shared race RNG are
unchanged. Leaves use a 60 Hz presentation accumulator, 80 slots per car,
expiration, teleport clearing, and pause/network-wait handling. Sun effects
use source path coordinates, including a fresh projection during replay,
and render beneath the HUD only in the main camera.

## Validation

Evidence is under `Verification/foliage-sun-20261004/` (ignored local output).

- `environment_presentation_tests`: 71,826 passing checks covering all source
  path rows in both directions, night/rain exclusion, surface admission,
  expiration, teleport cleanup, opponent pools, pause and identical static
  playback at 30/60/120/144/240 FPS. Real D3D11 leaf/flare captures and the
  portable Unity scene publication path are included.
- `discord_bug_application_tests`: 2,109 passing checks, including an actual
  Myogi race render with sun textures submitted and night/rain suppression.
- `Idas3Unity` compiled successfully. No managed script changes are required.
- Visual captures inspected: leaf cards, facing-sun flare and a Myogi race.
- A second extraction reproduced all six runtime data files byte for byte.
- The existing `Stage-GameData.ps1` recursively stages `Native/data`; the new
  environment folder is included by that existing path.

This is local native/renderer verification, not a new full Unity player build
or an on-hardware check of every course.
