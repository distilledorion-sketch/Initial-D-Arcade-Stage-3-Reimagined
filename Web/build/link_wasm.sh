#!/bin/bash
# Link the compiled units (Web/build/build_wasm.sh) into a runnable artifact.
#
# Usage: Web/build/link_wasm.sh <outdir> <mode>
#   mode node-test : headless harness as a Node program (real filesystem via NODERAWFS)
#   mode web       : ES module for the browser (MEMFS/lazy files; see Web/www)
# Env:   EMCC (default emcc), LINKOPT (default -O0: the JS-glue optimiser needs npm packages that
#        are not always available; run Web/build/optimize_wasm.sh afterwards to wasm-opt the binary).
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"; repo="$here/../.."; shim="$here/../shim"
out="$(cd "$1" && pwd)"; mode="${2:?mode}"
emcc="${EMCC:-emcc}"; linkopt="${LINKOPT:--O0}"

# 2 GB cap: above that Emscripten emits an unsignPointers JS pass that needs npm packages, and a
# mobile tab will not reliably get more anyway.
common="-fwasm-exceptions -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=268435456 -sMAXIMUM_MEMORY=2147483648 \
 -sSTACK_SIZE=8388608 -sERROR_ON_UNDEFINED_SYMBOLS=1 -sSUPPORT_LONGJMP=0 -sWASM_BIGINT"

# Every extern "C" Idas3* function in the bridge and the UI capture header becomes an export.
exports="$(grep -hoE '\bIdas3[A-Za-z0-9]+\s*\(' "$repo"/Native/src/unity_bridge.h "$repo"/Native/src/unity_ui_capture.h \
  | sed -E 's/\s*\($//' | sort -u)"
extra="Idas3SceneGetFrame Idas3UnityReadAudio Idas3UnityReadAudioDevice Idas3UnitySetAudioRunning Idas3UnityGetAudioStatistics"
list="[$(for f in $exports $extra _malloc _free; do case $f in _*) printf "'%s'," "$f";; *) printf "'_%s'," "$f";; esac; done | sed 's/,$//')]"

case "$mode" in
  node-test)
    "$emcc" -std=c++20 -O2 -g0 -w -fwasm-exceptions -DIDAS3_PORTABLE_SCENE -DIDAS3_UNITY_PLUGIN -DIDAS3_WEB \
      -I"$out/Native/src" -I"$shim/inc" -I"$shim" -include "$shim/web_compat.h" \
      -c "$repo/Web/tests/headless_scene.cpp" -o "$out/obj/headless_scene.o"
    "$emcc" $linkopt $common -sENVIRONMENT=node -sNODERAWFS=1 -sEXIT_RUNTIME=1 \
      "$out"/obj/*.o -o "$out/headless_scene.js"
    echo "linked $out/headless_scene.js" ;;
  web)
    "$emcc" $linkopt $common -sENVIRONMENT=web,worker -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=IdasCore \
      -sEXPORTED_FUNCTIONS="$list" \
      -sEXPORTED_RUNTIME_METHODS="['FS','HEAPU8','HEAPU32','HEAPF32','HEAP32','UTF8ToString','stringToUTF8','lengthBytesUTF8','getValue','setValue']" \
      -sFILESYSTEM=1 -lidbfs.js -sEXIT_RUNTIME=0 \
      $(ls "$out"/obj/*.o | grep -v headless_scene) -o "$out/idas_core.js"
    echo "linked $out/idas_core.js" ;;
  *) echo "unknown mode $mode" >&2; exit 2 ;;
esac
