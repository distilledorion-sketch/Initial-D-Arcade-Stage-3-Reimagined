"""Add source night/fresh headlight selections and original phase programs."""
import argparse,hashlib,json,struct,subprocess
from pathlib import Path
from export_car_presentation import KINDS
from export_original_assembly import IMAGE_HASH
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('root','image','hostfs','capture-exe'):p.add_argument('--'+n,type=Path,required=True)
    a=p.parse_args();image=a.image.read_bytes()
    if hashlib.sha256(image).hexdigest()!=IMAGE_HASH:raise ValueError('Canonical source image')
    phaseTable=struct.unpack_from('<I',image,0xc19145c-0xc020000)[0]
    rows=json.loads((a.root/'data/original_models/car_appearances_manifest.json').read_text());summary=[]
    for item in rows['players']+rows['rivals']:
        car,enemy,folder=item['car'],item['enemy'],item['folder'];modelBase=a.root/'data/original_models'/folder
        base=modelBase/'fresh_player' if enemy<0 else a.root/'data/original_models/rivals'/f'enemy_{enemy:02}'
        output=base/'headlights';output.mkdir(exist_ok=True)
        count=json.loads((modelBase/'model_manifest.json').read_text())['chunk_count'];draws=[];sources=[]
        for state in range(2):
            source=output/('open.json' if state else 'closed.json')
            subprocess.run([str(a.capture_exe),str(a.image),str(a.hostfs/'parts'/f'{folder}.bin'),str(source),str(car),str(count),str(enemy),str(state)],check=True,capture_output=True)
            data=json.loads(source.read_text());selected=[d for d in data['draws'] if d['return_pc'] in (0xc027372,0xc027430)]
            if len(selected)>1:raise ValueError(f'Ambiguous original headlight selection car{car} enemy{enemy} state{state}: {selected}')
            draws.append(selected[0] if selected else None);sources.append(data)
        original=json.loads((base/'source_capture.json').read_text())
        indices=[i for i,d in enumerate(original['draws']) if d['return_pc'] in (0xc027372,0xc027430)]
        if len(indices)>1 or bool(indices)!=bool(draws[0]):raise ValueError('Original closed appearance mismatch')
        if indices and original['draws'][indices[0]]['chunk']!=draws[0]['chunk']:raise ValueError('Original closed chunk mismatch')
        index=indices[0] if indices else 0xffffffff
        phase=struct.unpack_from('<I',image,phaseTable-0xc020000+car*4)[0]
        with (base/'headlights.bin').open('wb') as f:
            f.write(b'ID3POP1\0'+struct.pack('<IIII',1,index,phase,2))
            for d in draws:
                if d is None:
                    f.write(struct.pack('<II',0xffffffff,0));continue
                f.write(struct.pack('<II',d['chunk'],len(d['operations'])))
                for op in d['operations']:
                    dynamic=op['source']==0xc02736a
                    f.write(struct.pack('<II16f',KINDS[op['type']],int(dynamic),*(op['values']+[0.]*(16-len(op['values'])))))
        if sources[0]['popup_visible']!=0 or sources[1]['popup_visible']!=1 or sources[1]['popup_phase']!=0:raise ValueError('Fresh visibility invariant')
        summary.append(dict(car=car,enemy=enemy,closedChunk=draws[0]['chunk'] if draws[0] else None,openChunk=draws[1]['chunk'] if draws[1] else None,instance=indices[0] if indices else None,maximumPhase=phase,frameDuration=40,
            source_hashes=[hashlib.sha256((output/n).read_bytes()).hexdigest() for n in ('closed.json','open.json')]))
    (a.root/'data/original_models/headlights_manifest.json').write_text(json.dumps(dict(source_image_sha256=IMAGE_HASH,source='029040 enables motor from191440/298320.027080..0271BA advances;0272E8..027436 chooses18/19 and parts1D4/1B0 transforms.',appearances=summary),indent=2)+'\n')
    print(f'Exported{len(summary)} original headlight programs; initial-night states fully open.')
if __name__=='__main__':main()
