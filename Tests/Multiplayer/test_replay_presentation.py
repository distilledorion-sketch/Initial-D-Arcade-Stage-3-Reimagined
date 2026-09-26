"""Native replay presentation regressions; isolated saves, no network or GPU."""
import ctypes as C
import importlib.util
import json
import math
import re
import struct
import sys
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('scene',ROOT/'Tools/check_native_multiplayer.py')
scene=importlib.util.module_from_spec(spec);spec.loader.exec_module(scene)
class Hud(C.Structure):
    _fields_=[(n,C.c_uint32) for n in ('size','version','flags')]+[('gear',C.c_int32)]+[(n,C.c_float) for n in ('speed','rpm','limit','throttle','brake','drift')]

def main():
    output=Path(sys.argv[1]).resolve();output.mkdir(parents=True,exist_ok=False)
    saves=output/'replay-viewer-session';saves.mkdir()
    dll=C.CDLL(str(ROOT/'Assets/Plugins/x86_64/Idas3Unity.dll'))
    dll.Idas3SceneInitialize.argtypes=[C.c_char_p,C.c_char_p,C.c_int,C.c_int]
    dll.Idas3ReplayPose.argtypes=[C.c_double]+[C.c_float]*5+[C.c_int,C.c_float,C.c_int,C.c_float,C.c_int,C.c_int]
    error=C.create_string_buffer(2048);checks=0
    def check(ok,why):
        nonlocal checks
        checks+=1
        dll.Idas3UnityCopyError(error,len(error))
        assert ok,(why,error.value.decode('utf8','replace'))
    check(dll.Idas3SceneInitialize(str(ROOT/'Native').encode(),str(saves).encode(),1280,720)==1,'initialize')
    before=scene.hashes(saves)
    words=re.findall(r'0x([0-9A-F]+)u',(ROOT/'Native/src/original_start_grid_data.inc').read_text())
    poses=[struct.unpack('<f',struct.pack('<I',int(w,16)))[0] for w in words]
    report=[]
    try:
        for condition in (6,7):
            check(dll.Idas3ReplayStart(condition,0,1,0,1)==1,'start AE86 replay')
            stock=Hud(C.sizeof(Hud));check(dll.Idas3SceneGetHudTelemetry(C.byref(stock))==1,'stock telemetry')
            appearance=(C.c_uint32*15)();appearance[11]=5  # profile+164, A-route engine upgrade
            check(dll.Idas3ReplayAppearance(appearance,15)==1,'recorded tuned appearance')
            tuned=Hud(C.sizeof(Hud));check(dll.Idas3SceneGetHudTelemetry(C.byref(tuned))==1,'tuned telemetry')
            check(tuned.limit>10000 and tuned.limit>stock.limit,'replay restored tuned engine RPM range')
            pos=poses[(condition*2+1)*3:(condition*2+2)*3]
            direction=poses[108+condition*6:111+condition*6]
            yaw=math.atan2(direction[0],direction[2])
            def draw(on,tick=0,opponent=False):
                detail=bytearray(132)
                struct.pack_into('<7f',detail,0,10500,*pos,0,0,0)
                struct.pack_into('<I',detail,96,4)
                struct.pack_into('<I',detail,120,int(on))
                data=(C.c_ubyte*132).from_buffer_copy(detail)
                check(dll.Idas3ReplayDetailFrame(data,132)==1,'detail')
                if opponent:
                    sample=struct.pack('<I5fi',int(tick),*pos,yaw,40,4)+detail
                    other=(C.c_ubyte*160).from_buffer_copy(sample)
                    check(dll.Idas3ReplayOpponentFrame(other,160)==1,'opponent detail')
                check(dll.Idas3ReplayPose(tick,*pos,yaw,40,4,0,0,0,1280,720)==1,'pose')
                frame=scene.Frame(C.sizeof(scene.Frame));check(dll.Idas3SceneGetFrame(C.byref(frame))==1,'frame')
                hud=Hud(C.sizeof(Hud));check(dll.Idas3SceneGetHudTelemetry(C.byref(hud))==1,'HUD')
                check(hud.rpm>10000,'replay RPM was clamped to the stock tach scale')
                return frame.vertexCount
            dark=draw(False);lit=draw(True,60);again=draw(False,0)
            check(lit>dark and dark==again,'road headlight geometry follows replay toggle and backward seek')
            check(dll.Idas3ReplayOpponentStart(0,-1,appearance,15)==1,'opponent')
            two_dark=draw(False,0,True);two_lit=draw(True,60,True)
            check(two_lit-two_dark==2*(lit-dark),'both replay cars project independently')
            report.append(dict(condition=condition,stockLimit=stock.limit,tunedLimit=tuned.limit,headlightVertices=lit-dark))
        check(scene.hashes(saves)==before,'replay wrote persistent save data')
    finally:
        dll.Idas3SceneShutdown()
    result=dict(passed=True,checks=checks,cases=report)
    (output/'report.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))

if __name__=='__main__':main()
