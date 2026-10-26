"""Reconstruct supported update patches and upload only to the prepared .53 draft."""
from pathlib import Path
import contextlib, hashlib, importlib.util, json, os, re, subprocess, zipfile
PLAN=json.loads(Path('.github/release53_plan.json').read_text())
REPO=PLAN['repository'];VERSION=PLAN['version'];TAG='v'+VERSION
assert VERSION=='0.3.95-community-replays.53'
SHARD=int(os.environ['PATCH_SHARD']);assert SHARD in range(4)
WORK=Path('patch-work').resolve();WORK.mkdir(exist_ok=False)
RESULT=Path('results');RESULT.mkdir(exist_ok=True)
def gh(*args):return subprocess.check_output(['gh',*args],text=True,encoding='utf-8').strip()
def api(path):return json.loads(gh('api',f'repos/{REPO}/'+path))
def sha(path):
    with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def load(name,path):
    spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
policy=load('patch_policy','Tools/Build-UpdatePatch.py')
reuse=load('zip_reuse','.github/release53_zip_reuse.py')
def download(release,name,expected=None):
    matches=[a for a in release['assets'] if a['name']==name];assert len(matches)==1
    a=matches[0];assert a['state']=='uploaded' and a['digest'].startswith('sha256:')
    if expected:assert a['digest']=='sha256:'+expected
    path=WORK/name
    subprocess.run(['gh','release','download',release['tag_name'],'--repo',REPO,'--pattern',name,'--dir',str(WORK)],check=True)
    assert path.stat().st_size==a['size'] and 'sha256:'+sha(path)==a['digest']
    return path
def target_release():
    r=api('releases/'+str(PLAN['releaseId']))
    assert r['id']==PLAN['releaseId'] and r['tag_name']==TAG and r['draft'] is True and r['prerelease'] is False
    assert r['target_commitish']==PLAN['sourceCommit']
    return r
class Combined:
    def __init__(self,zips):
        self.zips=zips;self.byname={};self.byid={}
        for z in zips:
            for i in z.infolist():
                assert i.filename not in self.byname
                self.byname[i.filename]=z;self.byid[id(i)]=z
    def infolist(self):return [i for z in self.zips for i in z.infolist()]
    def open(self,key):return (self.byname[key] if isinstance(key,str) else self.byid[id(key)]).open(key)
base=api('releases/tags/v0.3.95-community-replays.52')
assert base['id']==PLAN['baselineReleaseId'] and not base['draft'] and not base['prerelease']
target=target_release()
before_paths=[download(base,r['name'],r['sha256']) for r in PLAN['baselineDownloads']]
after_paths=[download(target,r['name'],r['sha256']) for r in PLAN['targetDownloads']]
with contextlib.ExitStack() as stack:
    old=Combined([stack.enter_context(zipfile.ZipFile(p)) for p in before_paths])
    new=Combined([stack.enter_context(zipfile.ZipFile(p)) for p in after_paths])
    before=policy.inventory(old);after=policy.inventory(new)
    inv={n:dict(bytes=r['size'],sha256=r['sha256']) for n,r in after.items()}
    assert hashlib.sha256(json.dumps(inv,sort_keys=True,separators=(',',':')).encode()).hexdigest()==PLAN['candidateInventorySha256']
    retired=policy.validate_file_removals(before,after)
    supported={int(m[1]) for a in base['assets'] if (m:=re.fullmatch(r'Initial-D-Update-from-0\.3\.95-community-replays\.(\d+)-to-0\.3\.95-community-replays\.52-Patch\.zip',a['name']))}
    assert supported==set(range(20,52))
    reports=[]
    for num in sorted((n for n in supported if n%4==SHARD),reverse=True):
        previous=download(base,f'Initial-D-Update-from-0.3.95-community-replays.{num}-to-0.3.95-community-replays.52-Patch.zip')
        old_hash=sha(previous)
        with zipfile.ZipFile(previous) as z:old_manifest=json.loads(z.read('update-patch.json'))
        assert old_manifest['baseVersion']==f'0.3.95-community-replays.{num}' and old_manifest['targetVersion']=='0.3.95-community-replays.52'
        records={r['path']:r for r in old_manifest['files']}
        assert len(records)==len(old_manifest['files']) and {n:{k:r[k] for k in ('path','size','sha256')} for n,r in records.items()}==before
        assert previous.parent==WORK;previous.unlink()
        rows=[dict(r,included=before.get(n)!=r or records.get(n,{}).get('included',True)) for n,r in after.items()]
        manifest=dict(schema=1,baseVersion=f'0.3.95-community-replays.{num}',targetVersion=VERSION,files=rows)
        path=WORK/f'Initial-D-Update-from-{manifest["baseVersion"]}-to-{VERSION}-Patch.zip'
        with zipfile.ZipFile(path,'x',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
            z.writestr('update-patch.json',json.dumps(manifest,separators=(',',':')))
            for r in rows:
                if r['included']:reuse.copy_entry(new.byname[r['path']],z,r['path'])
        with zipfile.ZipFile(path) as z:
            assert set(z.namelist())=={'update-patch.json'}|{r['path'] for r in rows if r['included']}
            assert json.loads(z.read('update-patch.json'))==manifest
            for r in rows:
                if r['included']:
                    with z.open(r['path']) as f:assert hashlib.file_digest(f,'sha256').hexdigest()==r['sha256']
                else:assert before[r['path']]==after[r['path']] and records[r['path']]['included'] is False
        size=path.stat().st_size;digest=sha(path)
        assert 0<size<min(2**31,PLAN['targetDownloads'][0]['bytes'])
        matches=[a for a in target_release()['assets'] if a['name']==path.name]
        if not matches:subprocess.run(['gh','release','upload',TAG,str(path),'--repo',REPO],check=True)
        uploaded=[a for a in target_release()['assets'] if a['name']==path.name]
        assert len(uploaded)==1 and uploaded[0]['state']=='uploaded' and uploaded[0]['size']==size and uploaded[0]['digest']=='sha256:'+digest
        reports.append(dict(name=path.name,sha256=digest,bytes=size,baseVersion=manifest['baseVersion'],targetVersion=VERSION,
            changedFiles=sum(r['included'] for r in rows),totalFiles=len(rows),passed=True,uploaded=True,
            targetInventorySha256=PLAN['candidateInventorySha256'],basePatchSha256=old_hash,retiredSteamFilesOmitted=retired,
            scope='Every payload hash verified; retained-file proof uses the hash-verified published .52 patch chain. Not gameplay acceptance.'))
        (RESULT/f'patches-{SHARD}.json').write_text(json.dumps(reports,indent=2)+'\n')
        print(f'VERIFIED AND UPLOADED .{num} -> .53: {size:,} bytes',flush=True)
        assert path.parent==WORK;path.unlink()
assert len(reports)==8
