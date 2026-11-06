#!/bin/bash
# Assemble the deployable client: Web/www/* plus the linked core (idas_core.js/.wasm).
#   Web/build/package_web.sh <wasm-build dir> <out dir>
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
build="${1:?wasm build dir (contains idas_core.js)}"; out="${2:?output dir}"
for f in idas_core.js idas_core.wasm; do [ -f "$build/$f" ] || { echo "missing $build/$f (run link_wasm.sh web)" >&2; exit 1; }; done
mkdir -p "$out"
cp "$here"/../www/*.js "$here"/../www/*.html "$here"/../www/*.webmanifest "$out"/
cp "$build/idas_core.js" "$build/idas_core.wasm" "$out"/
echo "packaged $(ls "$out" | wc -l) files into $out"
