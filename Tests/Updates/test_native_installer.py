from pathlib import Path
import ctypes,hashlib,json,os,struct,subprocess,time,uuid,zipfile
root=Path(__file__).resolve().parents[2];proof=root/'Verification/wine-updater-20260922';proof.mkdir(exist_ok=True)
helper=root/'Native/build-update-helper/Idas3UpdateInstaller.exe';child=root/'Native/build-update-helper/UpdateTestChild.exe';driver=proof/'StageDriver.exe'
csc=Path(os.environ['SystemRoot'])/'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
r=subprocess.run([str(csc),'/nologo','/out:'+str(driver),'/r:System.IO.Compression.dll','/r:System.IO.Compression.FileSystem.dll','/r:System.Web.Extensions.dll',str(root/'Assets/Scripts/Idas3UpdateStaging.cs'),str(root/'Tests/Updates/StageDriver.cs')],capture_output=True,text=True);assert r.returncode==0,r.stdout+r.stderr
base=root/'Tests/Updates'/('installer-native-'+uuid.uuid4().hex);base.mkdir()
required={'InitialDUnity.exe':b'new exe','UnityPlayer.dll':b'new dll','InitialDUnity_Data/globalgamemanagers':b'new version','InitialDUnity_Data/Managed/Assembly-CSharp.dll':b'new scripts'}
results=[]
def case(name,patch=False,mutate=None,damage=False,missing=False,locked=False,tamper=False,concurrent=False,restart=False,good=False,bad_hash=False):
    folder=base/name;game=folder/'game space 日本';session=folder/'session space';game.mkdir(parents=True);session.mkdir();(folder/'ISOLATED_UPDATE_TEST.txt').write_text('Disposable native installer fixture')
    original={**{k:b'old '+v for k,v in required.items()},'InitialDUnity_Data/StreamingAssets/retained.bin':b'unchanged scenery','userdata/card.json':b'save','custom-music/song.mp3':b'music','replays/test.idreplay':b'replay'}
    target={**required,'InitialDUnity_Data/StreamingAssets/retained.bin':b'unchanged scenery','READ ME.txt':b'new readme'}
    if restart or concurrent:original['InitialDUnity.exe']=child.read_bytes();target['InitialDUnity.exe']=child.read_bytes()
    if damage:original['InitialDUnity_Data/StreamingAssets/retained.bin']=b'corrupt'
    if missing:original.pop('InitialDUnity_Data/StreamingAssets/retained.bin')
    for n,v in original.items():p=game/n;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(v)
    entries=list(target.items())
    if patch:
        files=[dict(path=n,sha256=hashlib.sha256(v).hexdigest(),size=len(v),included=n!='InitialDUnity_Data/StreamingAssets/retained.bin') for n,v in target.items()]
        entries=[(n,v) for n,v in entries if n!='InitialDUnity_Data/StreamingAssets/retained.bin']+[('update-patch.json',json.dumps(dict(schema=1,baseVersion='1.0.0',targetVersion='1.0.1',files=files)).encode())]
    if mutate:entries=mutate(entries)
    archive=session/'game.zip'
    with zipfile.ZipFile(archive,'w') as z:
        for n,v in entries:z.writestr(n,v)
    parent=None;pid=0;stamp=0;handle=None
    if restart or concurrent:
        parent=subprocess.Popen([str(game/'InitialDUnity.exe'),'-wait'],cwd=game,creationflags=subprocess.CREATE_NO_WINDOW)
        deadline=time.time()+10
        while not (game/'parent.txt').exists() and time.time()<deadline:time.sleep(.05)
        pid,stamp=map(int,(game/'parent.txt').read_text().split())
    staged=subprocess.run([str(driver),str(game),str(session),('0'*64 if bad_hash else hashlib.sha256(archive.read_bytes()).hexdigest()),'patch' if patch else 'full','1.0.0','1.0.1',str(pid),str(stamp)],capture_output=True,text=True)
    if locked:
        kernel=ctypes.windll.kernel32;kernel.CreateFileW.restype=ctypes.c_void_p
        handle=kernel.CreateFileW(str(game/'UnityPlayer.dll'),0x80000000,1,None,3,0,None);assert handle not in (None,ctypes.c_void_p(-1).value)
    try:
        code=staged.returncode;output=staged.stderr
        if code==0:
            if tamper:(session/'stage/UnityPlayer.dll').write_bytes(b'tampered')
            args=[str(helper),str(session/'install.plan')]+([] if restart else ['--test'])
            proc=subprocess.Popen(args,stdout=subprocess.PIPE,stderr=subprocess.PIPE,creationflags=subprocess.CREATE_NO_WINDOW)
            if parent:
                deadline=time.time()+15
                while not (session/'ready').exists() and proc.poll() is None and time.time()<deadline:time.sleep(.05)
                assert (session/'ready').exists(),(name,(session/'error.txt').read_text() if (session/'error.txt').exists() else '')
                assert parent.poll() is None
                assert (game/'UnityPlayer.dll').read_bytes()==original['UnityPlayer.dll'],'Wrote before process exit'
                if concurrent:
                    changed=concurrent if isinstance(concurrent,str) else 'UnityPlayer.dll'
                    previous=(game/changed).stat()
                    original[changed]=b'X'*len(original[changed]);(game/changed).write_bytes(original[changed])
                    # Metadata alone cannot prove integrity. The final pass must
                    # detect retained-file edits even with size/timestamps restored.
                    os.utime(game/changed,ns=(previous.st_atime_ns,previous.st_mtime_ns))
                (game/'release-parent').write_text('exit');parent.wait(timeout=10)
            stdout,stderr=proc.communicate(timeout=30);code=proc.returncode;output=stderr.decode(errors='replace')
            if (session/'error.txt').exists():output+=(session/'error.txt').read_text()
        assert (code==0)==good,(name,code,output)
        expected={**original,**target} if good else original
        for n,v in expected.items():assert (game/n).read_bytes()==v,(name,n)
        if not good:assert not (game/'READ ME.txt').exists(),name
        if restart:
            deadline=time.time()+10
            while not (game/'restarted.txt').exists() and time.time()<deadline:time.sleep(.05)
            assert (game/'restarted.txt').read_text()=='restarted'
        results.append(dict(test=name,passed=True));print(name+': PASS',flush=True)
    finally:
        if handle:kernel.CloseHandle(ctypes.c_void_p(handle))
        if parent and parent.poll() is None:(game/'release-parent').write_text('exit');parent.wait(timeout=10)
case('full',good=True)
case('bad-checksum',bad_hash=True)
case('patch',patch=True,good=True)
case('damaged-retained',patch=True,damage=True)
case('missing-retained',patch=True,missing=True)
case('traversal',mutate=lambda x:x+[('../escape',b'bad')])
case('private',mutate=lambda x:x+[('InitialDUnity_Data/userdata/save',b'bad')])
case('duplicate',mutate=lambda x:x+[('initialdunity.exe',b'bad')])
case('reserved',mutate=lambda x:x+[('InitialDUnity_Data/NUL',b'bad')])
case('incomplete',mutate=lambda x:[e for e in x if e[0]!='UnityPlayer.dll'])
link=zipfile.ZipInfo('InitialDUnity_Data/link');link.create_system=3;link.external_attr=0o120777<<16
case('symlink',mutate=lambda x:x+[(link,b'../escape')])
case('tampered-stage',tamper=True)
case('rollback-locked',locked=True)
case('concurrent',concurrent=True)
case('concurrent-retained-same-metadata',concurrent='InitialDUnity_Data/StreamingAssets/retained.bin')
case('process-handoff-restart',restart=True,good=True)
(proof/'native-windows-report.json').write_text(json.dumps(dict(passed=True,tests=results,fixture=str(base)),indent=2))
