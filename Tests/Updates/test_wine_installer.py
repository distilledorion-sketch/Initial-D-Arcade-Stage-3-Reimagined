# Run on Linux: xvfb-run -a python3 Tests/Updates/test_wine_installer.py
from pathlib import Path
import hashlib,json,os,shutil,struct,subprocess,tempfile,time
root=Path(__file__).resolve().parents[2];proof=root/'Verification/wine-updater-20260922'
base=Path(tempfile.mkdtemp(prefix='idas3-updater-wine-',dir=str(Path.home())));prefix=base/'prefix'
env=dict(os.environ,WINEPREFIX=str(prefix),WINEDEBUG='-all',WINEDLLOVERRIDES='mscoree,mshtml=')
wine='/usr/lib/wine/wine64'
helper=root/'Native/build-update-helper/Idas3UpdateInstaller.exe';child=root/'Native/build-update-helper/UpdateTestChild.exe'
def win(path):return 'Z:'+str(path.resolve()).replace('/','\\')
def text(s):return struct.pack('<I',len(s.encode('utf-16le'))//2)+s.encode('utf-16le')
results=[]
def case(name,corrupt=False,concurrent=False,handoff=False,linked=False):
    folder=base/name;game=folder/'game space 日本';session=folder/'session';game.mkdir(parents=True);session.mkdir();(folder/'ISOLATED_UPDATE_TEST.txt').write_text('Disposable Wine updater test')
    originals={'InitialDUnity.exe':child.read_bytes(),'UnityPlayer.dll':b'old engine','InitialDUnity_Data/globalgamemanagers':b'old version','InitialDUnity_Data/Managed/Assembly-CSharp.dll':b'old scripts','InitialDUnity_Data/StreamingAssets/track.bin':b'retained track'}
    targets={**originals,'UnityPlayer.dll':b'new engine','InitialDUnity_Data/globalgamemanagers':b'new version','InitialDUnity_Data/Managed/Assembly-CSharp.dll':b'new scripts'}
    for n,v in originals.items():p=game/n;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(v)
    (game/'userdata').mkdir();(game/'userdata/card.json').write_bytes(b'personal-save')
    parent=None;pid=0;stamp=0
    if handoff or concurrent:
        parent=subprocess.Popen([wine,win(game/'InitialDUnity.exe'),'-wait'],cwd=game,env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        deadline=time.time()+60
        while not (game/'parent.txt').exists() and time.time()<deadline:time.sleep(.1)
        assert (game/'parent.txt').exists(),name
        pid,stamp=map(int,(game/'parent.txt').read_text().split())
    plan=b'IDUPD002'+text(win(game))+struct.pack('<IQI',pid,stamp,len(targets))
    for n,v in targets.items():
        changed=v!=originals[n]
        plan+=text(n)+struct.pack('<BBQ',changed,True,len(v))+hashlib.sha256(originals[n]).digest()+hashlib.sha256(v).digest()
        if changed:p=session/'stage'/n;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(v)
    (session/'install.plan').write_bytes(plan);(session/'backup').mkdir()
    if linked:
        external=folder/'outside.track';external.write_bytes(b'retained track')
        link=game/'InitialDUnity_Data/StreamingAssets/track.bin';link.unlink();link.symlink_to(external)
    if corrupt:(session/'stage/UnityPlayer.dll').write_bytes(b'corrupt payload')
    args=[wine,win(helper),win(session/'install.plan')]+([] if handoff else ['--test'])
    proc=subprocess.Popen(args,env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    if parent:
        deadline=time.time()+40
        while not (session/'ready').exists() and proc.poll() is None and time.time()<deadline:time.sleep(.05)
        assert (session/'ready').exists(),(name,(session/'error.txt').read_text() if (session/'error.txt').exists() else '')
        assert (game/'UnityPlayer.dll').read_bytes()==originals['UnityPlayer.dll']
        if concurrent:originals['UnityPlayer.dll']=b'concurrent edit';(game/'UnityPlayer.dll').write_bytes(originals['UnityPlayer.dll'])
        (game/'release-parent').write_text('exit');parent.communicate(timeout=20)
    stdout,stderr=proc.communicate(timeout=80)
    good=not corrupt and not concurrent and not linked
    assert (proc.returncode==0)==good,(name,proc.returncode,stderr.decode(errors='replace'),(session/'error.txt').read_text() if (session/'error.txt').exists() else '')
    expected=targets if good else originals
    for n,v in expected.items():assert (game/n).read_bytes()==v,(name,n)
    assert (game/'userdata/card.json').read_bytes()==b'personal-save'
    if handoff:
        deadline=time.time()+20
        while not (game/'restarted.txt').exists() and time.time()<deadline:time.sleep(.05)
        assert (game/'restarted.txt').read_text()=='restarted'
    results.append(dict(test=name,passed=True));print(name+': PASS',flush=True)
case('clean')
case('linked-game-file',linked=True)
case('tampered-stage',corrupt=True)
case('concurrent-change',concurrent=True)
case('wait-apply-restart',handoff=True)
report=dict(passed=True,tests=results,wine=subprocess.check_output([wine,'--version'],env=env,text=True).strip(),prefix=str(prefix),externalPowerShellOrDotNetRequired=False)
(proof/'wine-report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report),flush=True)
