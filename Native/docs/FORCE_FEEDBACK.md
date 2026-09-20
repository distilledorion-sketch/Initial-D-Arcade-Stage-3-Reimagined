# The cabinet's force-feedback board, and what the game sends it

Everything here was read out of `idas3_main_0C020000.bin` (mapped at `0x0C020000`)
with `Native/tools/sh4_disasm.py`, which unlike capstone decodes the SH-4 FPU
group and the `FPUL`/`FPSCR` transfers — without those the whole owner reads as
`.word`.

```
python Native/tools/sh4_disasm.py <image> 0x0C15A5E0 0x0C15AAD0 -n
```

Two halves, and they are kept apart on purpose:

| half | arcade | remake |
|---|---|---|
| the board: decode, scaling, effects | a MIDI device on the AICA port | `src/original_ffb.cpp`, a command-for-command port of Flycast's `midiffb.cpp` |
| the owner: what commands to send | SH-4 code at `0C15A5E0` | `src/original_ffb_owner.cpp` |

`Idas3WheelUpdateCabinet` in `src/wheel_feedback_backend.cpp` runs one frame of
the owner, reads the board's snapshot, and plays it through DirectInput. Unity
sends telemetry and never a force.

## The wire

The game writes 4-byte packets into a 64-byte ring at `0C901690` (write index
`0C90171C`, pending count `0C901714`). The service routine at `0C15AB40` drains
that ring into `MOBUF` (`0xA070280C`) and reads replies out of `MIBUF`
(`0xA0702808`, reached through the pointer table at `0C31FE90`).

The encoder is `0C15AC80`. Given three bytes it masks each to seven bits, XORs
them into a checksum, sets bit 7 of the first, and appends the checksum:

```c
p[0] = (cmd & 0x7F) | 0x80;
p[1] =  a  & 0x7F;
p[2] =  b  & 0x7F;
p[3] = ((cmd & 0x7F) ^ p[1] ^ p[2]) & 0x7F;
```

Replies carry the 14-bit wheel encoder position, `((b1 & 0x7F) << 7) | (b2 & 0x7F)`,
centre 8192, stored at `0C901688` (`0C15ABBC..0C15ABD0`).

## The commands

Sixteen one-command wrappers sit at `0x40` stride from `0C159B40`:

| address | packet | meaning | called? |
|---|---|---|---|
| `0C159B40` | `{0x7C, 0, 63}` | — | yes |
| `0C159B80` | `{0x7D, 0, 0}` | — | yes |
| `0C159BC0` | `{0x7F, 0, 0}` | reset / calibrate | yes |
| `0C159C00` | `{0x00, 0, 1}` | enable on | yes |
| `0C159C40` | `{0x00, a!=0, b!=0}` | enable(a,b) | yes |
| `0C159C80` | `{0x01, a, b}` | force limit | yes |
| `0C159CC0` | `{0x02, a, b}` | — | yes |
| `0C159D00` | `{0x03, a, b}` | drive power, `(a>>3)/15` | yes |
| `0C159D40` | `{0x04, v>>7, v}` | torque, 14-bit, centre `0x80` | yes |
| `0C159D80` | `{0x05, a, b}` | rumble (half-Hz, amplitude) | yes |
| `0C159DC0` | `{0x06, a, b}` | damper (strength, pole) | yes |
| `0C159E00` | `{0x70, 0, 0}` | — | **no** |
| `0C159E40` | `{0x07, v>>7, v}` | — | **no** |
| `0C159E80` | `{0x0B, a, b}` | spring | **no** |
| `0C159EC0` | `{0x0D, (b&7)\|(a?0:0x40), c&3}` | — | **no** |
| `0C159F00` | `{0x7A, 0, a}` | — | yes |

**The arcade never sends a spring command.** An image-wide scan for literal-pool
references to `0C159E80` finds none, and the same for `0x07`, `0x0D` and `0x70`.
All self-centring is carried by the constant force below. Adding a spring to the
remake would double the centring, which is why `original_ffb_owner.cpp` does not.

`0C159F40` resets the module: it zeroes `0CAA9748/974C/9758/9760` and
`0C901714/171C`, sets `0CAA9750 = -1` and `0CAA9754 = 0x0080`, the torque centre.

## The per-frame owner, `0C15A5E0 .. 0C15AAD0`

State base is `r10 = 0x0C900F00`. Caches: `0CAA974C` frame counter (saturates at
256), `0CAA9754` last torque, `0CAA9758` last damper pole, `0CAA975C` rumble
latch. Every send is gated on its cache.

### Torque, command 4

```c
grip = 1.0f - F(state + 0x27C);                       // 0C15A61A..0C15A624

// sign from one pair of fields, magnitude from another, bounded at 40
if (F(state+0x3B4) > F(state+0x3B8)) {                // 0C15A62C
    d = F(state+0x3C4) - F(state+0x3C8);              // 0C15A636..0C15A63C
    road = d < 0 ? 0 : (d > 1 ? 40 : (int)(d * 40.0f));
} else {
    d = F(state+0x3C8) - F(state+0x3C4);              // 0C15A664..0C15A66A
    road = d < 0 ? 0 : (d > 1 ? -40 : -(int)(d * 40.0f));
}
// A four-way jump table at 0C15A68A REPLACES `road` when state+0x150 is
// non-zero and state+0x154 <= 3. Its displacement bytes at 0C15A6AC are
// 54 04 54 04, so there are only two bodies: modes 0 and 2 -> 0C15A700,
// modes 1 and 3 -> 0C15A6B0, identical but for a negation.
if (I(state+0x150) && (unsigned)I(state+0x154) <= 3) {
    int sum  = carLimit[*0x0C901654] + strengthLimitB[*0x0C9015EC];
    float f  = F(state + 0x158) * 4.0f * (float)sum;      // 0C15A6B0..0C15A6D6
    road = (I(state+0x154) & 1) ? -(int)f : (int)f;       // 0C15A6DE negates
    const int bound = I(state+0x438) ? 16 : 24;           // 0C15A6E0..0C15A78E
    road = clamp(road, -bound, bound);
}

// the dominant term: a centring spring expressed as a constant force
v = road + 0x80 - (int)(200.0f * F(state + 0x1CC) * grip);   // 0C15A790..0C15A7AA

limit = carLimit[*0x0C901654]                         // 0C31FE04, 35 entries
      + strengthLimitB[*0x0C9015EC]                   // 0C31FDD8, 11 entries
      + strengthLimitA[*0x0C9015D8];                  // 0C31FD54, 11 entries
limit = clamp(limit, 1, 127);                         // 0C15A7C4..0C15A7D2
if (I(state+0x434)) limit = (int)(limit / 1.5f);      // 0C15A7D8..0C15A7EA
if (I(state+0x438)) limit = (int)(limit * 0.5f);      // 0C15A7EC..0C15A7FE
if (I(state+0x140)) limit >>= 2;                      // 0C15A804..0C15A80E
limit -= (int)((float)(int)(limit*0.9f) * (F(state+0x24C)+1.0f) * 0.5f);
limit = clamp(limit, 1, 127);                         // 0C15A834..0C15A83E

v = clamp(v, 0x80 - limit, 0x80 + limit);             // 0C15A840..0C15A854
half = I(0x0CAA974C) >> 1;                            // the soft start
v = clamp(v, 0x80 - half, 0x80 + half);               // 0C15A856..0C15A86C
if (v != I(0x0CAA9754)) send{0x04, v>>7, v};          // 0C15A870..0C15A87A
I(0x0CAA9754) = v;
```

The soft start is the detail that makes a cabinet feel like a cabinet: for the
first 254 frames of a race the wheel cannot reach full authority, so it eases in
instead of snapping. `F(state+0x24C)`'s neutral value is exactly `-1.0f`, which
makes the trim term vanish for every limit.

### Damper, command 6

```c
grip = 1.0f - F(state + 0x27C);                       // recomputed, 0C15A886..0C15A88C
pole = (int)( damperRate[*0x0C9015D8]                 // 0C31FD80, 11 floats
            * grip * grip
            * damperScale[*0x0C9015EC] );             // 0C31FDAC, 11 floats
if (I(state+0x438)) pole >>= 1;                       // 0C15A8A6..0C15A8AE
if (I(state+0x140)) pole >>= 2;                       // 0C15A8B0..0C15A8B8
if (I(state+0x1A8)) pole = 90;                        // 0C15A8BA..0C15A8C2
pole = clamp(pole, 1, 120);                           // 0C15A8C4..0C15A8D2
if (pole != I(0x0CAA9758)) send{0x06, 2, pole};       // 0C15A8D4..0C15A8E2
```

Strength is the hard-coded immediate `2` on every send; only the pole moves.

### Rumble, command 5

A seven-term accumulator starting at `0C15A8E4` — it forms
`|F(+0x258)| + |F(+0x25C)|` and `|F(+0x260)| + |F(+0x264)|` and combines them as
`A*80 + B` — ends at `0C15AA9A`:

```c
if (r8) { send{0x05, 63, r8}; latch = 1; }            // 63 half-Hz = 31.5 Hz
else if (latch == 1) { send{0x05, 0, 0}; latch = 0; } // silenced, not faded
```

The rate is always 63. Only the amplitude varies. The full accumulator is **not
yet decoded**.

## The tables

```
carLimit       0C31FE04  35 ints    90 90 90 94 95 93 95 98 97 90 91 91 92 90 94
                                    93 97 95 96 98 90 91 98 98 97 90 90 98 98 91
                                    88 96 92 89 99
strengthLimitA 0C31FD54  11 ints    -35 -28 -21 -14 -7 0 7 14 21 28 35
strengthLimitB 0C31FDD8  11 ints    -40 -30 -20 -10 0 10 20 30 40 70 70
damperRate     0C31FD80  11 floats  10 10 15 20 25 35 40 45 50 55 60
damperScale    0C31FDAC  11 floats  0.2 0.4 0.6 0.8 1.0 1.2 1.4 1.6 1.8 2.6 2.6
```

`*0x0C901654` is the **car**: the same index selects a 24-byte engine record at
`0C270EB0` whose first float is the redline (8000, 9000, 10000 rpm), and the
table has exactly 35 entries for the game's 35 cars.

`*0x0C9015D8` and `*0x0C9015EC` are two eleven-position operator dials.
`0C9015D8` is written as `clamp(I(0x0C4004DC), 0, 10)` at `0C15984A..0C159860`;
`0C9015EC` is a zero-extended byte from offset `0x04A3` of a structure
(`0C1597CA..0C1597D6`). Both tables repeat their top entry, which is the cabinet
saying the dial saturates — and indeed at dial 10 every car's limit exceeds the
board's ceiling of 127, so the per-car differences disappear at full strength.

The remake maps the player's one strength slider onto both dials, which
reproduces the cabinet's own strength range rather than inventing a gain.

## The boot script, `0C159FA0`

One call per frame, switching on the step counter at `0CAA9748`:

| step | commands |
|---|---|
| 0 | `{0x7F,0,0}` reset / calibrate |
| 120 | `{0x7A,0,16}`, `{0x7D,0,0}` |
| 180 | `{0x7C,0,63}` |
| 240 | `{0x01,48,64}`, `{0x02,127,84}`, `{0x00,0,1}` |
| 300 | `{0x7D,0,0}` |
| 360 | `{0x7C,0,63}` |
| 421..539 | `{0x00,0,1}`, then torque `clamp((*(u16*)0x0C92F0E0) >> 8, 48, 208)` — a self-test that servos the wheel from the cabinet's own ADC |
| 600 | `{0x01,48,64}`, `{0x02,127,84}`, `{0x00,0,0}`, `{0x7A,0,20}`, then latches |

**Command 1 is sent here and nowhere else, always as `{0x01,48,64}`.** The limit
computed in the per-frame owner is *never transmitted* — it is only a local clamp
on the torque code. Since command 1 caps the spring and this game never sends a
spring, command 1 has no audible effect at all; it is sent for fidelity.

The remake replays step 240 (`{0x01,48,64}`, `{0x02,127,84}`, enable) when a race
starts, and deliberately skips step 0's `0x7F` — calibration sweeps a real
cabinet's wheel to find its stops, and a DirectInput wheel has already done that —
and the 421..539 ADC self-test.

`0C15A1C0` is a second, cyclic 2048-step script sharing the same counter: it swings
the wheel hard left and right every 120 steps and then pulses the rumble. That is
the attract-mode wheel dance.

## Mode-change helpers, `0C15A1A0 .. 0C15A5E0`

Ten small void, no-argument helpers: enable/disable, damper and rumble off, a
torque hold at 80 / 176, and three "centre the wheel" variants that read the
analog channel at `0C92F0E0`, re-centre it against the stored calibration at
`0C9015E8`, clamp, and send command `0x04` only on a change.

## What the remake stands in for

`original_ffb_owner.cpp` implements the structure above exactly. Four things it
cannot yet source from the original are marked `REMAKE MAPPING` in that file:

| original | remake stand-in |
|---|---|
| `F(state+0x1CC)` wheel position | `steering` |
| `F(state+0x27C)` used as `1 - x` | `1 - |headingError| / 0.30` |
| the `±40` road term | the barrier push, `wallLateral` |
| the rumble accumulator | held and decayed `impact` |

It also leaves the `state+0x24C` trim at its neutral and does not implement the
`+0x434` / `+0x438` / `+0x140` gates or the `+0x1A8` damper override, because the
remake has nothing established to drive them.

## Still unknown

1. The rumble accumulator, `0C15A8E4 .. 0C15AA98`. Seven terms, of which two are
   `|F(+0x258)| + |F(+0x25C)|` and `|F(+0x260)| + |F(+0x264)|`, combined as
   `A*80 + B`.
2. What `state+0x1CC`, `+0x27C`, `+0x3B4/+0x3B8`, `+0x3C4/+0x3C8`, `+0x24C`,
   `+0x140`, `+0x1A8`, `+0x434`, `+0x438` actually are. Finding their writers is
   the single highest-value next step: it would turn every `REMAKE MAPPING` above
   into a port.
3. Whether commands `0x02`, `0x7A`, `0x7C`, `0x7D` do anything the board decodes;
   Flycast's `midiffb.cpp` ignores all four.
