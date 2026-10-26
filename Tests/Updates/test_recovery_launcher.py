"""Run the user-facing recovery entry points against isolated update caches."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import uuid

ROOT=Path(__file__).resolve().parents[2]
FILES=['InitialDUnity.exe','UnityPlayer.dll','InitialDUnity_Data/globalgamemanagers','InitialDUnity_Data/Managed/Assembly-CSharp.dll']


def string(value):
    data=str(value).encode('utf-16le')
    return struct.pack('<I',len(data)//2)+data


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--helper',type=Path,required=True)
    parser.add_argument('--report-dir',type=Path)
    args=parser.parse_args()
    if sys.flags.optimize:parser.error('Assertions must stay enabled')
    proof=(args.report_dir or ROOT/'Verification'/('recovery-launcher-'+uuid.uuid4().hex)).resolve()
    proof.mkdir(parents=True,exist_ok=False)
    setup=proof/'Setup 日本語 & spaces !';setup.mkdir()
    for name in ('Recover Initial D.cmd','Recover-InitialD.ps1'):
        shutil.copyfile(ROOT/'Tools'/name,setup/name)
    shutil.copyfile(args.helper,setup/'Idas3UpdateInstaller.exe')
    shell=Path(os.environ['SystemRoot'])/'System32/WindowsPowerShell/v1.0/powershell.exe'
    cmd=Path(os.environ['SystemRoot'])/'System32/cmd.exe'
    results=[]

    def fixture(folder,index):
        session=folder/'InitialDUpdates'/uuid.uuid4().hex
        game=folder/('Game 日本語 & spaces ! '+str(index));game.mkdir()
        session.mkdir(parents=True)
        plan=b'IDUPD002'+string(game)+struct.pack('<IQI',0,0,len(FILES))
        old={}
        for name in FILES:
            before=b'old '+name.encode();after=b'new '+name.encode();old[name]=before
            dest=game/name;backup=session/'backup'/name
            dest.parent.mkdir(parents=True,exist_ok=True);backup.parent.mkdir(parents=True,exist_ok=True)
            dest.write_bytes(after);backup.write_bytes(before)
            plan+=string(name)+struct.pack('<BBQ',1,1,len(after))+hashlib.sha256(before).digest()+hashlib.sha256(after).digest()
        old['userdata/card.json']=b'private fixture'
        (game/'userdata').mkdir();(game/'userdata/card.json').write_bytes(old['userdata/card.json'])
        (session/'install.plan').write_bytes(plan);(session/'ready').write_text('Prepared')
        return game,session,old

    def inventory(game):
        return {p.relative_to(game).as_posix():p.read_bytes() for p in game.rglob('*') if p.is_file()}

    def case(name,mode='cmd',number=1,choice='',corrupt=False,missing=False):
        folder=proof/name;folder.mkdir()
        cases=[fixture(folder,i+1) for i in range(number)]
        before=[inventory(g) for g,s,o in cases]
        if corrupt:
            (cases[0][1]/'backup'/FILES[-1]).write_bytes(b'corrupt last backup')
        if missing:
            (cases[0][0]/FILES[0]).unlink();before[0]=inventory(cases[0][0])
        env={k:v for k,v in os.environ.items() if k.lower()!='psmodulepath'}
        env.update(TEMP=str(folder),TMP=str(folder))
        if mode=='cmd':
            command=f'"{cmd}" /d /s /c ""{setup / "Recover Initial D.cmd"}""'
        else:
            command=[str(shell),'-NoProfile','-ExecutionPolicy','Bypass','-File',str(setup/'Recover-InitialD.ps1')]
            if mode=='list':command+=['-ListOnly']
            if mode=='destination':command+=['-Destination',str(cases[-1][0])]
            if mode=='session':command+=['-Session',str(cases[-1][1])]
            if mode=='mismatch':command+=['-Session',str(cases[-1][1]),'-Destination',str(cases[0][0])]
        process=subprocess.run(command,cwd=folder,env=env,input=choice+'\n\n',capture_output=True,text=True,errors='replace',timeout=90,creationflags=subprocess.CREATE_NO_WINDOW)
        (folder/'output.txt').write_text(process.stdout+process.stderr,encoding='utf-8')
        result={'name':name,'passed':False,'exitCode':process.returncode};results.append(result)
        try:
            expected_failure=corrupt or mode=='mismatch'
            assert (process.returncode==0)!=expected_failure,process.stdout+process.stderr
            selected=(number-1 if mode in ('destination','session') else int(choice)-1 if choice else 0)
            no_change=mode in ('list','mismatch') or number>1 and mode=='cmd' and not choice or expected_failure
            for index,(game,session,old) in enumerate(cases):
                assert inventory(game)==(before[index] if no_change or index!=selected else old),'Changed the wrong installation or did not restore it'
                assert (session/'backup').exists(),'Discarded recovery backups'
            if mode=='list':
                data=json.loads(process.stdout)
                assert len(data)==number and {row['GameFolder'] for row in data}=={str(g) for g,s,o in cases}
            if corrupt:
                assert 'backup is missing or damaged' in process.stdout+process.stderr,'Native failure detail did not reach the recovery command'
            result['passed']=True;print('PASS '+name,flush=True)
        except Exception as error:
            result['error']=str(error);print('FAIL '+name+': '+str(error),flush=True)

    case('cmd-single')
    case('missing-executable',missing=True)
    case('list-only',mode='list',number=2)
    case('explicit-destination',mode='destination',number=2)
    case('explicit-session',mode='session',number=2)
    case('mismatched-selection',mode='mismatch',number=2)
    case('select-second',number=2,choice='2')
    case('cancel-selection',number=2)
    case('damaged-backup',corrupt=True)
    summary={'passed':all(r['passed'] for r in results),'count':len(results),'tests':results,
             'inputs':{name:hashlib.sha256((setup/name).read_bytes()).hexdigest() for name in ('Recover Initial D.cmd','Recover-InitialD.ps1','Idas3UpdateInstaller.exe')},
             'scope':'Actual CMD/Windows PowerShell recovery entry points, isolated TEMP caches and native helper. No actual game installation or user profile is touched.'}
    (proof/'report.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8')
    return 0 if summary['passed'] else 1


if __name__=='__main__':
    raise SystemExit(main())
