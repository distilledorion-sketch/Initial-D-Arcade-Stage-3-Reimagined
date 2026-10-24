"""Package a Windows player without reading or including personal music or ROMs.

Release-specific verification scripts may import collect_player_files() to use
the same privacy boundary before running their additional build/smoke checks.
"""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import json
import re
import stat
import sys
import zipfile

ALLOWED_DIRS = {'InitialDUnity_Data', 'MonoBleedingEdge', 'D3D12'}
ALLOWED_FILES = {'InitialDUnity.exe', 'UnityPlayer.dll', 'UnityCrashHandler64.exe',
                 'dstorage.dll', 'dstoragecore.dll', 'steam_appid.txt', 'READ ME.txt',
                 'Replay Viewer.cmd', 'MULTIPLAYER TEST.txt'}
MUSIC_DIRS = {'custom music', 'custom-music'}
PRIVATE = {'userdata', 'userdata-unity-scene', 'community-times', 'replays',
           'admin-access.txt', 'identity.json', 'game-options.json',
           'deploy.private.json', 'library.json', 'pending.json'}
EXCLUDED = {'BUILD INFO.json', 'data.manifest.json', 'ENNA ONLINE UPDATE.txt'}


def original_rom_name(name):
    return any(p.lower() == 'rom' or p.lower().endswith(('.chd', '.cue')) or
               re.fullmatch(r'(?:.*[-_ ])?track[-_ ]?\d+\.bin', p, re.I)
               for p in name.replace('\\', '/').split('/'))


def collect_player_files(player):
    player = Path(player)
    files = []
    folded = set()

    def visit(folder):
        for path in sorted(folder.iterdir()):
            rel = path.relative_to(player)
            name = rel.as_posix()
            # Prune by name before metadata inspection or descending into the
            # folder. Personal tracks, links and README contents are never read.
            if any(part.lower() in MUSIC_DIRS for part in rel.parts):
                continue
            assert not any(part.lower() in PRIVATE for part in rel.parts), ('Personal release input', name)
            info = path.lstat()
            assert not stat.S_ISLNK(info.st_mode) and not (getattr(info, 'st_file_attributes', 0) & stat.FILE_ATTRIBUTE_REPARSE_POINT), path
            if stat.S_ISDIR(info.st_mode):
                assert name == 'rom' or not original_rom_name(name), ('Unexpected ROM directory', name)
                assert name == 'rom' or rel.parts[0] in ALLOWED_DIRS, ('Unexpected game directory', name)
                visit(path)
                continue
            assert stat.S_ISREG(info.st_mode), ('Unexpected non-file entry', name)
            if name == 'rom/README.txt':
                continue
            assert not original_rom_name(name), ('Original ROM file in package input; use clean staging', name)
            assert path.suffix.lower() != '.idreplay', ('Personal replay in package input', name)
            if path.name in EXCLUDED or path.suffix.lower() in {'.pdb', '.log'}:
                continue
            assert rel.parts[0] in ALLOWED_DIRS if len(rel.parts) > 1 else rel.name in ALLOWED_FILES, rel
            assert len(name) <= 220 and name.lower() not in folded, rel
            assert not any(part in ('', '.', '..') or part.endswith((' ', '.')) or
                           re.search(r'[\\<>:"|?*\x00-\x1f]', part) or
                           re.match(r'^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(\.|$)', part, re.I)
                           for part in rel.parts), rel
            folded.add(name.lower())
            files.append(path)

    assert player.is_dir(), player
    visit(player)
    return files


def build(player, archive, acceptance=None, candidate_only=False):
    player, archive = Path(player), Path(archive)
    if sys.flags.optimize:
        raise ValueError('Packaging requires Python assertions enabled')
    if archive.resolve().is_relative_to(player.resolve()):
        raise ValueError('Package output must be outside the player')
    if archive.exists() or archive.with_suffix('.report.json').exists():
        raise FileExistsError('Use a new package and report path')
    if bool(acceptance) == bool(candidate_only):
        raise ValueError('Supply complete --acceptance evidence, or explicitly use --candidate-only for a private test package')
    receipt = None
    if candidate_only:
        if not archive.name.startswith('Candidate-'):
            raise ValueError('Private test archives must use the Candidate- filename prefix')
    else:
        # Do not accept a cached releaseReady flag. Recheck all referenced
        # evidence and the candidate before creating any distributable archive.
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        import release_acceptance
        receipt = release_acceptance.check(Path(acceptance), player)
    files = collect_player_files(player)
    spec = importlib.util.spec_from_file_location('update_patch', Path(__file__).with_name('Build-UpdatePatch.py'))
    patch = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(patch)
    assert patch.REQUIRED <= {p.relative_to(player).as_posix() for p in files}, 'Incomplete Windows player'
    archive.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive, 'x', zipfile.ZIP_DEFLATED, compresslevel=6) as output:
        for path in files:
            output.write(path, path.relative_to(player).as_posix())
    with zipfile.ZipFile(archive) as output:
        assert output.testzip() is None, 'Release ZIP failed CRC verification'
        inventory = patch.inventory(output)
        assert len(inventory) == len(files)
    if receipt:
        if archive.stat().st_size >= 2 * 1024**3:
            raise ValueError('Release archive exceeds the download limit; use the verified split downloads')
        actual = {name: {'bytes': row['size'], 'sha256': row['sha256']} for name, row in inventory.items()}
        digest = hashlib.sha256(json.dumps(actual, sort_keys=True, separators=(',', ':')).encode()).hexdigest()
        if digest != receipt['candidateInventorySha256']:
            raise ValueError('Packaged bytes differ from the accepted candidate; archive is not approved')
    with archive.open('rb') as stream:
        digest = hashlib.file_digest(stream, 'sha256').hexdigest()
    report = dict(archive=str(archive), bytes=archive.stat().st_size, sha256=digest,
                  files=len(files), crcCheckPassed=True, privateDataExcluded=True,
                  customMusicEntriesExcluded=True, romEntriesExcluded=True,
                  releaseReady=receipt is not None, candidateOnly=candidate_only,
                  acceptance=receipt)
    archive.with_suffix('.report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report), flush=True)
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--player', required=True)
    parser.add_argument('--archive', required=True)
    approval = parser.add_mutually_exclusive_group(required=True)
    approval.add_argument('--acceptance', help='Complete local acceptance manifest; checked again before packaging')
    approval.add_argument('--candidate-only', action='store_true', help='Private verification archive; requires Candidate- filename prefix')
    args = parser.parse_args()
    build(args.player, args.archive, args.acceptance, args.candidate_only)
