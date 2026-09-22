from pathlib import Path
import ctypes,hashlib,importlib.util,json,os,subprocess,uuid,zipfile,time
root=Path(__file__).resolve().parents[2]
base=root/'Tests/Updates'/('installer-patch-'+uuid.uuid4().hex);base.mkdir()
spec=importlib.util.spec_from_file_location('pack',root/'Tools/Build-UpdatePatch.py');pack=importlib.util.module_from_spec(spec);spec.loader.exec_module(pack)
old={'InitialDUnity.exe':b'exe','UnityPlayer.dll':b'engine','InitialDUnity_Data/globalgamemanagers':b'old version','InitialDUnity_Data/Managed/Assembly-CSharp.dll':b'old scripts','InitialDUnity_Data/StreamingAssets/track.bin':b'unchanged scenery'*1000}
new={**old,'InitialDUnity_Data/globalgamemanagers':b'new version','InitialDUnity_Data/Managed/Assembly-CSharp.dll':b'new scripts','InitialDUnity_Data/StreamingAssets/new.bin':b'new content'}
for name,files in [('old',old),('new',new)]:
    with zipfile.ZipFile(base/(name+'.zip'),'w') as z:
        for k,v in files.items():z.writestr(k,v)
report=pack.build(base/'old.zip',base/'new.zip','1.0.0','1.0.1',base)
assert report['changedFiles']==3 and report['bytes']<(base/'new.zip').stat().st_size
with zipfile.ZipFile(report['archive']) as z:patch_files={n:z.read(n) for n in z.namelist()}
assert 'UnityPlayer.dll' not in patch_files and 'InitialDUnity.exe' not in patch_files
shell=os.environ['SystemRoot']+'/System32/WindowsPowerShell/v1.0/powershell.exe'
env={k:v for k,v in os.environ.items() if k.lower()!='psmodulepath'}
results=[]
def case(name, mutate=None, damaged=None, missing=None, good=False, locked=False, concurrent=False):
    folder=base/name;game=folder/'game';session=folder/'session';game.mkdir(parents=True);session.mkdir()
    (folder/'ISOLATED_UPDATE_TEST.txt').write_text('Disposable patch fixture')
    original={**old,'userdata/card.json':b'save','InitialDUnity_Data/StreamingAssets/custom-music/song.mp3':b'music','InitialDUnity_Data/StreamingAssets/replays/run.idreplay':b'replay'}
    if damaged:original[damaged]=b'damaged data'
    if missing:original.pop(missing)
    for n,v in original.items():p=game/n;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(v)
    untouched=(game/'UnityPlayer.dll').stat().st_mtime_ns
    files=dict(patch_files)
    manifest=json.loads(files['update-patch.json'])
    if mutate:mutate(manifest,files)
    files['update-patch.json']=json.dumps(manifest).encode()
    archive=session/'game.zip'
    with zipfile.ZipFile(archive,'w') as z:
        for n,v in files.items():z.writestr(n,v)
    plan=dict(installRoot=str(game),archive=str(archive),sha256=hashlib.sha256(archive.read_bytes()).hexdigest(),parentId=0,parentStartTicks='0',patch=True,baseVersion='1.0.0',targetVersion='1.0.1')
    parent=None;handle=None
    if concurrent:
        import sys
        parent=subprocess.Popen([sys.executable,'-c','input()'],stdin=subprocess.PIPE,creationflags=subprocess.CREATE_NO_WINDOW)
        plan['parentId']=parent.pid
        plan['parentStartTicks']=subprocess.check_output([shell,'-NoProfile','-Command',f'(Get-Process -Id {parent.pid}).StartTime.ToUniversalTime().Ticks.ToString()'],env=env,text=True).strip()
    if locked:
        kernel=ctypes.windll.kernel32;kernel.CreateFileW.restype=ctypes.c_void_p
        handle=kernel.CreateFileW(str(game/'InitialDUnity_Data/Managed/Assembly-CSharp.dll'),0x80000000,1,None,3,0,None)
        assert handle not in (None,ctypes.c_void_p(-1).value)
    (session/'install.json').write_text(json.dumps(plan));(session/'install.ps1').write_bytes((root/'Assets/Resources/UpdateInstaller.ps1.txt').read_bytes())
    try:
        helper=subprocess.Popen([shell,'-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',str(session/'install.ps1'),'-ManifestPath',str(session/'install.json'),'-TestMode'],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,creationflags=subprocess.CREATE_NO_WINDOW)
        if parent:
            deadline=time.time()+20
            while not (session/'ready').exists() and helper.poll() is None and time.time()<deadline:time.sleep(.05)
            assert (session/'ready').exists()
            original['UnityPlayer.dll']=b'concurrent edit';(game/'UnityPlayer.dll').write_bytes(original['UnityPlayer.dll'])
            parent.stdin.write(b'\n');parent.stdin.flush();parent.wait(timeout=5)
        stdout,stderr=helper.communicate(timeout=60)
    finally:
        if handle:kernel.CloseHandle(ctypes.c_void_p(handle))
        if parent and parent.poll() is None:parent.stdin.write(b'\n');parent.stdin.flush();parent.wait(timeout=5)
    assert (helper.returncode==0)==good,(name,stdout,stderr,(session/'error.txt').read_text() if (session/'error.txt').exists() else '')
    expected={**original,**new} if good else original
    for n,v in expected.items():assert (game/n).read_bytes()==v,(name,n)
    assert (session/'patch-invalid').exists()==(not good and not locked and not concurrent),'Patch fallback classification'
    if good:assert (game/'UnityPlayer.dll').stat().st_mtime_ns==untouched,'Unchanged engine rewritten'
    else:assert not (game/'InitialDUnity_Data/StreamingAssets/new.bin').exists()
    results.append(dict(test=name,passed=True));print(name+': PASS',flush=True)
case('clean-patch',good=True)
case('repair-changed-file',damaged='InitialDUnity_Data/Managed/Assembly-CSharp.dll',good=True)
case('damaged-unchanged-file',damaged='InitialDUnity_Data/StreamingAssets/track.bin')
case('missing-unchanged-file',missing='InitialDUnity_Data/StreamingAssets/track.bin')
case('wrong-base',lambda m,f:m.update(baseVersion='0.9.0'))
case('wrong-target',lambda m,f:m.update(targetVersion='1.0.2'))
case('missing-payload',lambda m,f:f.pop('InitialDUnity_Data/StreamingAssets/new.bin'))
case('wrong-payload-hash',lambda m,f:f.update({'InitialDUnity_Data/Managed/Assembly-CSharp.dll':b'bad scripts'}))
case('extra-payload',lambda m,f:f.update({'InitialDUnity_Data/extra.bin':b'extra'}))
case('inventory-traversal',lambda m,f:m['files'].append(dict(path='../outside',sha256='0'*64,size=0,included=False)))
case('inventory-private',lambda m,f:m['files'].append(dict(path='InitialDUnity_Data/userdata/card.json',sha256='0'*64,size=0,included=False)))
case('duplicate-inventory',lambda m,f:m['files'].append(dict(m['files'][0],path=m['files'][0]['path'].lower())))
case('rollback-locked',locked=True)
case('concurrent-change',concurrent=True)
(root/'Tests/Updates/patch-report.json').write_text(json.dumps(dict(passed=True,tests=results,package=report,fixture=str(base)),indent=2))
