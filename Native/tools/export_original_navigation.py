"""Recover the cabinet navigation banks and paths without recolouring/resampling."""
from pathlib import Path
import argparse, hashlib, json, struct
import extract_original_models as models
import texture_bank

def export(hostfs, out):
    manifest = {'schema': 'idas3-navigation-v1', 'courses': []}
    for code in ('ez', 'nm', 'hd', 'df', 'vh', 'uh', 'sy', 'tu'):
        name = 'navi_' + code
        source = hostfs / 'model/navi'
        _, chunks, _, _ = models.parse_model(source, name+'_pol.tbl', name+'_tex.tbl')
        dest = out / code
        dest.mkdir(parents=True, exist_ok=True)
        models.write_binary(dest/'navigation.idasmesh', chunks)
        table = (source/(name+'_tex.tbl')).read_bytes()
        raw = models.read_payload(source/(name+'_tex.bin.nz'))
        records = []
        for i in range(len(table)//16):
            w,h,fmt,flags,_,offset,_ = struct.unpack_from('<HHBBHII', table, i*16)
            size = 2048+w*h//4 if flags == 3 else w*h*2
            payload = raw[offset:offset+size]
            rgba = texture_bank.decode_vq(payload,w,h,fmt) if flags == 3 else texture_bank.decode(payload,w,h,fmt,'twiddled')
            records.append(dict(index=i,width=w,height=h,rgba=rgba))
        texture_bank.write_native_pack(dest/'textures.idastex', records)
        path = hostfs/'path'/(name+'_path.bin')
        (dest/'path.bin').write_bytes(path.read_bytes())
        inputs = [source/(name+s) for s in ('_pol.tbl','_pol.bin.nz','_tex.tbl','_tex.bin.nz')] + [path]
        manifest['courses'].append(dict(code=code, inputs={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs},
            outputs={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(dest.iterdir())}))
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')

if __name__ == '__main__':
    p=argparse.ArgumentParser();p.add_argument('--hostfs',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();export(a.hostfs,a.out)
