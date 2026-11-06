#!/bin/bash
# Native Linux build of the portable scene core + headless harness (for debugging and tests).
# Usage: Web/build/build_native.sh <outdir> [-jN]    Env: OPT (default -O1), FORCE=1 to rebuild all.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"; native="$here/../../Native"; shim="$here/../shim"
out="$(mkdir -p "$1" && cd "$1" && pwd)"; jobs="${2:--j2}"; jobs="${jobs#-j}"
opt="${OPT:--O1}"
flags="-std=c++20 $opt -g0 -w -ffp-contract=off -fno-strict-aliasing -DIDAS3_PORTABLE_SCENE -DIDAS3_UNITY_PLUGIN \
 -I$native/src -I$native/vendor/lz4 -I$shim/inc -I$shim -include $shim/web_compat.h"
mkdir -p "$out/obj"
cd "$native"
{ cat "$here/scene_sources.txt"; echo "vendor/lz4/lz4.c"; } | FLAGS="$flags" OUT="$out" FORCE="${FORCE:-}" xargs -P "$jobs" -I{} bash -c '
  src="{}"; obj="$OUT/obj/$(echo "$src" | tr / _).o"
  if [ -z "$FORCE" ] && [ -f "$obj" ] && [ "$obj" -nt "$src" ]; then exit 0; fi
  lang=""; case "$src" in *.c) lang="-x c++";; esac
  g++ $FLAGS $lang -c "$src" -o "$obj" || { echo "FAILED: $src" >&2; exit 255; }'
g++ $flags -c "$here/../tests/headless_scene.cpp" -o "$out/obj/headless_scene.o"
g++ "$out"/obj/*.o -o "$out/headless_scene" -lpthread
echo "built $out/headless_scene"
