# Web client (mobile and desktop browsers)

A browser build of the game's portable scene core. Everything for it lives under `Web/`; nothing in
`Native/`, `Assets/` or the Android project is modified (the few source changes the WebAssembly build
needs are `patches/*.patch`, applied to a build-time copy).

```
 browser tab
 ├─ main thread   app.js  input (keyboard / gamepad / touch), ROM gate, audio context, UI shell
 ├─ worker        worker.js  the whole game: core (wasm) + WebGL2 renderer on an OffscreenCanvas
 │                rangefs.js  lazily fetched, read-only game data (HTTP Range + prefetch)
 └─ AudioWorklet  audio-worklet.js  jitter buffer fed with the core's own 44.1 kHz mixer output
```

The core is the same C++ the Unity plugin and the Linux harness run (`IDAS3_PORTABLE_SCENE`), compiled to
wasm32 with Emscripten. It produces the scene (vertex ranges, textures, cameras, lighting constants),
2D draw lists and audio; `www/renderer.js` turns those into WebGL2 draws, the same job
`Assets/Scripts` does in Unity.

## Requirements

* Module workers, `OffscreenCanvas` WebGL2 inside a worker, `AudioWorklet`: Chrome/Edge, Firefox 105+, Safari 17+.
* Memory: the core runs with a 2 GB ceiling; about 307 MB of heap and ~215 MB of cached game data were
  in use in the menu-to-race session that was measured (see Limitations: not measured on a phone).
* The player supplies their own **GDS-0033** disc image, like the desktop game (see below).

## Build

Prerequisites: Emscripten 3.1.x (built and tested with 3.1.55), Python 3, Node 18+.

```
export EMCC=/path/to/emcc
Web/build/build_wasm.sh  build/wasm -j8        # compile the units (patches are applied first; a failing patch aborts)
Web/build/link_wasm.sh   build/wasm web        # idas_core.js + idas_core.wasm
Web/build/package_web.sh build/wasm dist       # www/* + core -> dist/
python3 -I Web/tools/make_manifest.py Native > manifest.json
```

`link_wasm.sh` links at `-O0` by default because the glue optimizer needs npm packages that are not
always installed. Run Binaryen's `wasm-opt -O2` over `idas_core.wasm` for a release build (not done or
measured yet). `build_native.sh` builds the same units for Linux for debugging and for `tests/`.

## Run

```
node Web/tools/serve.mjs --www dist --data Native --manifest manifest.json --port 8080
```

Open `http://localhost:8080/`, choose the disc files, play. Query options: `?stats=1` (overlay),
`?scale=<factor>` / `?maxlong=<px>` (render size; defaults 0.75 / 1280 on touch devices, 1 / 1920 otherwise), `?touch=1` (force touch controls),
`?data=<url>` (game data base URL).

`serve.mjs` is a development server. For hosting, put `dist/` and the converted game data
(`/game/`, from `Native/data`) behind any static host that supports **HTTP Range requests**, serves
`/game/` with long-lived caching, and compresses the compressible file types. A race session reads
~215 MB of the 2.85 GiB tree; the boot set is ~140 MB raw (~40 MB compressed) and menu to race ~75 MB
raw more. Prefetch sets are recorded from the client's access log (`make_manifest.py --set boot=list`).

**Hosting the data is the owner's decision**: those files are the game's converted assets. The ROM gate
below does not make them private.

## ROM gate

`www/romgate.js` is a port of `Assets/Scripts/Idas3RomValidation.cs`: exact size and SHA-256 of
`gds-0033.chd`, or the CUE sheet (same normalization, 16 KiB limit) plus the three BINs. Files are read
in the browser and not uploaded; nothing is remembered between visits. As on desktop this is a check
inside the client the player runs, so it is not a technical barrier. There is deliberately no flag or
query parameter that skips it; the automated tests replace the gate module inside Playwright only.

## Controls

Keyboard and gamepad follow the desktop mapping. Touch: left-hand steering zone (drag from where you
touch), GAS/BRAKE pedals, shift ▲▼, VIEW, LIGHTS, PAUSE while driving; D-pad, OK, BACK, Y in menus.
Taps shorter than one frame are latched so they are never lost. Tilt steering is not implemented.

Saves are written to IndexedDB (`/saves`) every 10 s and when the tab is hidden or closed. Two tabs
of the same profile are last-writer-wins.

## Tests (`Web/tests`)

| test | what it checks |
| --- | --- |
| `headless_scene.cpp` (native and wasm builds) | scripted sessions; wasm state, frame and audio output match native (tiny libm differences only in a few vertex alpha values) |
| `render_dump.cjs` + `render.html` | renders captured frames/UI dumps with `renderer.js` in headless Chromium, counts GL calls |
| `boot_test.cjs` | full client boot against `serve.mjs`: loading, menus, race, stats, audio, access log |
| `gate_ui_test.cjs`, `romgate_test.mjs` | gate screen and the validation logic against synthetic discs |
| `touch_test.cjs` | touch layouts and short-tap latching |
| `persist_test.cjs` | IndexedDB save persistence across reloads |

All of this ran on software GL (SwiftShader) in a container. No real phone, GPU or real GDS-0033 dump
was available.

## Renderer cost

GL calls per captured race frame (two views, 2858 draws): 70,877 at the start, **11,181** after
slab-allocated geometry buffers, texture/sampler bind dedupe and shadowed per-range uniforms; the
image is pixel-identical to the unoptimized output. The captured dumps carry no geometry ids, so the
cached-geometry path is exercised only in live runs (`gfx` in the worker stats: uploaded vertices,
cache hits, cached geometry, slabs).

## Limitations / not done

* **Online play is not implemented.** The core's multiplayer bridge is transport independent (snapshot
  and `Idas3MultiplayerAuthorityPacket/Receive` functions), but rooms, quick match, chat, sanctions, the
  player directory and the online menus live in the Unity layer (`Assets/Scripts/Multiplayer`, built on
  Epic Online Services P2P, a TCP transport and an HTTP directory), not in the core. A web version needs
  those flows reimplemented in JS and a browser-reachable relay (WebSocket/WebRTC); that means running a
  server and accounts/moderation policy, which is a product decision.
* Not tested on real hardware: phone performance and memory pressure, iOS audio unlock, Safari's
  OffscreenCanvas WebGL2, acceptance of a real GDS-0033 dump.
* The wasm is unoptimized (`-O0` link, no `wasm-opt`).
* Known renderer gaps: opaque-alpha depth pre-pass, special headlight shader, imported-course variant,
  HUD group scale/offset, Arcade meter/ornament HUD themes.
