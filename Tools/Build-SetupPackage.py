"""Build the Windows setup/recovery download from verified helper and launcher inputs."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import uuid
import zipfile

ROOT=Path(__file__).resolve().parents[1]
SCRIPTS=('Install Initial D.cmd','Install-InitialD.ps1','Recover Initial D.cmd','Recover-InitialD.ps1')


def sha(data):
    return hashlib.sha256(data).hexdigest()


def read_report(path,minimum,required=()):
    data=json.loads(path.read_text(encoding='utf-8-sig'))
    tests=data.get('tests',[])
    names=[t.get('name',t.get('test')) for t in tests]
    if data.get('passed') is not True or len(tests)<minimum or data.get('count')!=len(tests) or any(t.get('passed') is not True for t in tests):
        raise ValueError('Incomplete or failed verification: '+str(path))
    if None in names or len(set(names))!=len(names) or not set(required)<=set(names):
        raise ValueError('Required verification cases missing: '+str(path))
    return data


def build(helper,archive,installation_report,recovery_report,launcher_report):
    if os.name!='nt':raise ValueError('Build this Windows setup package on Windows')
    inputs={name:(ROOT/'Tools'/name).read_bytes() for name in SCRIPTS}
    inputs['Idas3UpdateInstaller.exe']=helper.read_bytes()
    helper_hash=sha(inputs['Idas3UpdateInstaller.exe'])
    normal=read_report(installation_report,23,('process-handoff-restart','rollback-failure-keeps-recovery','session-inside-game-keeps-payloads'))
    recovery=read_report(recovery_report,21,('interrupted-seed-updater','interrupted-recovery','running-game','missing-last-backup','locked-destination'))
    launcher=read_report(launcher_report,9,('cmd-single','missing-executable','damaged-backup','select-second','cancel-selection'))
    if normal.get('helperSha256')!=helper_hash or recovery.get('helperSha256')!=helper_hash or recovery.get('realInterruptionIncluded') is not True:
        raise ValueError('The helper does not match full installation/recovery evidence')
    for name in ('Idas3UpdateInstaller.exe','Recover Initial D.cmd','Recover-InitialD.ps1'):
        if launcher.get('inputs',{}).get(name)!=sha(inputs[name]):
            raise ValueError('Recovery launcher changed after verification: '+name)
    version=re.search(r"^\$version='([0-9]+\.[0-9]+\.[0-9]+(?:-[A-Za-z0-9.-]+)?)';",inputs['Install-InitialD.ps1'].decode('utf-8-sig'),re.M)
    if not version:raise ValueError('Cannot identify the pinned setup version')
    inputs['README.txt']=(
        'Extract every file from this ZIP before running a command.\r\n\r\n'
        'NEW INSTALLATION: Run Install Initial D.cmd. It downloads and verifies both game packages. '
        'An original GDS-0033 ROM is required and is not included. Existing players normally use the in-game updater.\r\n\r\n'
        'INTERRUPTED UPDATE: Close the game, then run Recover Initial D.cmd under the same Windows account. '
        'It finds saved unfinished updates and restores the previous game files from verified local backups, '
        'even if InitialDUnity.exe cannot launch. If more than one installation is listed, choose the affected one. '
        'Saves, ROMs, replays and custom music are preserved. After success, launch the game normally and retry the update. '
        'If recovery reports an error, it keeps the backups so the operation can be retried.\r\n\r\n'
        'Manual or Linux/Proton installation: extract both the Windows-x64 and Additional-Courses ZIPs into the same folder. '
        'This Windows recovery tool has not been validated on Wine/Proton.\r\n').encode('utf-8')
    archive=archive.absolute();archive.parent.mkdir(parents=True,exist_ok=True)
    report_path=archive.with_suffix('.report.json')
    if archive.exists() or report_path.exists():raise FileExistsError('Choose a new setup output path')
    temporary=archive.with_name('.'+archive.name+'.building-'+uuid.uuid4().hex)
    try:
        with zipfile.ZipFile(temporary,'x',zipfile.ZIP_DEFLATED,compresslevel=9) as output:
            for name,data in inputs.items():output.writestr(name,data)
        with zipfile.ZipFile(temporary) as check:
            if check.testzip() is not None or set(check.namelist())!=set(inputs):raise ValueError('Setup archive verification failed')
            if any(check.read(name)!=data for name,data in inputs.items()):raise ValueError('Setup archive changed an input')
        archive_hash=sha(temporary.read_bytes())
        # Windows rename refuses an existing destination; never replace a release.
        temporary.rename(archive)
        report={'passed':True,'version':version[1],'archive':str(archive),'bytes':archive.stat().st_size,'sha256':archive_hash,
                'files':{name:sha(data) for name,data in inputs.items()},
                'evidence':{str(p):sha(p.read_bytes()) for p in (installation_report,recovery_report,launcher_report)},
                'scope':'Verified setup/recovery ZIP creation. No upload, release publication, network install or power-loss durability claim.'}
        with report_path.open('x',encoding='utf-8') as output:json.dump(report,output,indent=2)
        return report
    finally:
        if temporary.exists():temporary.unlink() # Only this invocation's exact temporary file.


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--helper',type=Path,required=True)
    parser.add_argument('--archive',type=Path,required=True)
    parser.add_argument('--installation-report',type=Path,required=True)
    parser.add_argument('--recovery-report',type=Path,required=True)
    parser.add_argument('--launcher-report',type=Path,required=True)
    args=parser.parse_args()
    print(json.dumps(build(args.helper,args.archive,args.installation_report,args.recovery_report,args.launcher_report),indent=2))
