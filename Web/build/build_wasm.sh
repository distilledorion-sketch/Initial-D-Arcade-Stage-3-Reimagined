#!/bin/bash
# Build the portable scene core to WebAssembly (wasm32, Emscripten).
#
# Usage: Web/build/build_wasm.sh <outdir> [-jN]
# Env:   EMCC       path to emcc (default: emcc on PATH)
#        OPT        optimisation for compilation units (default -O2)
#        FORCE=1    rebuild every unit
#        SYNTAX=1   only syntax-check the units (fast portability triage)
#
# Nothing under Native/ is modified. The units are compiled from a build-time copy of
# Native/{src,vendor,tests} with Web/patches/*.patch applied; the Win32 gaps are covered by
# Web/shim via -include / an extra include path, exactly as in the native Linux build.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"; repo="$here/../.."; shim="$here/../shim"
out="$(mkdir -p "$1" && cd "$1" && pwd)"; jobs="${2:--j2}"; jobs="${jobs#-j}"
emcc="${EMCC:-emcc}"; opt="${OPT:--O2}"

# 1. build-time copy + patches (cp -a keeps timestamps so incremental builds still work)
mkdir -p "$out/Native"
for d in src vendor tests; do rm -rf "$out/Native/$d"; cp -a "$repo/Native/$d" "$out/Native/$d"; done
for p in "$here"/../patches/*.patch; do
  # The tree was just copied fresh, so a patch that does not apply means the sources drifted: stop.
  (cd "$out" && patch -p1 -s --no-backup-if-mismatch < "$p") || { echo "patch failed: $p" >&2; exit 1; }
done

# 2. compile
flags="-std=c++20 $opt -g0 -w -ffp-contract=off -fno-strict-aliasing -fwasm-exceptions \
 -DIDAS3_PORTABLE_SCENE -DIDAS3_UNITY_PLUGIN -DIDAS3_WEB \
 -I$out/Native/src -I$out/Native/vendor/lz4 -I$shim/inc -I$shim -include $shim/web_compat.h"
mkdir -p "$out/obj"
cd "$out/Native"
{ cat "$here/scene_sources.txt"; echo "vendor/lz4/lz4.c"; } | EMCC="$emcc" FLAGS="$flags" OUT="$out" FORCE="${FORCE:-}" SYNTAX="${SYNTAX:-}" \
  xargs -P "$jobs" -I{} bash -c '
  src="{}"; obj="$OUT/obj/$(echo "$src" | tr / _).o"
  if [ -z "$SYNTAX" ] && [ -z "$FORCE" ] && [ -f "$obj" ] && [ "$obj" -nt "$src" ]; then exit 0; fi
  lang=""; case "$src" in *.c) lang="-x c++";; esac
  if [ -n "$SYNTAX" ]; then
    "$EMCC" $FLAGS $lang -fsyntax-only "$src" > "$OUT/obj/$(echo "$src" | tr / _).log" 2>&1 \
      && echo "ok   $src" || echo "FAIL $src"
  else
    "$EMCC" $FLAGS $lang -c "$src" -o "$obj" || { echo "FAILED: $src" >&2; exit 255; }
  fi'
echo "units done: $out/obj"
