from pathlib import Path
import ctypes, hashlib, json, os, subprocess, uuid, zipfile

base=Path(__file__).resolve().parent/('installer-'+uuid.uuid4().hex)
base.mkdir()
(base/'ISOLATED_UPDATE_TEST.txt').write_text('Disposable updater fixture; no real installation or saves.')
source=Path(__file__).resolve().parents[2]/'Assets/Resources/UpdateInstaller.ps1.txt'
shell=os.environ['SystemRoot']+'/System32/WindowsPowerShell/v1.0/powershell.exe'
required={'InitialDUnity.exe':b'new exe','UnityPlayer.dll':b'new player','InitialDUnity_Data/globalgamemanagers':b'new data','InitialDUnity_Data/Managed/Assembly-CSharp.dll':b'new scripts'}
results=[]
def fixture(name, modify=None, bad_hash=False, lock=False):
    case=base/name;case.mkdir();install=case/'game';install.mkdir();session=case/'session';session.mkdir()
    (case/'ISOLATED_UPDATE_TEST.txt').write_text('Disposable fixture')
    originals={**{k:b'old '+v for k,v in required.items()},'userdata/cards/save.json':b'personal save','replays/my.idreplay':b'replay','custom-music/song.mp3':b'music','my-settings.txt':b'personal config'}
    for key,value in originals.items():
        p=install/key;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(value)
    items=list(required.items())+[('READ ME.txt',b'new readme')]
    if modify:items=modify(items)
    archive=session/'game.zip'
    with zipfile.ZipFile(archive,'w') as z:
        for path,value in items:z.writestr(path,value)
    digest=hashlib.sha256(archive.read_bytes()).hexdigest()
    (session/'install.json').write_text(json.dumps(dict(installRoot=str(install),archive=str(archive),sha256='0'*64 if bad_hash else digest,parentId=0,parentStartTicks='0')))
    (session/'install.ps1').write_bytes(source.read_bytes())
    handle=None
    if lock:
        kernel=ctypes.windll.kernel32
        kernel.CreateFileW.restype=ctypes.c_void_p
        handle=kernel.CreateFileW(str(install/'UnityPlayer.dll'),0x80000000,1,None,3,0,None)
        assert handle not in (None,ctypes.c_void_p(-1).value)
    try:
        env={k:v for k,v in os.environ.items() if k.lower()!='psmodulepath'}
        run=subprocess.run([shell,'-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',str(session/'install.ps1'),'-ManifestPath',str(session/'install.json'),'-TestMode'],env=env,capture_output=True,text=True,timeout=90)
    finally:
        if handle:kernel.CloseHandle(ctypes.c_void_p(handle))
    success=name=='success'
    assert (run.returncode==0)==success,(name,run.returncode,run.stdout,run.stderr,(session/'error.txt').read_text() if (session/'error.txt').exists() else '')
    for key,value in originals.items():
        expected=required[key] if success and key in required else value
        assert (install/key).read_bytes()==expected,(name,key,'changed unexpectedly')
    if not success:
        assert not (install/'READ ME.txt').exists(),name
        if lock:assert 'Rollback needs attention' not in (session/'error.txt').read_text()
    results.append(dict(test=name,passed=True));print(name+': PASS',flush=True)
fixture('success')
fixture('bad-checksum',bad_hash=True)
fixture('parent-traversal',lambda x:x+[('../escape.txt',b'bad')])
fixture('mixed-traversal',lambda x:x+[('InitialDUnity_Data/../../escape.txt',b'bad')])
fixture('absolute',lambda x:x+[('C:/escape.txt',b'bad')])
fixture('duplicate',lambda x:x+[('initialdunity.exe',b'bad')])
fixture('reserved',lambda x:x+[('InitialDUnity_Data/NUL',b'bad')])
fixture('trailing-dot',lambda x:x+[('InitialDUnity_Data/file.',b'bad')])
fixture('personal-file',lambda x:x+[('InitialDUnity_Data/userdata/cards.json',b'bad')])
fixture('incomplete',lambda x:[p for p in x if p[0]!='UnityPlayer.dll'])
link=zipfile.ZipInfo('InitialDUnity_Data/link');link.create_system=3;link.external_attr=(0o120777<<16)
fixture('symlink',lambda x:x+[(link,b'../../escape')])
fixture('rollback-locked-file',lock=True)
assert not (base/'escape.txt').exists()
(Path(__file__).resolve().parent/'installer-report.json').write_text(json.dumps(dict(passed=True,tests=results,fixture=str(base)),indent=2))
