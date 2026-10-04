# Exhaust backfire lighting

Restores the light pulse accompanying the existing two-frame tuned Evo III
exhaust flame. Both the local player and confirmed online opponent events
illuminate the source course/car light scopes, including the mirror view.

## Original evidence

`original_backfire_lighting_tests` executes the canonical AS3 image in the
existing bounded SH-4 reference fixture. Source point constructor `054660`,
complete effect update/draw owner `17CB00`, and the original ELAN converter run;
random-number input and geometry submission are explicit test boundaries.

The light has RGB `(1, .8, .4)`, near/far attenuation distances `.001/7.45`,
and fixed car-local position `(.788, .227, -3.156)` for all three exhausts.
Source `17CB74..80` supplies the distances, `17CB9E..B2` supplies the anchor,
and `17CC80..9A` supplies the active colour. The source converter produces
attenuation words `3F94BE1F / 00003F80`. The inactive branch clears RGB;
the native presentation omits the inactive light entirely.

The position uses the published ACar visual world matrix, matching the
original `17CC16..26` point transformation. It does not inherit the separate
body-only road/ride displacement or the flame mesh's placement correction.

## Host integration

Rendering copies the current course and car light sets, then appends active
backfire lights. Base sets, headlights, ambient values and shadow gain are
unchanged. Source light-array capacity remains 16; a full scope rejects an
additional light without overwriting another light or exceeding its bounds.

The existing accepted local audio event and confirmed remote event supply
the two-frame state at 60 Hz. Rendering adds no RNG calls, sound events,
network messages or physics changes. Repaints do not advance or extend the
pulse, pausing holds its frame, and expiry removes its illumination.

The pulse uses the existing original-light shader path. It does not add a
second Unity light pass, baked illumination, dynamic shadow maps or global
screen brightening. Unlit/baked materials retain their original behaviour.
Replay timing and original random flame-length variation remain separate
unfinished work; playback does not invent unrecorded backfire events.

## Validation

- 1,152 source-owner cases, 26,496 comparisons, 9,314,889 original instructions:
  all three exhausts, both active frames and the inactive frame, random world
  transforms, and exact main/mirror hardware lighting packets.
- 2,875 actual game host checks cover local and confirmed remote pulses, repeated
  draws, unchanged simulation digest, expiry, hidden effects, replay
  suppression, full light arrays, and simultaneous lights on all nine base
  courses with each day/night/dry/wet combination.
- The Unity scene publication test checks warm RGB and attenuation in both
  course and car scopes for main and mirror views.
- Night-race before/on/expired GPU captures are retained under
  `Verification/discord-bugs-20261004/backfire-light-host-final`.
  The light changes 29,739 rear-body pixels outside the flame's bounds;
  the expired capture is byte-identical to the unlit baseline.

No public release or leaderboard deployment is included in this change.
