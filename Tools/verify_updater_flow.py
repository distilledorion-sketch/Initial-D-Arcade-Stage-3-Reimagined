"""Private Windows acceptance run for the real Unity updater diagnostic scene.

Runs loopback-only fixtures, never the desktop game or a public update. The
diagnostic managed assembly must match the fresh shipping candidate exactly.
"""
import argparse
import functools
import hashlib
import http.server
import json
import os
from pathlib import Path
import subprocess
import threading
import time
import zipfile
import verify_release as verify
import verify_packaged_player as packaged

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def save(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')


class Handler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        self.server.downloads.append(self.path)
        if self.path not in ('/update.zip', '/patch.zip', '/content.zip'):
            self.send_error(404)
            return
        if self.path == '/content.zip' and self.server.interrupt_content:
            data = (Path(self.directory) / 'content.zip').read_bytes()
            self.send_response(200)
            self.send_header('Content-Length', str(len(data)))
            self.end_headers()
            self.wfile.write(data[:len(data)//2])
            self.wfile.flush()
            self.close_connection = True
            return
        super().do_GET()

    def log_message(self, *args):
        pass


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--diagnostic-player', required=True, type=Path)
    parser.add_argument('--build-evidence', required=True, type=Path)
    parser.add_argument('--child', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    if os.name != 'nt' or not __debug__:
        parser.error('Windows with assertions enabled is required')
    diagnostic = args.diagnostic_player.resolve(strict=True)
    evidence = args.build_evidence.resolve(strict=True)
    child = args.child.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    report = dict(passed=False, scope='Same managed assembly in isolated diagnostic scene, real Unity downloader and embedded helper; not public-release or Internet acceptance.', tests=[])
    save(output / 'report.json', report)
    try:
        inventory = verify.candidate_inventory(evidence / 'player')
        build, version = packaged.require_build_evidence(evidence, inventory)
        assembly = 'InitialDUnity_Data/Managed/Assembly-CSharp.dll'
        expected = inventory['files'][assembly]['sha256']
        assert sha(evidence / 'player' / assembly) == expected
        assert sha(diagnostic / assembly) == expected, 'Diagnostic managed assembly differs from fresh candidate'
        assert sha(diagnostic / 'UnityPlayer.dll') == inventory['files']['UnityPlayer.dll']['sha256']
        helper_sha = sha(ROOT / 'Assets/Resources/UpdateInstaller.exe.bytes')
        report.update(candidateInventorySha256=inventory['inventorySha256'], assemblySha256=expected,
                      helperSha256=helper_sha, applicationVersion=version,
                      buildSummarySha256=sha(evidence / 'summary.json'), runnerSha256=sha(__file__))
        for name in ('full', 'patch', 'damaged-retained', 'corrupt-full', 'corrupt-patch',
                     'split-full', 'split-patch-fallback', 'corrupt-content', 'interrupted-content'):
            case = output / name
            game = case / 'game space 日本'
            game.mkdir(parents=True)
            temporary = case / 'temp'
            temporary.mkdir()
            (case / 'ISOLATED_UPDATE_TEST.txt').write_text('Owned disposable updater fixture', encoding='utf-8')
            old = {'InitialDUnity.exe': child.read_bytes(), 'UnityPlayer.dll': b'old engine',
                   'InitialDUnity_Data/globalgamemanagers': b'old metadata',
                   'InitialDUnity_Data/Managed/Assembly-CSharp.dll': b'old scripts'}
            target = {**old, 'UnityPlayer.dll': b'new engine',
                      'InitialDUnity_Data/globalgamemanagers': b'new metadata',
                      'InitialDUnity_Data/Managed/Assembly-CSharp.dll': b'new scripts'}
            split = name in ('split-full', 'split-patch-fallback', 'corrupt-content', 'interrupted-content')
            courses = {'InitialDUnity_Data/StreamingAssets/GUNSAI/menu.idastex': b'new Gunsai',
                       'InitialDUnity_Data/StreamingAssets/ODAWARA/menu.idastex': b'new Odawara'} if split else {}
            old.update({key: b'old course' for key in courses})
            target.update(courses)
            private = {'userdata/card.json': b'keep save', 'rom/keep.bin': b'keep original',
                       'Custom Music/keep.mp3': b'keep music', 'replays/keep.idr': b'keep replay'}
            for relative, data in {**old, **private}.items():
                destination = game / relative
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes(data)
            archive = case / 'update.zip'
            with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED) as package:
                for relative, data in target.items():
                    if relative not in courses:
                        package.writestr(relative, data)
            content = case / 'content.zip'
            if split:
                with zipfile.ZipFile(content, 'x', compression=zipfile.ZIP_DEFLATED) as package:
                    for relative, data in courses.items():
                        package.writestr(relative, data)
            patch = None
            if name in ('patch', 'damaged-retained', 'corrupt-patch', 'split-patch-fallback'):
                patch = case / 'patch.zip'
                manifest = dict(schema=1, baseVersion=version, targetVersion='99.0.0', files=[])
                with zipfile.ZipFile(patch, 'w', compression=zipfile.ZIP_DEFLATED) as package:
                    for relative, data in target.items():
                        included = data != old[relative]
                        manifest['files'].append(dict(path=relative, size=len(data), sha256=hashlib.sha256(data).hexdigest(), included=included))
                        if included:
                            package.writestr(relative, data)
                    package.writestr('update-patch.json', json.dumps(manifest))
                assert patch.stat().st_size < archive.stat().st_size
            if name in ('damaged-retained', 'split-patch-fallback'):
                old['InitialDUnity.exe'] += b'changed after installation'
                (game / 'InitialDUnity.exe').write_bytes(old['InitialDUnity.exe'])
            server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), functools.partial(Handler, directory=str(case)))
            server.downloads = []
            server.interrupt_content = name == 'interrupted-content'
            server_thread = threading.Thread(target=server.serve_forever, daemon=True)
            server_thread.start()
            config = dict(gameRoot=str(game), url=f'http://127.0.0.1:{server.server_port}/update.zip',
                          sha256=('0' * 64 if name == 'corrupt-full' else sha(archive)), bytes=archive.stat().st_size)
            if patch:
                config.update(patchUrl=f'http://127.0.0.1:{server.server_port}/patch.zip',
                              patchSha256=('0' * 64 if name == 'corrupt-patch' else sha(patch)), patchBytes=patch.stat().st_size)
            if split:
                config.update(contentUrl=f'http://127.0.0.1:{server.server_port}/content.zip',
                              contentSha256='0' * 64 if name == 'corrupt-content' else sha(content), contentBytes=content.stat().st_size)
            save(case / 'config.json', config)
            env = dict(os.environ, TEMP=str(temporary), TMP=str(temporary))
            entry = dict(name=name, passed=False)
            report['tests'].append(entry)
            save(output / 'report.json', report)
            try:
                with (case / 'stdout.log').open('wb') as log:
                    process = subprocess.run([str(diagnostic / 'InitialDUnity.exe'), '-batchmode', '-nographics',
                        '-idas3-updater-flow-smoke', str(case / 'config.json'), '-logFile', str(case / 'player.log')],
                        cwd=diagnostic, env=env, stdout=log, stderr=subprocess.STDOUT,
                        creationflags=subprocess.CREATE_NO_WINDOW, timeout=180)
                flow = json.loads((case / 'unity-flow.json').read_text(encoding='utf-8-sig'))
                assert flow['applicationVersion'] == flow['installedVersion'] == version and flow['targetVersion'] == '99.0.0'
                assert Path(flow['tempPath']).resolve() == temporary
                cache = temporary / 'InitialDUpdates'
                assert Path(flow['cacheRoot']).resolve() == cache
                sessions = {Path(row['session']).resolve() for row in flow['observations'] if row.get('session')}
                assert sessions and all(path.parent == cache for path in sessions)
                entry.update(exitCode=process.returncode, reachedShutdown=flow['reachedShutdown'], downloads=server.downloads)
                valid = name not in ('corrupt-full', 'corrupt-patch', 'corrupt-content', 'interrupted-content')
                if valid:
                    assert process.returncode == 0 and flow['reachedShutdown'] and not flow.get('error'), flow
                    deadline = time.monotonic() + 30
                    while not (game / 'restarted.txt').exists() and time.monotonic() < deadline:
                        time.sleep(.1)
                    assert (game / 'restarted.txt').read_text() == 'restarted'
                    assert all((game / relative).read_bytes() == data for relative, data in target.items())
                    completed = [path for path in sessions if (path / 'result.json').exists()]
                    assert len(completed) == 1, completed
                    session = completed[0]
                    assert sha(session / 'install.exe') == helper_sha
                    status = json.loads((session / 'result.json').read_text())
                    assert status['passed'] and status['cleanupSafe'] and not status['rollbackNeeded']
                    for payload in ('stage', 'backup', 'game.zip', 'content.zip'):
                        assert not (session / payload).exists(), payload
                    entry.update(installedHashesVerified=True, restarted=True, embeddedHelperVerified=True, payloadsCleaned=True)
                else:
                    assert process.returncode != 0 and not flow['reachedShutdown']
                    failure = 'course download interrupted' if name == 'interrupted-content' else 'verification failed'
                    assert failure in flow['error'].lower(), flow
                    assert all((game / relative).read_bytes() == data for relative, data in old.items())
                    assert not (game / 'restarted.txt').exists()
                    assert all(not (path / 'ready').exists() for path in sessions)
                    entry.update(unchangedAfterRejection=True, installerNotPrepared=True)
                expected_downloads = ['/patch.zip', '/update.zip'] if name in ('damaged-retained', 'split-patch-fallback') else ['/patch.zip'] if patch else ['/update.zip']
                if split:
                    expected_downloads += ['/content.zip']
                assert server.downloads == expected_downloads, server.downloads
                assert all((game / relative).read_bytes() == data for relative, data in private.items())
                entry.update(passed=True, privateFilesPreserved=True)
                print('PASS ' + name, flush=True)
            except Exception as error:
                entry['error'] = str(error)
                raise
            finally:
                server.shutdown()
                server.server_close()
                server_thread.join(timeout=5)
                save(output / 'report.json', report)
        after = verify.candidate_inventory(evidence / 'player')
        assert after['inventorySha256'] == inventory['inventorySha256'], 'Candidate changed during updater verification'
        assert sha(diagnostic / assembly) == expected and sha(ROOT / 'Assets/Resources/UpdateInstaller.exe.bytes') == helper_sha
        report.update(passed=True, count=len(report['tests']), candidateUnchanged=True)
    except Exception as error:
        report['error'] = str(error)
        raise
    finally:
        save(output / 'report.json', report)


if __name__ == '__main__':
    main()
