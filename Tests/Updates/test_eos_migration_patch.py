"""Verify EOS migration patches with the production staging code and old native installer.

The exact obsolete Steam inventory may be omitted, without deleting installed
files. EOS target files must be staged, retained files verified, and saves kept.
All input binaries are inert fixture bytes; no game or online service is run.
"""
from pathlib import Path
from framework_runtime import configure_managed_test_driver
import argparse
import hashlib
import importlib.util
import json
import os
import shutil
import subprocess
import uuid
import zipfile

ROOT=Path(__file__).resolve().parents[2]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--installer',type=Path,default=ROOT/'Native/build-update-helper/Idas3UpdateInstaller.exe')
    parser.add_argument('--report-dir',type=Path,default=ROOT/'Verification/release-47-20261011/eos-migration-patch')
    args=parser.parse_args()
    proof=args.report_dir.resolve();proof.mkdir(parents=True,exist_ok=True)
    base=proof/('fixture-'+uuid.uuid4().hex);base.mkdir()
    (base/'ISOLATED_UPDATE_TEST.txt').write_text('Disposable EOS migration fixture; no actual installation or saves.')
    spec=importlib.util.spec_from_file_location('patch',ROOT/'Tools/Build-UpdatePatch.py')
    pack=importlib.util.module_from_spec(spec);spec.loader.exec_module(pack)
    before={name:('inert retired fixture '+name).encode() for name in pack.RETIRED_STEAM_FILES}
    before.update({'InitialDUnity.exe':b'old executable','UnityPlayer.dll':b'unchanged engine',
        'InitialDUnity_Data/globalgamemanagers':b'old version','InitialDUnity_Data/Managed/Assembly-CSharp.dll':b'old scripts'})
    after={name:value for name,value in before.items() if name not in pack.RETIRED_STEAM_FILES}
    after.update({'InitialDUnity_Data/globalgamemanagers':b'new EOS version',
        'InitialDUnity_Data/Managed/Assembly-CSharp.dll':b'new EOS scripts',
        'InitialDUnity_Data/Managed/com.Epic.OnlineServices.dll':b'EOS managed fixture',
        'InitialDUnity_Data/Plugins/x86_64/EOSSDK-Win64-Shipping.dll':b'EOS native fixture'})
    for name in ('EOS-SDK-license.txt','EOS-native-license.txt','EOS-native-third-party-notices.txt','EOS-plugin-MIT-license.txt'):
        after['InitialDUnity_Data/Plugins/EOS-Notices/'+name]=('notice fixture '+name).encode()
    checks=[]
    def check(value,name):
        assert value,name
        checks.append(name);print('PASS\t'+name,flush=True)
    retired=pack.validate_file_removals(before,after)
    check(set(retired)==pack.RETIRED_STEAM_FILES and len(retired)==11,'Only exact 11 verified obsolete Steam paths are omitted')
    for unexpected in ('InitialDUnity_Data/Managed/Other.dll','InitialDUnity_Data/Plugins/x86_64/steam_api.dll',
                       'InitialDUnity_Data/StreamingAssets/SteamInput/unlisted.vdf','InitialDUnity_Data/StreamingAssets/course.bin'):
        try:pack.validate_file_removals({**before,unexpected:{}},after)
        except AssertionError:check(True,'Unlisted removal rejected: '+unexpected)
        else:raise AssertionError('Unexpected removal accepted: '+unexpected)
    for name,files in [('old',before),('new',after)]:
        with zipfile.ZipFile(base/(name+'.zip'),'w') as archive:
            for path,value in files.items():archive.writestr(path,value)
    package=pack.build(base/'old.zip',base/'new.zip','0.3.95-community-replays.46','0.3.95-community-replays.47',base)
    with zipfile.ZipFile(package['archive']) as archive:
        manifest=json.loads(archive.read('update-patch.json'))
        check(not pack.RETIRED_STEAM_FILES.intersection(archive.namelist()) and
              not pack.RETIRED_STEAM_FILES.intersection(row['path'] for row in manifest['files']),
              'Retired files are absent from patch payload and target inventory')
    compiler=Path(os.environ['SystemRoot'])/'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
    driver=proof/'StageDriver.exe'
    configure_managed_test_driver(driver)
    run=subprocess.run([str(compiler),'/nologo','/out:'+str(driver),'/r:System.IO.Compression.dll','/r:System.IO.Compression.FileSystem.dll',
                        '/r:System.Web.Extensions.dll',str(ROOT/'Assets/Scripts/Idas3UpdatePaths.cs'),str(ROOT/'Assets/Scripts/Idas3UpdateStaging.cs'),str(ROOT/'Tests/Updates/StageDriver.cs')],capture_output=True,text=True)
    (proof/'compile.log').write_text(run.stdout+run.stderr)
    assert run.returncode==0,run.stdout+run.stderr
    game=base/'game';session=base/'session';game.mkdir();session.mkdir()
    originals={**before,'userdata/card.json':b'personal save fixture','rom/gds-0033.chd':b'private ROM filename fixture'}
    for name,value in originals.items():
        path=game/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(value)
    untouched={name:(game/name).stat().st_mtime_ns for name in originals if name not in after or after.get(name)==originals[name]}
    shutil.copyfile(package['archive'],session/'game.zip')
    staged=subprocess.run([str(driver),str(game),str(session),package['sha256'],'patch','0.3.95-community-replays.46','0.3.95-community-replays.47','0','0'],capture_output=True,text=True,timeout=30)
    assert staged.returncode==0,staged.stdout+staged.stderr
    installed=subprocess.run([str(args.installer.resolve()),str(session/'install.plan'),'--test'],capture_output=True,text=True,timeout=30,creationflags=subprocess.CREATE_NO_WINDOW)
    (proof/'install.log').write_text(staged.stdout+staged.stderr+installed.stdout+installed.stderr)
    assert installed.returncode==0,(session/'error.txt').read_text() if (session/'error.txt').exists() else installed.stderr
    check(all((game/name).read_bytes()==value for name,value in after.items()),'Old native updater stages every EOS target file and updated game file')
    check(all((game/name).read_bytes()==before[name] and (game/name).stat().st_mtime_ns==untouched[name] for name in retired),
          'All obsolete installed Steam files retain their bytes and timestamps')
    check(all((game/name).read_bytes()==originals[name] and (game/name).stat().st_mtime_ns==stamp for name,stamp in untouched.items()),
          'Unchanged engine, personal save and ROM fixture remain untouched')
    check(json.loads((session/'result.json').read_text())['passed'],'Native installer records successful terminal result')
    report={'passed':True,'checks':checks,'count':len(checks),'fixture':str(base),'package':package,
            'installer':str(args.installer.resolve()),'installer_sha256':hashlib.sha256(args.installer.read_bytes()).hexdigest(),
            'scope':__doc__.strip()}
    (proof/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS '+str(len(checks))+' EOS migration patch checks; report: '+str(proof/'report.json'))


if __name__=='__main__':main()
