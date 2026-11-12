"""Exercise Unity's real updater coroutine in an isolated Wine installation."""
from pathlib import Path
import functools
import hashlib
import http.server
import json
import os
import re
import shutil
import subprocess
import tempfile
import threading
import time
import zipfile

root = Path(__file__).resolve().parents[2]
proof = Path(os.environ.get('IDAS3_FLOW_TEST_OUTPUT', root / 'Verification/linux-reports-20260927/unity-flow-baseline'))
proof.mkdir(parents=True, exist_ok=False)
base = Path(tempfile.mkdtemp(prefix='idas3-unity-updater-', dir=str(Path.home())))
prefix = base / 'prefix'
wine = '/usr/lib/wine/wine64'
player = Path(os.environ['IDAS3_FLOW_TEST_PLAYER']).resolve(strict=True)
child = Path(os.environ.get('IDAS3_UPDATE_TEST_CHILD', root / 'Native/build-update-helper/UpdateTestChild.exe')).resolve(strict=True)
version = re.search(r'^  bundleVersion: (.+)$', (root / 'ProjectSettings/ProjectSettings.asset').read_text(), re.M).group(1).strip()
env = dict(os.environ, WINEPREFIX=str(prefix), WINEDEBUG='-all',
           WINEDLLOVERRIDES='mscoree,mshtml=;winemenubuilder.exe=d')

def win(path):
    # Deliberately retain aliases so production code has to handle them.
    return 'Z:' + str(path.absolute()).replace('/', '\\')

def unix(path):
    if path.startswith('C:\\'):
        return prefix / 'drive_c' / path[3:].replace('\\', '/')
    assert path.startswith('Z:\\'), path
    return Path(path[2:].replace('\\', '/'))

class QuietHandler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        self.server.downloads.append(self.path)
        if self.path == '/content.zip' and self.server.interrupt_content:
            data = (Path(self.directory) / 'content.zip').read_bytes()
            self.send_response(200)
            self.send_header('Content-Length', str(len(data)))
            self.end_headers()
            self.wfile.write(data[:len(data) // 2])
            self.wfile.flush()
            self.close_connection = True
            return
        super().do_GET()

    def log_message(self, *args):
        pass

results = []
report = {'passed': False, 'fixture': str(base), 'player': str(player),
          'runnerSha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
          'assemblySha256': hashlib.sha256((player.parent / 'InitialDUnity_Data/Managed/Assembly-CSharp.dll').read_bytes()).hexdigest(),
          'helperSha256': hashlib.sha256((root / 'Assets/Resources/UpdateInstaller.exe.bytes').read_bytes()).hexdigest(),
          'wine': subprocess.check_output([wine, '--version'], env=env, text=True).strip(), 'results': results,
          'scope': 'Isolated loopback Unity updater under Wine; no live game, public release or Proton acceptance.'}
(proof / 'report.json').write_text(json.dumps(report, indent=2))
for name in os.environ.get('IDAS3_FLOW_CASES', 'plain,steam-library-link,linked-game-file,linked-content-directory,patch,damaged-retained,corrupt-download,split-full,split-patch-fallback,corrupt-content,interrupted-content,temp-link,linked-cache-root,unmarked-fixture,foreign-game').split(','):
    folder = base / name
    game = folder / 'game space 日本' if name != 'foreign-game' else folder / 'not-direct-child/game space 日本'
    game.mkdir(parents=True)
    if name != 'unmarked-fixture':
        (folder / 'ISOLATED_UPDATE_TEST.txt').write_text('Disposable Unity updater fixture')
    originals = {'InitialDUnity.exe': child.read_bytes(), 'UnityPlayer.dll': b'old engine',
                 'InitialDUnity_Data/globalgamemanagers': b'old metadata',
                 'InitialDUnity_Data/Managed/Assembly-CSharp.dll': b'old scripts'}
    targets = {**originals, 'UnityPlayer.dll': b'new engine',
               'InitialDUnity_Data/globalgamemanagers': b'new metadata',
               'InitialDUnity_Data/Managed/Assembly-CSharp.dll': b'new scripts'}
    split = name in ('split-full', 'split-patch-fallback', 'corrupt-content', 'interrupted-content')
    courses = {'InitialDUnity_Data/StreamingAssets/GUNSAI/menu.idastex': b'new Gunsai',
               'InitialDUnity_Data/StreamingAssets/ODAWARA/menu.idastex': b'new Odawara'} if split else {}
    originals.update({key: b'old course' for key in courses})
    targets.update(courses)
    for relative, data in {**originals, 'userdata/card.json': b'keep save', 'rom/keep.bin': b'keep original'}.items():
        path = game / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    archive = folder / 'update.zip'
    with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED) as output:
        for relative, data in targets.items():
            if relative not in courses:
                output.writestr(relative, data)
    content = folder / 'content.zip'
    if split:
        with zipfile.ZipFile(content, 'x', compression=zipfile.ZIP_DEFLATED) as output:
            for relative, data in courses.items():
                output.writestr(relative, data)
    patch = None
    if name in ('patch', 'damaged-retained', 'split-patch-fallback'):
        patch = folder / 'patch.zip'
        manifest = {'schema': 1, 'baseVersion': version, 'targetVersion': '99.0.0', 'files': []}
        with zipfile.ZipFile(patch, 'w', compression=zipfile.ZIP_DEFLATED) as output:
            for relative, data in targets.items():
                included = data != originals[relative]
                manifest['files'].append({'path': relative, 'size': len(data), 'sha256': hashlib.sha256(data).hexdigest(), 'included': included})
                if included:
                    output.writestr(relative, data)
            output.writestr('update-patch.json', json.dumps(manifest))
        assert patch.stat().st_size < archive.stat().st_size
    if name in ('damaged-retained', 'split-patch-fallback'):
        originals['InitialDUnity.exe'] += b'changed after initial installation'
        (game / 'InitialDUnity.exe').write_bytes(originals['InitialDUnity.exe'])
    outside = None
    if name == 'linked-game-file':
        outside = folder / 'outside-engine.dll'
        outside.write_bytes(originals['UnityPlayer.dll'])
        (game / 'UnityPlayer.dll').unlink()
        (game / 'UnityPlayer.dll').symlink_to(outside)
    if name == 'linked-content-directory':
        outside = folder / 'outside-data'
        (game / 'InitialDUnity_Data').rename(outside)
        (game / 'InitialDUnity_Data').symlink_to(outside, target_is_directory=True)
    game_arg = game
    run_env = env.copy()
    if name == 'steam-library-link':
        alias = base / 'Steam'
        alias.symlink_to(folder, target_is_directory=True)
        game_arg = alias / game.name
    if name == 'temp-link':
        real = folder / 'real-temp'
        previous_temp = results[0]['flow']['tempPath']
        assert previous_temp.startswith('C:\\'), previous_temp
        alias = prefix / 'drive_c' / previous_temp[3:].replace('\\', '/').rstrip('/')
        assert alias.resolve().is_relative_to(prefix.resolve()) and alias.is_dir() and not alias.is_symlink()
        assert real.absolute().is_relative_to(base.resolve()) and not real.exists()
        alias.rename(real)
        alias.symlink_to(real, target_is_directory=True)
    if name == 'linked-cache-root':
        previous_cache = results[-1]['flow']['cacheRoot']
        if previous_cache.startswith('C:\\'):
            cache = prefix / 'drive_c' / previous_cache[3:].replace('\\', '/')
        else:
            assert previous_cache.startswith('Z:\\'), previous_cache
            cache = Path(previous_cache[2:].replace('\\', '/'))
        outside = folder / 'outside-cache'
        assert cache.resolve().is_relative_to(base.resolve()) and cache.is_dir() and not cache.is_symlink()
        assert outside.absolute().is_relative_to(base.resolve()) and not outside.exists()
        cache.rename(outside)
        (outside / 'sentinel.txt').write_bytes(b'untouched cache target')
        cache_before = sorted(str(path.relative_to(outside)) for path in outside.rglob('*'))
        cache.symlink_to(outside, target_is_directory=True)
    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), functools.partial(QuietHandler, directory=str(folder)))
    server.downloads = []
    server.interrupt_content = name == 'interrupted-content'
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    case_proof = proof / name
    case_proof.mkdir()
    # The diagnostic requires its marker and game to share this exact parent.
    config = folder / 'config.json'
    url = f'http://127.0.0.1:{server.server_port}'
    configuration = {'gameRoot': win(game_arg), 'url': url + '/update.zip',
                     'sha256': '0' * 64 if name == 'corrupt-download' else hashlib.sha256(archive.read_bytes()).hexdigest(), 'bytes': archive.stat().st_size}
    if patch:
        configuration.update(patchUrl=url + '/patch.zip', patchSha256=hashlib.sha256(patch.read_bytes()).hexdigest(), patchBytes=patch.stat().st_size)
    if split:
        configuration.update(contentUrl=url + '/content.zip', contentSha256='0' * 64 if name == 'corrupt-content' else hashlib.sha256(content.read_bytes()).hexdigest(), contentBytes=content.stat().st_size)
    config.write_text(json.dumps(configuration))
    shutil.copyfile(config, case_proof / 'config.json')
    try:
        with (case_proof / 'wine.log').open('w') as log:
            process = subprocess.run([wine, win(player), '-batchmode', '-nographics', '-nosound', '-idas3-test-silent',
                                      '-idas3-updater-flow-smoke', win(config), '-logFile', win(case_proof / 'player.log')],
                                     cwd=player.parent, env=run_env, stdout=log, stderr=subprocess.STDOUT, timeout=180)
        flow_path = folder / 'unity-flow.json'
        flow = json.loads(flow_path.read_text()) if flow_path.exists() else {'error': 'No Unity flow report'}
        if flow_path.exists():
            shutil.copyfile(flow_path, case_proof / 'unity-flow.json')
        deadline = time.monotonic() + 15
        while flow.get('reachedShutdown') and not (game / 'restarted.txt').exists() and time.monotonic() < deadline:
            time.sleep(.1)
        applied = all((game / relative).read_bytes() == value for relative, value in targets.items())
        unchanged = all((game / relative).read_bytes() == value for relative, value in originals.items())
        assert applied or unchanged, 'Partial installation in isolated fixture: ' + name
        assert (game / 'userdata/card.json').read_bytes() == b'keep save'
        assert (game / 'rom/keep.bin').read_bytes() == b'keep original'
        if name == 'linked-game-file':
            assert outside.read_bytes() == originals['UnityPlayer.dll']
        if name == 'linked-content-directory':
            assert (outside / 'globalgamemanagers').read_bytes() == originals['InitialDUnity_Data/globalgamemanagers']
        if name == 'linked-cache-root':
            assert (outside / 'sentinel.txt').read_bytes() == b'untouched cache target'
            assert sorted(str(path.relative_to(outside)) for path in outside.rglob('*')) == cache_before
        result = {'name': name, 'exitCode': process.returncode, 'applied': applied, 'unchanged': unchanged,
                  'restarted': (game / 'restarted.txt').exists(), 'downloads': server.downloads, 'flow': flow}
        results.append(result)
        (proof / 'report.json').write_text(json.dumps(report, indent=2))
        print(f"{name}: exit={process.returncode}, applied={applied}, restarted={result['restarted']}, downloads={server.downloads}", flush=True)
        if os.environ.get('IDAS3_FLOW_EXPECT_FIXED') == '1':
            allowed = name not in ('linked-game-file', 'linked-content-directory', 'corrupt-download', 'corrupt-content', 'interrupted-content', 'linked-cache-root', 'unmarked-fixture', 'foreign-game')
            assert applied == allowed and bool(flow.get('reachedShutdown')) == allowed and result['restarted'] == allowed, result
            if allowed:
                assert process.returncode == 0, result
                session = unix(next(entry['session'] for entry in reversed(flow['observations']) if entry['session']))
                status = json.loads((session / 'result.json').read_text())
                assert status['passed'] and status['cleanupSafe'] and not status['rollbackNeeded'], status
                assert hashlib.sha256((session / 'install.exe').read_bytes()).hexdigest() == report['helperSha256']
                assert all(not (session / payload).exists() for payload in ('stage', 'backup', 'game.zip', 'content.zip'))
                result.update(embeddedHelperVerified=True, payloadsCleaned=True)
            if name == 'patch':
                assert server.downloads == ['/patch.zip'], server.downloads
            if name == 'damaged-retained':
                assert server.downloads == ['/patch.zip', '/update.zip'], server.downloads
            if split:
                expected = ['/patch.zip', '/update.zip', '/content.zip'] if name == 'split-patch-fallback' else ['/update.zip', '/content.zip']
                assert server.downloads == expected, server.downloads
            if name in ('linked-cache-root', 'unmarked-fixture', 'foreign-game'):
                assert not server.downloads, server.downloads
            if not allowed:
                assert unchanged and process.returncode != 0, result
            result['passed'] = True
            (proof / 'report.json').write_text(json.dumps(report, indent=2))
    except subprocess.TimeoutExpired:
        # Terminate only this newly created prefix, including a helper that
        # could otherwise outlive the timed-out Unity process.
        subprocess.run(['/usr/lib/wine/wineserver', '-k'], env=env, timeout=15, check=True)
        report['error'] = 'Timed out: ' + name
        (proof / 'report.json').write_text(json.dumps(report, indent=2))
        raise
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)
report['passed'] = os.environ.get('IDAS3_FLOW_EXPECT_FIXED') == '1'
(proof / 'report.json').write_text(json.dumps(report, indent=2))
