#!/usr/bin/env python3
"""List the game's data files for the web client: {path: size} for everything under <native>/data.

  python3 -I Web/tools/make_manifest.py Native [--set name=listfile ...] > manifest.json

Each --set adds a named prefetch list (one manifest path per line, e.g. from the client's access log) under
`prefetch`: the client downloads the "boot" set before starting and "menus" while the menus run, as
whole, compressible, HTTP-cached files instead of one blocking range request per read.

The web client mounts these as lazily fetched read-only files (www/rangefs.js), so the manifest is
the only thing it needs to know up front. `id` changes whenever any path or size changes and is used
as the cache key for downloaded data.
"""
import hashlib, json, os, sys

def main():
    args = sys.argv[1:]
    sets = {}
    while '--set' in args:
        i = args.index('--set')
        name, _, path = args[i + 1].partition('=')
        sets[name] = path
        del args[i:i + 2]
    if len(args) != 1:
        sys.exit(__doc__)
    root = os.path.abspath(args[0])
    data = os.path.join(root, 'data')
    if not os.path.isdir(data):
        sys.exit(f'no data directory in {root}')
    files = {}
    for dirpath, dirnames, filenames in os.walk(data):
        dirnames.sort()
        for name in sorted(filenames):
            full = os.path.join(dirpath, name)
            if os.path.islink(full) or not os.path.isfile(full):
                continue
            files[os.path.relpath(full, root).replace(os.sep, '/')] = os.path.getsize(full)
    digest = hashlib.sha256()
    for path in sorted(files):
        digest.update(f'{path}\0{files[path]}\n'.encode())
    prefetch = {}
    for name, path in sets.items():
        listed = [l.strip() for l in open(path, encoding='utf-8') if l.strip()]
        unknown = [p for p in listed if p not in files]
        if unknown:
            sys.exit(f'--set {name}: {len(unknown)} paths are not in the data tree, e.g. {unknown[0]}')
        prefetch[name] = list(dict.fromkeys(listed))
    json.dump({'version': 1, 'id': digest.hexdigest()[:16], 'count': len(files), 'bytes': sum(files.values()),
               'prefetch': prefetch, 'files': files}, sys.stdout, separators=(',', ':'))

main()
