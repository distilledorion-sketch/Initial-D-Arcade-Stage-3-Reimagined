"""Build a verified changed-file patch between two complete release ZIPs.
Keep the complete Windows ZIP in every release for old clients and Full Repair.
Publish each output patch as a release asset; its name selects the matching base.
"""
from pathlib import Path
import argparse, hashlib, json, re, zipfile

REQUIRED={'InitialDUnity.exe','UnityPlayer.dll','InitialDUnity_Data/globalgamemanagers','InitialDUnity_Data/Managed/Assembly-CSharp.dll'}
PRIVATE={'userdata','userdata-unity-scene','community-times','replays','custom-music','custom music','admin-access.txt','identity.json','game-options.json','deploy.private.json','library.json','pending.json'}

def inventory(z):
    result={};folded=set()
    for i in z.infolist():
        name=i.filename;parts=name.split('/')
        # Personal music and its generated README must never enter a full ZIP,
        # payload or retained-file manifest, even as an empty directory entry.
        assert not any(p.lower() in {'custom music','custom-music'} for p in parts), 'Personal music must not enter release packages: '+name
        if i.is_dir(): continue
        # Builds contain setup instructions, but update ZIPs must omit the ROM
        # directory for older installers. Never open or hash a user's dump.
        if name=='rom/README.txt': continue
        assert not any(p.lower()=='rom' for p in parts), 'Original ROM files must not enter release packages: '+name
        assert not any(p.lower() in PRIVATE or p in ('','.','..') or p.endswith((' ','.')) or re.search(r'[\\<>:"|?*\x00]',p) or re.match(r'^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(\.|$)',p,re.I) for p in parts),name
        assert parts[0] in {'InitialDUnity_Data','MonoBleedingEdge','D3D12'} if len(parts)>1 else name in {'InitialDUnity.exe','UnityPlayer.dll','UnityCrashHandler64.exe','dstorage.dll','dstoragecore.dll','steam_appid.txt','READ ME.txt','Replay Viewer.cmd','MULTIPLAYER TEST.txt'},name
        assert len(name)<=220 and name.lower() not in folded and not (i.external_attr&0x400) and ((i.external_attr>>16)&0xf000)!=0xa000,name
        folded.add(name.lower())
        with z.open(i) as f: sha=hashlib.file_digest(f,'sha256').hexdigest()
        result[name]={'path':name,'size':i.file_size,'sha256':sha}
    assert REQUIRED<=result.keys()
    return result

def build(base,target,base_version,target_version,out):
    for v in [base_version,target_version]: assert re.fullmatch(r'[0-9A-Za-z.-]{1,96}',v),v
    out=Path(out);out.mkdir(parents=True,exist_ok=True)
    dest=out/f'Initial-D-Update-from-{base_version}-to-{target_version}-Patch.zip'
    assert not dest.exists(),dest
    with zipfile.ZipFile(base) as old,zipfile.ZipFile(target) as new:
        before=inventory(old);after=inventory(new)
        assert not before.keys()-after.keys(),'File removals require a full release until explicit removal manifests are supported.'
        files=[]
        for name,record in after.items():files.append(dict(record,included=before.get(name)!=record))
        manifest=dict(schema=1,baseVersion=base_version,targetVersion=target_version,files=files)
        with zipfile.ZipFile(dest,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as patch:
            patch.writestr('update-patch.json',json.dumps(manifest,separators=(',',':')))
            for f in files:
                if f['included']:
                    with new.open(f['path']) as source,patch.open(f['path'],'w',force_zip64=True) as output:
                        import shutil
                        shutil.copyfileobj(source,output)
        with zipfile.ZipFile(dest) as patch:
            assert patch.testzip() is None
            assert set(patch.namelist())=={'update-patch.json'}|{f['path'] for f in files if f['included']}
            for f in files:
                source=patch if f['included'] else old
                with source.open(f['path']) as stream: assert hashlib.file_digest(stream,'sha256').hexdigest()==f['sha256']
    sha=hashlib.file_digest(dest.open('rb'),'sha256').hexdigest()
    report=dict(archive=str(dest),bytes=dest.stat().st_size,sha256=sha,changedFiles=sum(f['included'] for f in files),totalFiles=len(files),reconstructedTargetVerified=True)
    dest.with_suffix('.report.json').write_text(json.dumps(report,indent=2))
    print(json.dumps(report),flush=True)
    return report

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for key in ['base','target','base-version','target-version','out']:p.add_argument('--'+key,required=True)
    a=p.parse_args();build(a.base,a.target,a.base_version,a.target_version,a.out)
