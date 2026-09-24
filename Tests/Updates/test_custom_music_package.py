"""Personal tracks are never scanned, archived or retained by release patches."""
from pathlib import Path
from unittest.mock import patch
import importlib.util
import io
import uuid
import zipfile

root = Path(__file__).resolve().parents[2]


def module(name, filename):
    spec = importlib.util.spec_from_file_location(name, root/'Tools'/filename)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


release = module('release', 'Build-ReleasePackage.py')
update = module('update', 'Build-UpdatePatch.py')
fixture = root/'Tests/Updates'/('installer-music-'+uuid.uuid4().hex)
player = fixture/'player'
required = {name: b'isolated game fixture' for name in update.REQUIRED}
personal = {'Custom Music/README.txt': b'personal instructions',
            'Custom Music/song.mp3': b'personal song',
            'Custom Music/subfolder/other.wav': b'another personal song',
            'InitialDUnity_Data/StreamingAssets/custom-music/song.ogg': b'legacy song'}
for name, data in {**required, **personal, 'rom/README.txt': b'ROM instructions'}.items():
    path = player/name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)

original_iterdir, original_lstat = Path.iterdir, Path.lstat


def check_music_path(path):
    assert not any(part.lower() in release.MUSIC_DIRS for part in path.parts), 'Packager inspected personal music: '+str(path)


def safe_iterdir(path):
    check_music_path(path)
    return original_iterdir(path)


def safe_lstat(path):
    check_music_path(path)
    return original_lstat(path)


with patch.object(Path, 'iterdir', safe_iterdir), patch.object(Path, 'lstat', safe_lstat):
    assert {p.relative_to(player).as_posix() for p in release.collect_player_files(player)} == set(required)
    report = release.build(player, fixture/'full.zip')
assert report['customMusicEntriesExcluded']
with zipfile.ZipFile(report['archive']) as archive:
    assert set(archive.namelist()) == set(required)


def inventory(name):
    data = io.BytesIO()
    with zipfile.ZipFile(data, 'w') as archive:
        for path, contents in {**required, name: b'private fixture'}.items():
            archive.writestr(path, contents)
    data.seek(0)

    class NoMusicReads(zipfile.ZipFile):
        def open(self, path, *args, **kwargs):
            name = path.filename if isinstance(path, zipfile.ZipInfo) else path
            assert name in required, 'Inventory read personal music: '+name
            return super().open(path, *args, **kwargs)

    with NoMusicReads(data) as archive:
        return update.inventory(archive)


for name in ['Custom Music/', 'Custom Music/README.txt', 'Custom Music/song.mp3',
             'custom music/song.wav', 'CUSTOM MUSIC/song.ogg', 'custom-music/song.pcm',
             'InitialDUnity_Data/StreamingAssets/Custom Music/song.mp3']:
    try:
        inventory(name)
    except AssertionError as error:
        assert str(error).startswith('Personal music must not enter release packages:'), error
    else:
        raise AssertionError('Inventory accepted personal music: '+name)

# A mistakenly staged original ROM still fails before any file is archived.
(player/'rom/gds-0033.chd').write_bytes(b'private fixture, not a valid dump')
try:
    release.collect_player_files(player)
except AssertionError as error:
    assert 'Original ROM file in package input' in str(error), error
else:
    raise AssertionError('Packager accepted an original ROM')
assert all((player/name).read_bytes() == data for name, data in personal.items())
print('PASS: Full ZIP excludes music without inspecting tracks; patch inventory rejects music paths before reads; original ROMs remain blocked.')
