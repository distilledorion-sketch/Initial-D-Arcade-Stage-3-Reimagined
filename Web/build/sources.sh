#!/bin/sh
# Prints the translation units that make up the portable scene core, relative to Native/.
# Derived from Native/CMakeLists.txt (idas3_core, idas3_native_assets, idas3_original and the
# InitialDRemake app sources, minus main.cpp which unity_bridge.cpp includes itself), plus
# the scene bridge. Regenerate with Web/build/extract_sources.py if CMakeLists changes.
cd "$(dirname "$0")/../../Native" && cat ../Web/build/scene_sources.txt
