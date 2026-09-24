from pathlib import Path
import ctypes,hashlib,json,os,struct,subprocess,time,uuid,zipfile
root=Path(__file__).resolve().parents[2];proof=root/'Verification/wine-updater-20260922';proof.mkdir(exist_ok=True)
helper=root/'Native/build-update-helper/Idas3UpdateInstaller.exe';child=root/'Native/build-update-helper/UpdateTestChild.exe';driver=proof/'StageDriver.exe'
csc=Path(os.environ['SystemRoot'])/'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
r=subprocess.run([str(csc),'/nologo','/out:'+str(driver),'/r:System.IO.Compression.dll','/r:System.IO.Compression.FileSystem.dll','/r:System.Web.Extensions.dll',str(root/'Assets/Scripts/Idas3UpdateStaging.cs'),str(root/'Tests/Updates/StageDriver.cs')],capture_output=True,text=True);assert r.returncode==0,r.stdout+r.stderr
base=root/'Tests/Updates'/('installer-native-'+uuid.uuid4().hex);base.mkdir()
required={'InitialDUnity.exe':b'new exe','UnityPlayer.dll':b'new dll','InitialDUnity_Data/globalgamemanagers':b'new version','InitialDUnity_Data/Managed/Assembly-CSharp.dll':b'new scripts'}
rom_files={
    'rom/README.txt':b'local ROM instructions',
    'rom/gds-0033.chd':b'private CHD fixture; not a valid dump',
    'rom/gds-0033.cue':b'private CUE fixture',
    **{f'rom/gds-0033-track{i}.bin':f'private track {i} fixture'.encode() for i in range(1,4)},
}
results=[]
kernel=ctypes.windll.kernel32
kernel.CreateFileW.argtypes=[ctypes.c_wchar_p,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_void_p,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_void_p]
kernel.CreateFileW.restype=ctypes.c_void_p
kernel.CloseHandle.argtypes=[ctypes.c_void_p]
invalid_handle=ctypes.c_void_p(-1).value

def assert_lease_held(session):
    handle=kernel.CreateFileW(str(session/'.cleanup-lock'),0xC0000000,0,None,3,0,None)
    if handle not in (None,invalid_handle):kernel.CloseHandle(handle);raise AssertionError('Active update session was not leased')

def dismiss_error(proc):
    user=ctypes.windll.user32
    callback=ctypes.WINFUNCTYPE(ctypes.c_bool,ctypes.c_void_p,ctypes.c_void_p)
    user.GetWindowThreadProcessId.argtypes=[ctypes.c_void_p,ctypes.POINTER(ctypes.c_uint32)]
    user.GetClassNameW.argtypes=[ctypes.c_void_p,ctypes.c_wchar_p,ctypes.c_int]
    user.GetWindowTextW.argtypes=[ctypes.c_void_p,ctypes.c_wchar_p,ctypes.c_int]
    user.PostMessageW.argtypes=[ctypes.c_void_p,ctypes.c_uint32,ctypes.c_size_t,ctypes.c_ssize_t]
    found=[]
    def close(hwnd,_):
        pid=ctypes.c_uint32();user.GetWindowThreadProcessId(hwnd,ctypes.byref(pid))
        if pid.value==proc.pid:
            title=ctypes.create_unicode_buffer(128);kind=ctypes.create_unicode_buffer(128)
            user.GetWindowTextW(hwnd,title,128);user.GetClassNameW(hwnd,kind,128)
            if title.value=='Initial D update' and kind.value=='#32770':
                assert user.PostMessageW(hwnd,0x10,0,0),'Cannot close updater test dialog'
                found.append(True)
        return True
    deadline=time.time()+10
    while proc.poll() is None and time.time()<deadline:
        user.EnumWindows(callback(close),0);time.sleep(.05)
    assert found,'Missing updater restart error dialog'

def changed_plan_paths(plan):
    data=plan.read_bytes();offset=8
    def read_string():
        nonlocal offset
        length=struct.unpack_from('<I',data,offset)[0];offset+=4
        value=data[offset:offset+length*2].decode('utf-16le');offset+=length*2;return value
    read_string();offset+=12;count=struct.unpack_from('<I',data,offset)[0];offset+=4;paths=[]
    for _ in range(count):
        path=read_string();changed=data[offset];offset+=74
        if changed:paths.append(path.replace('\\','/'))
    return paths
def case(name,patch=False,mutate=None,damage=False,missing=False,locked=False,tamper=False,concurrent=False,restart=False,good=False,bad_hash=False,restart_failure=False,rollback_failure=False):
    folder=base/name;game=folder/'game space 日本';session=folder/'session space';game.mkdir(parents=True);session.mkdir();(folder/'ISOLATED_UPDATE_TEST.txt').write_text('Disposable native installer fixture')
    original={**{k:b'old '+v for k,v in required.items()},'InitialDUnity_Data/StreamingAssets/retained.bin':b'unchanged scenery','userdata/card.json':b'save','custom-music/song.mp3':b'music','replays/test.idreplay':b'replay',**rom_files}
    target={**required,'InitialDUnity_Data/StreamingAssets/retained.bin':b'unchanged scenery','READ ME.txt':b'new readme'}
    if restart or concurrent or rollback_failure:original['InitialDUnity.exe']=child.read_bytes();target['InitialDUnity.exe']=child.read_bytes()
    if restart_failure:target['InitialDUnity.exe']=b'not a Windows executable'
    if damage:original['InitialDUnity_Data/StreamingAssets/retained.bin']=b'corrupt'
    if missing:original.pop('InitialDUnity_Data/StreamingAssets/retained.bin')
    for n,v in original.items():p=game/n;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(v)
    rom_times={name:(game/name).stat().st_mtime_ns for name in rom_files}
    entries=list(target.items())
    if patch:
        files=[dict(path=n,sha256=hashlib.sha256(v).hexdigest(),size=len(v),included=n!='InitialDUnity_Data/StreamingAssets/retained.bin') for n,v in target.items()]
        entries=[(n,v) for n,v in entries if n!='InitialDUnity_Data/StreamingAssets/retained.bin']+[('update-patch.json',json.dumps(dict(schema=1,baseVersion='1.0.0',targetVersion='1.0.1',files=files)).encode())]
    if mutate:entries=mutate(entries)
    archive=session/'game.zip'
    with zipfile.ZipFile(archive,'w') as z:
        for n,v in entries:z.writestr(n,v)
    parent=None;pid=0;stamp=0;handle=None;proc=None;failed_restore=None
    if restart or concurrent or rollback_failure:
        parent=subprocess.Popen([str(game/'InitialDUnity.exe'),'-wait'],cwd=game,creationflags=subprocess.CREATE_NO_WINDOW)
        deadline=time.time()+10
        while not (game/'parent.txt').exists() and time.time()<deadline:time.sleep(.05)
        pid,stamp=map(int,(game/'parent.txt').read_text().split())
    staged=subprocess.run([str(driver),str(game),str(session),('0'*64 if bad_hash else hashlib.sha256(archive.read_bytes()).hexdigest()),'patch' if patch else 'full','1.0.0','1.0.1',str(pid),str(stamp)],capture_output=True,text=True)
    if locked:
        handle=kernel.CreateFileW(str(game/'UnityPlayer.dll'),0x80000000,1,None,3,0,None);assert handle not in (None,invalid_handle)
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
                assert_lease_held(session)
                # A second helper must not classify the active session as safe
                # for cleanup when it cannot acquire installer ownership.
                duplicate=subprocess.run(args,capture_output=True,timeout=10)
                assert duplicate.returncode!=0 and not (session/'result.json').exists() and not (session/'error.txt').exists()
                if concurrent:
                    changed=concurrent if isinstance(concurrent,str) else 'UnityPlayer.dll'
                    previous=(game/changed).stat()
                    original[changed]=b'X'*len(original[changed]);(game/changed).write_bytes(original[changed])
                    # Metadata alone cannot prove integrity. The final pass must
                    # detect retained-file edits even with size/timestamps restored.
                    os.utime(game/changed,ns=(previous.st_atime_ns,previous.st_mtime_ns))
                if rollback_failure:
                    changed=changed_plan_paths(session/'install.plan');assert len(changed)>1
                    failed_restore=changed[0]
                    assert failed_restore in original,'Rollback test requires an existing file'
                    (session/'backup'/failed_restore).write_bytes(b'corrupt recovery backup')
                    (session/'stage'/changed[1]).write_bytes(b'corrupt later payload')
                (game/'release-parent').write_text('exit');parent.wait(timeout=10)
            if restart_failure:
                deadline=time.time()+15
                while not (session/'error.txt').exists() and proc.poll() is None and time.time()<deadline:time.sleep(.05)
                assert (session/'error.txt').exists(),'Missing restart failure'
                assert_lease_held(session)
                assert all(not (session/n).exists() for n in ('stage','backup','game.zip')),'Restart error retained payloads'
                dismiss_error(proc)
            stdout,stderr=proc.communicate(timeout=30);code=proc.returncode;output=stderr.decode(errors='replace')
            if (session/'error.txt').exists():output+=(session/'error.txt').read_text()
            terminal=json.loads((session/'result.json').read_text())
            assert terminal['schema']==1 and terminal['passed']==good and terminal['cleanupSafe']==(not rollback_failure) and terminal['rollbackNeeded']==rollback_failure,(name,terminal)
            assert all((session/n).exists()==rollback_failure for n in ('stage','backup','game.zip')),(name,'Payload cleanup/retention failed')
            # A stale plan retry must preserve terminal status/recovery data.
            retry=subprocess.run(args,capture_output=True,timeout=10)
            assert retry.returncode!=0 and json.loads((session/'result.json').read_text())==terminal
            assert all((session/n).exists()==rollback_failure for n in ('stage','backup','game.zip'))
        assert (code==0)==(good and not restart_failure),(name,code,output)
        expected={**original,**target} if good else dict(original)
        if rollback_failure:expected[failed_restore]=target[failed_restore]
        for n,v in expected.items():assert (game/n).read_bytes()==v,(name,n)
        for n,stamp in rom_times.items():assert (game/n).stat().st_mtime_ns==stamp,(name,'ROM timestamp changed',n)
        if not good:assert not (game/'READ ME.txt').exists(),name
        if restart and not restart_failure:
            deadline=time.time()+10
            while not (game/'restarted.txt').exists() and time.time()<deadline:time.sleep(.05)
            assert (game/'restarted.txt').read_text()=='restarted'
        results.append(dict(test=name,passed=True));print(name+': PASS',flush=True)
    finally:
        if handle:kernel.CloseHandle(handle)
        if proc and proc.poll() is None:proc.kill();proc.wait(timeout=10)
        if parent and parent.poll() is None:(game/'release-parent').write_text('exit');parent.wait(timeout=10)
case('full',good=True)
case('bad-checksum',bad_hash=True)
case('patch',patch=True,good=True)
case('damaged-retained',patch=True,damage=True)
case('missing-retained',patch=True,missing=True)
case('traversal',mutate=lambda x:x+[('../escape',b'bad')])
case('private',mutate=lambda x:x+[('InitialDUnity_Data/userdata/save',b'bad')])
case('rom-payload',mutate=lambda x:x+[('rom/gds-0033.chd',b'must not replace private ROM')])
case('rom-readme-entry',mutate=lambda x:x+[('rom/README.txt',b'old installers reject ROM instructions in ZIPs')])
case('rom-directory-entry',mutate=lambda x:x+[('rom/',b'')])
case('patch-rom-payload',patch=True,mutate=lambda x:x+[('rom/gds-0033.chd',b'must not replace private ROM')])
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
case('restart-failure-cleans-payloads',restart=True,restart_failure=True,good=True)
case('rollback-failure-keeps-recovery',rollback_failure=True)
def reject_overlapping_session(name,session_inside_game):
    folder=base/name
    game=folder/'game' if session_inside_game else folder/'session/stage/game'
    session=game/'session' if session_inside_game else folder/'session'
    game.mkdir(parents=True);session.mkdir(parents=True,exist_ok=True)
    (game/'InitialDUnity.exe').write_bytes(b'game executable')
    sentinels={session/'stage/keep.bin':b'stage data',session/'backup/keep.bin':b'recovery data',session/'game.zip':b'archive data',game/'InitialDUnity.exe':b'game executable'}
    for path,value in sentinels.items():path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(value)
    encoded=str(game).encode('utf-16le')
    plan=session/'install.plan';plan.write_bytes(b'IDUPD002'+struct.pack('<I',len(encoded)//2)+encoded)
    rejected=subprocess.run([str(helper),str(plan),'--test'],capture_output=True,timeout=10)
    assert rejected.returncode!=0 and not (session/'result.json').exists(),name
    for path,value in sentinels.items():assert path.read_bytes()==value,(name,path)
    results.append(dict(test=name,passed=True));print(name+': PASS',flush=True)
reject_overlapping_session('session-inside-game-keeps-payloads',True)
reject_overlapping_session('game-inside-session-keeps-payloads',False)
(proof/'native-windows-report.json').write_text(json.dumps(dict(passed=True,tests=results,fixture=str(base)),indent=2))
