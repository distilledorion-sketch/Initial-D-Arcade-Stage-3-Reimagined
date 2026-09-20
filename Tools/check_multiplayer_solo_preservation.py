"""Same-input native scene regression against the pre-multiplayer plugin."""
import ctypes as C
import hashlib
import json
from pathlib import Path
import sys
import time
import check_native_multiplayer as mp

out=Path(sys.argv[1]).resolve()
if out.exists():raise RuntimeError('Use a new evidence directory')
out.mkdir(parents=True)
before=mp.ROOT/'Verification/multiplayer-20260909/before/desktop-player/InitialDUnity_Data/Plugins/x86_64/Idas3Unity.dll'
a=mp.Client(out,'before',before);b=mp.Client(out,'after')
started=time.time();samples=[]
try:
    for client in (a,b):client.step(0,False,116) # native F5 TimeAttack
    for tick in range(900):
        for client in (a,b):client.step(1/60,True,68 if 300<=tick%600<325 else None)
        x,y=a.status(),b.status()
        mp.check(bytes(x)[56:76]==bytes(y)[56:76],'exact phase/flags/speed/RPM/event')
        mp.check(x.ticks==y.ticks,'exact simulation counter')
        if tick%120==0:
            fa,fb=a.scene(),b.scene()
            mp.check(fa.vertexCount==fb.vertexCount and fa.rangeCount==fb.rangeCount,'same geometry counts')
            ah=hashlib.sha256(C.string_at(fa.vertices,fa.vertexCount*64)).hexdigest()
            bh=hashlib.sha256(C.string_at(fb.vertices,fb.vertexCount*64)).hexdigest()
            mp.check(ah==bh,'unchanged complete solo car/course vertex bytes')
            mp.check(C.string_at(fa.ranges,fa.rangeCount*64)==C.string_at(fb.ranges,fb.rangeCount*64),'unchanged range/material/ownership bytes')
            samples.append({'tick':tick,'speed':x.speed,'rpm':x.rpm,'vertices':fa.vertexCount,'sha256':ah})
finally:
    a.stop();b.stop()
report={'result':'PASS','checks':mp.checks,'ownerFrames':900,'samples':samples,'seconds':time.time()-started}
(out/'report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
