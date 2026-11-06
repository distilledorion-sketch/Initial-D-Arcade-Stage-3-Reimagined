#!/usr/bin/env python3
"""Regenerate scene_sources.txt from Native/CMakeLists.txt."""
import os, re, sys
here = os.path.dirname(os.path.abspath(__file__))
native = os.path.join(here, '..', '..', 'Native')
txt = open(os.path.join(native, 'CMakeLists.txt')).read()
targets = {'idas3_core', 'idas3_native_assets', 'idas3_original', 'idas3_asset_storage', 'InitialDRemake'}
srcs = set()
for m in re.finditer(r'(add_library|add_executable|target_sources)\(\s*(\w+)\s*(?:STATIC|SHARED|WIN32|PRIVATE|PUBLIC)?\s*([^)]*)\)', txt, re.S):
    if m.group(2) in targets:
        srcs.update(re.findall(r'src/[\w./-]+\.cpp', m.group(3)))
srcs |= {'src/unity_bridge.cpp', 'src/unity_audio_output.cpp', 'src/unity_scene_capture.cpp', 'src/unity_ui_capture.cpp'}
srcs.discard('src/main.cpp')
missing = sorted(s for s in srcs if not os.path.exists(os.path.join(native, s)))
if missing: sys.exit('missing: %s' % missing)
open(os.path.join(here, 'scene_sources.txt'), 'w').write('\n'.join(sorted(srcs)) + '\n')
print(len(srcs), 'sources')
