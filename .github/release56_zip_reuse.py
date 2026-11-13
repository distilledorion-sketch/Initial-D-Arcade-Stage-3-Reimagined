"""Copy verified ordinary ZIP deflate streams without recompressing them."""
import copy,hashlib,json,struct,zipfile,zlib
from pathlib import Path

def copy_entry(source, destination, name):
    entry=source.getinfo(name)
    assert entry.compress_type in (zipfile.ZIP_STORED,zipfile.ZIP_DEFLATED)
    assert not entry.flag_bits&1 and entry.file_size<2**32 and entry.compress_size<2**32
    stream=source.fp;stream.seek(entry.header_offset);header=stream.read(30)
    assert header[:4]==b'PK\x03\x04'
    names,extras=struct.unpack_from('<HH',header,26)
    stream.seek(entry.header_offset+30+names+extras)
    out=copy.copy(entry);out.extra=b'';out.comment=b'';out.flag_bits=0
    out.header_offset=destination.fp.tell();out.create_system=0;out.external_attr=0x20
    out.extract_version=20;out.create_version=20
    destination._writecheck(out);destination._didModify=True
    destination.fp.write(out.FileHeader(False))
    remaining=entry.compress_size
    while remaining:
        block=stream.read(min(1024*1024,remaining));assert block
        destination.fp.write(block);remaining-=len(block)
    destination.start_dir=destination.fp.tell()
    destination.filelist.append(out);destination.NameToInfo[out.filename]=out

def copy_raw(reference,destination,name,record):
    raw=Path(record['raw'])
    with raw.open('rb') as stream:assert hashlib.file_digest(stream,'sha256').hexdigest()==record['rawSha256']
    out=copy.copy(reference.getinfo(name));out.extra=b'';out.comment=b'';out.flag_bits=0
    out.header_offset=destination.fp.tell();out.create_system=0;out.external_attr=0x20
    out.extract_version=20;out.create_version=20;out.compress_type=zipfile.ZIP_DEFLATED;out.compress_size=record['bytes']
    assert out.file_size==record['originalBytes'] and out.compress_size==raw.stat().st_size
    destination._writecheck(out);destination._didModify=True;destination.fp.write(out.FileHeader(False))
    with raw.open('rb') as stream:
        while block:=stream.read(1024*1024):destination.fp.write(block)
    destination.start_dir=destination.fp.tell();destination.filelist.append(out);destination.NameToInfo[name]=out

def optimized_full(player, files, destination, candidate, baseline, baseline_inventory, proof):
    inputs={}
    for p in files:
        with p.open('rb') as f:digest=hashlib.file_digest(f,'sha256').hexdigest()
        name=p.relative_to(player).as_posix()
        inputs[name]={'path':name,'size':p.stat().st_size,'sha256':digest}
    with zipfile.ZipFile(candidate) as compact,zipfile.ZipFile(candidate.with_name('compact.zip')) as fast,zipfile.ZipFile(baseline) as old:
        best={n:min((compact,fast),key=lambda z:z.getinfo(n).compress_size) for n in inputs}
        optimized_path=candidate.with_name('optimized-deflate-snapshot.json')
        raw={r['path']:r for r in json.loads(optimized_path.read_text()) if 'raw' in r} if optimized_path.exists() else {}
        raw={n:r for n,r in raw.items() if n in inputs and r['sha256']==inputs[n]['sha256'] and r['bytes']<best[n].getinfo(n).compress_size}
        choices={};benefits=[];total=22
        for name,record in inputs.items():
            entry=best[name].getinfo(name)
            with best[name].open(entry) as f:matches=hashlib.file_digest(f,'sha256').hexdigest()==record['sha256']
            if not matches:
                alternate=fast if best[name] is compact else compact
                with alternate.open(name) as f:alternate_matches=hashlib.file_digest(f,'sha256').hexdigest()==record['sha256']
                if alternate_matches:best[name]=alternate;entry=alternate.getinfo(name);matches=True
            optimized_size=raw[name]['bytes'] if name in raw else entry.compress_size
            if not matches:
                choices[name]='fresh';total+=len(zlib.compress((player/name).read_bytes(),6))-6
            elif baseline_inventory.get(name)==record:
                previous=old.getinfo(name);choices[name]='old';total+=previous.compress_size
                gain=previous.compress_size-optimized_size
                if gain>0:benefits.append((gain/max(1,optimized_size),gain,name))
            else:
                choices[name]='compact';total+=optimized_size
            total+=76+2*len(name.encode('utf-8'))
        initial=total;savings=0
        for _,gain,name in sorted(benefits,reverse=True):
            if total<2*1024**3-1024*1024:break
            choices[name]='compact';total-=gain;savings+=gain
        assert total<2*1024**3,('Lossless ZIP still exceeds GitHub size limit',total)
        with zipfile.ZipFile(destination,'x',zipfile.ZIP_DEFLATED,compresslevel=6) as output:
            for name,choice in choices.items():
                if choice=='fresh':output.write(player/name,name)
                elif choice=='compact' and name in raw:copy_raw(best[name],output,name,raw[name])
                else:copy_entry(old if choice=='old' else best[name],output,name)
        result={'estimatedBytes':total,'initialBytes':initial,'additionalLosslessSavings':savings,
                'reusedBaseEntries':sum(c=='old' for c in choices.values()),'optimizedEntries':sum(c=='compact' for c in choices.values()),
                'freshEntries':sum(c=='fresh' for c in choices.values()),'files':inputs}
        proof.write_text(json.dumps(result,separators=(',',':')))
        return inputs

def build_patch(base,target,base_version,target_version,out,inventory):
    dest=Path(out)/f'Initial-D-Update-from-{base_version}-to-{target_version}-Patch.zip'
    assert not dest.exists()
    with zipfile.ZipFile(base) as old,zipfile.ZipFile(target) as new:
        before=inventory(old);after=inventory(new)
        omissions=json.loads((Path(__file__).resolve().parent/'omitted-unused-exports.json').read_text())
        assert before.keys()-after.keys()=={r['path'] for r in omissions['omitted']}
        for r in omissions['omitted']:
            assert r['path'].startswith('InitialDUnity_Data/StreamingAssets/IDAS3/data/original_assets/rival_dialog/banks/') and r['path'].endswith('/original_texture_decoded.bin')
            for n,payload in r['retainedRuntimeFiles'].items():assert after[n]==before[n]==payload
        files=[dict(record,included=before.get(name)!=record) for name,record in after.items()]
        manifest={'schema':1,'baseVersion':base_version,'targetVersion':target_version,'files':files}
        with zipfile.ZipFile(dest,'x',zipfile.ZIP_DEFLATED,compresslevel=6) as output:
            output.writestr('update-patch.json',json.dumps(manifest,separators=(',',':')))
            for f in files:
                if f['included']:copy_entry(new,output,f['path'])
        with zipfile.ZipFile(dest) as patch:
            assert set(patch.namelist())=={'update-patch.json'}|{f['path'] for f in files if f['included']}
            assert json.loads(patch.read('update-patch.json'))==manifest
            for f in files:
                if f['included']:
                    # Reading to EOF verifies each CRC as well as its SHA256.
                    with patch.open(f['path']) as stream:assert hashlib.file_digest(stream,'sha256').hexdigest()==f['sha256']
                else:
                    # The base inventory just hashed this retained file from
                    # the open, digest-verified baseline archive.
                    assert before[f['path']]==after[f['path']]
    with dest.open('rb') as f:digest=hashlib.file_digest(f,'sha256').hexdigest()
    result={'archive':str(dest),'bytes':dest.stat().st_size,'sha256':digest,
            'changedFiles':sum(f['included'] for f in files),'totalFiles':len(files),'reconstructedTargetVerified':True}
    dest.with_suffix('.report.json').write_text(json.dumps(result,indent=2))
    return result
