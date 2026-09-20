#!/usr/bin/env python3
"""Import the 15 additional PS2 race songs without changing existing track IDs."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re

SONGS = (
    ("100", "100", "Dave Rodgers"),
    ("BACK_ON_THE_ROCKS", "Back on the Rocks", "Mega NRG Man"),
    ("BIG_IN_JAPAN", "Big in Japan", "Robert Patton"),
    ("BURNING_DESIRE", "Burning Desire", "Mega NRG Man"),
    ("CRAZY_FOR_YOUR_LOVE", "Crazy for Your Love", "Morris"),
    ("CRAZY_NIGHT", "Crazy Night", "Boys Band"),
    ("DONT_STAND_SO_CLOSE", "Don't Stand So Close", "Dr. Love"),
    ("DONT_YOU", "Don't You (Forget About My Love)", "Sophie"),
    ("GET_ME_POWER", "Get Me Power", "Mega NRG Man"),
    ("I_NEED_YOUR_LOVE", "I Need Your Love", "Dave Simon"),
    ("MIKADO", "Mikado", "Dave McLoud"),
    ("NO_ONE_SLEEP_IN_TOKYO", "No One Sleep in Tokyo", "Edo Boys"),
    ("STAY", "Stay", "Victoria"),
    ("WEST_END_GUY", "West End Guy", "Digital Planet"),
    ("WHITE_LIGHT", "White Light", "Mr. Groove"),
)
SOURCES = [
    "https://www.play-asia.com/super-eurobeat-presents-initial-d-special-stage-original-soundtr/13/7063r",
    "https://vgost.fandom.com/wiki/Initial_D%3A_Special_Stage",
]

def key(title):
    normalized = re.sub('[^a-z0-9]', '', title.lower())
    return 'speedyspeedboy' if normalized == 'speedspeedboy' else normalized

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True, help='Extracted RACEBGM folder')
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    project = Path(__file__).resolve().parents[1]
    spec = importlib.util.spec_from_file_location('catalog', project/'Tools/Export-MusicCatalog.py')
    exporter = importlib.util.module_from_spec(spec); spec.loader.exec_module(exporter)
    require = exporter.require
    streams = project/'Native/data/original_audio/streams'
    header_path = project/'Native/src/music_catalog.h'
    header = header_path.read_text(encoding='utf-8-sig')
    entries = exporter.parse_entries(header)
    require(len(entries) in (102, 117), 'Unexpected pre-import catalog')
    previous = json.loads((streams/'music_catalog.json').read_bytes())
    old_keys = {key(entry['title']) for entry in entries[:102]}
    source_files = sorted(args.source.glob('*.ADX'))
    require(len(source_files) == 31, 'Expected 31 extracted PS2 race streams')
    additional = {p.stem for p in source_files if key(p.stem) not in old_keys}
    require(additional == {song[0] for song in SONGS}, 'Duplicate comparison changed')
    imports, titles, pending = [], [], []
    for code, title, artist in SONGS:
        require(key(title) not in old_keys, 'Duplicate song: '+title)
        filename = code+'.ADX'; data = (args.source/filename).read_bytes()
        metadata = exporter.inspect_adx(data, filename)
        require(metadata['version']==3 and metadata['channels']==2 and metadata['looping'],
                'Unexpected PS2 race encoding: '+filename)
        relative = 'specialstage/'+filename; identity = 'specialstage.'+code
        destination = streams/relative
        require(not destination.exists() or destination.read_bytes()==data, 'Existing import differs')
        pending.append((destination,data))
        imports.append(dict(id=identity, sourceStage=10, sourceGame='Initial D Special Stage',
            filename=filename, path='data/original_audio/streams/'+relative,
            sourceArchive='CDVD/DATA/SOUND/RACEBGM.AFS', sourceArchiveMember=filename,
            displayLabel=title, titleKnown=True, **metadata))
        titles.append(dict(id=identity,title=title,artist=artist,sources=SOURCES,
            fileMappingEvidence='Original RACEBGM.AFS filename '+filename+
            '; title/artist cross-checked with the Special Stage soundtrack listings.'))
    if len(entries)==102:
        header = header.replace('RaceMusicTrack,102>', 'RaceMusicTrack,117>')
        at = exporter.DECLARATION.search(header).end(2)
        lines = ''.join('    {'+', '.join((json.dumps('specialstage.'+code),json.dumps(title),
            '10',json.dumps('specialstage/'+code+'.ADX'),json.dumps(artist)))+'},\n'
            for code,title,artist in SONGS)
        header = header[:at]+lines+header[at:]
    new_entries = exporter.parse_entries(header)
    require(new_entries[:102]==entries[:102], 'Existing saved song indices changed')
    require([e['id'] for e in new_entries[102:]]==[e['id'] for e in imports], 'PS2 order changed')
    manifest_path=streams/'extra_music_manifest.json'; titles_path=streams/'music_title_metadata.json'
    manifest=json.loads(manifest_path.read_bytes()); title_data=json.loads(titles_path.read_bytes())
    manifest['tracks']=[t for t in manifest['tracks'] if t['sourceStage']!=10]+imports
    manifest['trackCount']=len(manifest['tracks'])
    manifest['specialStageImport']=dict(sourceGame='Initial D Special Stage',added=15,excludedDuplicates=16,
        transformation='none; original ADX v3 files and loops preserved',
        titleSources=SOURCES,decoderReference='https://github.com/vgmstream/vgmstream/blob/master/src/meta/adx.c')
    title_data['tracks']=[t for t in title_data['tracks'] if not t['id'].startswith('specialstage.')]+titles
    encode=lambda value:(json.dumps(value,indent=2,ensure_ascii=False)+'\n').encode('utf-8')
    pending += [(header_path,header.encode()),(manifest_path,encode(manifest)),(titles_path,encode(title_data))]
    for path,data in pending:
        if args.check:
            require(path.exists() and path.read_bytes()==data, 'Missing or stale import: '+str(path))
        else:
            path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
    catalog=exporter.export(project)
    require(catalog['tracks'][:102]==previous['tracks'][:102], 'Prior song metadata/audio changed')
    if args.check: require(catalog==previous,'Stale exported catalog')
    else: (streams/'music_catalog.json').write_bytes(encode(catalog))
    print(json.dumps(dict(status='verified' if args.check else 'imported',tracks=117,added=15,
        excludedDuplicates=16,prior102Unchanged=True,bytes=sum(t['bytes'] for t in imports)),indent=2))

if __name__=='__main__': main()
