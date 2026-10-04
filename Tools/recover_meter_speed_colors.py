"""Bind complete, distinct authored speed-atlas families.

GetSpeedColor in the supplied WBP_SpeedMeter_Base uses <90/<150/<210.
Numbered files alone do not establish a changing palette. Identical legacy
sets retain their original texture/color. No texture files are modified here.
"""
import json,re,hashlib
from pathlib import Path
root=Path(__file__).resolve().parents[1]
path=root/'Assets/Resources/ArcadeHud/Catalog/catalog.json'
data=json.loads(path.read_text(encoding='utf-8'));count=0;meters=[];fixed=[]
for meter in data['meters']:
    changed=False
    for layer in meter['layers']:
        if layer.get('role') not in ('speed1','speed10','speed100') and layer['name']!='SpeedRate':continue
        layer.pop('speedPalette',None)
        layer.pop('speedTextures',None)
        layer.pop('speedRainbow',None)
        # A specified digit brush color (DAC White, Pop Team Epic Stay!)
        # belongs to that design even when its neutral atlas is shared.
        if any(abs(v-1)>1e-6 for v in layer.get('brushColor',[1,1,1])[:3]):
            continue
        texture=layer.get('texture','')
        if not re.search(r'_SpdNum(?:_Oth)?0[1-4]$',texture):continue
        choices=[texture[:-2]+f'{i:02}' for i in range(1,5)]
        if not all((root/'Assets/Resources'/f'{choice}.png').is_file() for choice in choices):continue
        hashes={hashlib.sha256((root/'Assets/Resources'/f'{choice}.png').read_bytes()).digest() for choice in choices}
        if len(hashes)==1:
            if meter['id'] not in fixed:fixed.append(meter['id'])
            continue
        layer['speedTextures']=choices
        # Only these two families supply a neutral fourth atlas for the
        # existing high-speed hue animation. Classic's fourth is pastel yellow.
        layer['speedRainbow']=bool(re.search(r'T_Meter(?:00|30)_SpdNum(?:_Oth)?04$',choices[3]))
        count+=1;changed=True
    if changed:meters.append(meter['id'])
path.write_text(json.dumps(data,ensure_ascii=False,separators=(',',':'))+'\n',encoding='utf-8')
print(json.dumps(dict(layers=count,meters=meters,fixedColorMeters=fixed)))
