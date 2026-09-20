from pathlib import Path
import hashlib,json,os,subprocess,time,uuid,zipfile

base=Path(__file__).resolve().parent/('handoff-'+uuid.uuid4().hex)
game=base/'game';game.mkdir(parents=True);session=base/'session';session.mkdir()
(base/'ISOLATED_UPDATE_TEST.txt').write_text('Disposable executable and updater fixture')
shell=os.environ['SystemRoot']+'/System32/WindowsPowerShell/v1.0/powershell.exe'
csc=os.environ['SystemRoot']+'/Microsoft.NET/Framework64/v4.0.30319/csc.exe'
env={k:v for k,v in os.environ.items() if k.lower()!='psmodulepath'}
code='using System;using System.IO;class P{static void Main(string[] a){if(a.Length>0&&a[0]=="-wait"){Console.ReadLine();return;}File.WriteAllText(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"restarted.txt"),"VERSION|"+string.Join(" ",a));}}'
for name,version in [('old','old'),('new','new')]:
    src=base/(name+'.cs');src.write_text(code.replace('VERSION',version))
    result=subprocess.run([csc,'/nologo','/out:'+str(base/(name+'.exe')),str(src)],capture_output=True,text=True)
    assert result.returncode==0,result.stdout+result.stderr
(game/'InitialDUnity.exe').write_bytes((base/'old.exe').read_bytes())
(game/'UnityPlayer.dll').write_bytes(b'old dll')
(game/'userdata').mkdir();(game/'userdata/save.json').write_bytes(b'my save')
required={'InitialDUnity.exe':(base/'new.exe').read_bytes(),'UnityPlayer.dll':b'new dll','InitialDUnity_Data/globalgamemanagers':b'fixture','InitialDUnity_Data/Managed/Assembly-CSharp.dll':b'fixture'}
archive=session/'game.zip'
with zipfile.ZipFile(archive,'w') as z:
    for k,v in required.items():z.writestr(k,v)
source=Path(__file__).resolve().parents[2]/'Assets/Resources/UpdateInstaller.ps1.txt'
(session/'install.ps1').write_bytes(source.read_bytes())
parent=subprocess.Popen([str(game/'InitialDUnity.exe'),'-wait'],stdin=subprocess.PIPE,creationflags=subprocess.CREATE_NO_WINDOW)
ticks=subprocess.check_output([shell,'-NoProfile','-Command',f'(Get-Process -Id {parent.pid}).StartTime.ToUniversalTime().Ticks.ToString()'],env=env,text=True).strip()
(session/'install.json').write_text(json.dumps(dict(installRoot=str(game),archive=str(archive),sha256=hashlib.sha256(archive.read_bytes()).hexdigest(),parentId=parent.pid,parentStartTicks=ticks)))
helper=subprocess.Popen([shell,'-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',str(session/'install.ps1'),'-ManifestPath',str(session/'install.json')],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,creationflags=subprocess.CREATE_NO_WINDOW)
try:
    deadline=time.time()+30
    while not (session/'ready').exists() and time.time()<deadline and helper.poll() is None:time.sleep(.1)
    assert (session/'ready').exists(),'Helper did not prepare'
    assert parent.poll() is None
    assert (game/'InitialDUnity.exe').read_bytes()==(base/'old.exe').read_bytes(),'Replaced the running game'
    assert (game/'UnityPlayer.dll').read_bytes()==b'old dll'
    parent.stdin.write(b'\n');parent.stdin.flush();parent.wait(timeout=10)
    stdout,stderr=helper.communicate(timeout=40)
    assert helper.returncode==0,(stdout,stderr,(session/'error.txt').read_text() if (session/'error.txt').exists() else '')
    deadline=time.time()+10
    while not (game/'restarted.txt').exists() and time.time()<deadline:time.sleep(.1)
    assert (game/'restarted.txt').read_text()=='new|-idas3-skip-update-once'
    assert (game/'userdata/save.json').read_bytes()==b'my save'
    assert not (session/'game.zip').exists() and not (session/'backup').exists() and not (session/'stage').exists()
    report=dict(passed=True,parentWait=True,replacedAfterExit=True,restartedNewExecutable=True,savesUnchanged=True,temporaryFilesCleaned=True,fixture=str(base))
    (Path(__file__).resolve().parent/'handoff-report.json').write_text(json.dumps(report,indent=2))
    print(json.dumps(report),flush=True)
finally:
    if parent.poll() is None:parent.stdin.write(b'\n');parent.stdin.flush();parent.wait(timeout=10)
