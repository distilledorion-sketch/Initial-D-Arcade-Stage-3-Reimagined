"""Actual scene-DLL multiplayer regression. Private saves; no GPU/audio device/Steam.
Usage: python Tools/check_native_multiplayer.py <new evidence directory>
"""
import ctypes as C
import hashlib
import json
import math
import shutil
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
U32, U64, F32 = C.c_uint32, C.c_uint64, C.c_float
class Config(C.Structure):
    _fields_ = [(n,U32) for n in 'size version course reverse wet night localCar remoteCar localSlot automatic'.split()]
class Snapshot(C.Structure):
    _fields_ = [('size',U32),('version',U32),('sequence',U64),('raceTicks',U64),('flags',U32),('car',U32),
        ('bodyPosition',F32*3),('actorPosition',F32*3),('yaw',F32),('pitch',F32),('roll',F32),('steering',F32),
        ('suspension',F32*4),('wheelRotation',F32*4),('speed',F32),('rpm',F32),('progress',F32),
        ('headlightPhase',U32),('headlightCounter',C.c_int32),('headlightVisible',U32)]
class Input(C.Structure):
    _fields_=[('size',U32),('flags',U32),('dt',C.c_double),('keys',U32*8),('buttons',U32),
        ('lx',C.c_int32),('ly',C.c_int32),('rx',C.c_int32),('ry',C.c_int32),('lt',U32),('rt',U32),('pad',U32),('width',C.c_int32),('height',C.c_int32)]
class Status(C.Structure):
    _fields_=[('size',U32),('state',U32),('frames',U64),('ticks',U64),('generation',U64),
        ('width',C.c_int32),('height',C.c_int32),('stage',C.c_int32),('child',C.c_int32),('course',C.c_int32),('car',C.c_int32),('racePhase',C.c_int32),
        ('flags',U32),('speed',F32),('rpm',F32),('event',U32),('reserved',U32)]
class Range(C.Structure):
    _fields_=[(n,U32) for n in 'first count texture tsp pcw isp gmp flags gloss lightScope viewMask sourceIndex'.split()]+[('direction',F32*4)]
class Camera(C.Structure):
    _fields_=[('data',F32*17),('flags',U32)]
class Frame(C.Structure):
    _fields_=[('size',U32),('version',U32),('frameGeneration',U64),('textureGeneration',U64)]+[(n,U32) for n in 'width height vertexCount rangeCount textureCount overlayCount viewCount screenFadeArgb'.split()]+[
        ('vertices',C.c_void_p),('ranges',C.POINTER(Range)),('textures',C.c_void_p),('overlays',C.c_void_p),('constants',C.c_void_p),('lights',C.c_void_p),('fog',C.c_void_p),('cameras',Camera*2)]

checks=0
def check(ok,text):
    global checks
    checks+=1
    if not ok: raise AssertionError(text)
def hashes(path):
    return {str(p.relative_to(path)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(path.rglob('*')) if p.is_file()}
class Client:
    def __init__(self,directory,tag,source=None):
        self.root=directory/tag;self.root.mkdir()
        self.saves=self.root/'saves';self.saves.mkdir()
        dll=self.root/(tag+'.dll');shutil.copy2(source or ROOT/'Assets/Plugins/x86_64/Idas3Unity.dll',dll)
        self.dll=C.CDLL(str(dll));self.error=C.create_string_buffer(2048)
        self.dll.Idas3SceneInitialize.argtypes=[C.c_char_p,C.c_char_p,C.c_int,C.c_int]
        check(self.dll.Idas3SceneInitialize(str(ROOT/'Native').encode(),str(self.saves).encode(),1280,720)==1,self.reason())
        self.before=hashes(self.saves)
    def reason(self):
        self.dll.Idas3UnityCopyError(self.error,2048)
        return self.error.value.decode('utf8','replace')
    def status(self):
        s=Status(C.sizeof(Status));check(self.dll.Idas3UnityGetStatus(C.byref(s))==1,'status');return s
    def snapshot(self):
        s=Snapshot(C.sizeof(Snapshot));check(self.dll.Idas3MultiplayerGetLocalSnapshot(C.byref(s))==1,self.reason());return s
    def scene(self):
        f=Frame(C.sizeof(Frame));check(self.dll.Idas3SceneGetFrame(C.byref(f))==1,'frame');return f
    def step(self,dt=1/60,throttle=True,key=None):
        i=Input(C.sizeof(Input),1,dt)
        if throttle:i.keys[87//32]|=1<<(87%32)
        if key is not None:i.keys[key//32]|=1<<(key%32)
        check(self.dll.Idas3SceneStep(C.byref(i))==1,self.reason())
    def apply(self,s):check(self.dll.Idas3MultiplayerApplyRemoteSnapshot(C.byref(s))==1,self.reason())
    def go(self):check(self.dll.Idas3MultiplayerSetGo(1)==1,self.reason())
    def start(self,car,other,slot):
        config=Config(C.sizeof(Config),1,3,0,0,1,car,other,slot,1)
        check(self.dll.Idas3MultiplayerStart(C.byref(config))==1,self.reason())
    def leave(self):check(self.dll.Idas3MultiplayerLeave()==1,self.reason())
    def stop(self):check(self.dll.Idas3SceneShutdown()==1,self.reason())

def main():
    destination=Path(sys.argv[1]).resolve()
    if destination.exists():raise RuntimeError('Evidence directory must be new')
    destination.mkdir(parents=True);started=time.time()
    check([C.sizeof(t) for t in (Config,Snapshot,Input,Status,Frame,Range)]==[40,128,88,80,256,64],'ABI sizes')
    a=Client(destination,'host');b=Client(destination,'join');clients=[a,b]
    report={}
    try:
        a.start(0,8,0);b.start(8,0,1)
        for client in clients:
            check(hashes(client.saves)==client.before,'Start did not write profile/records/settings')
            check(client.status().flags&128,'multiplayer status flag')
        sa,sb=a.snapshot(),b.snapshot()
        report['gridSeparation']=math.dist(sa.actorPosition,sb.actorPosition)
        check(report['gridSeparation']>.5,'separate authored grid positions')
        for client,peer in ((a,sb),(b,sa)):
            client.apply(peer)
            client.step(0)
            f=client.scene();peer_ranges=[f.ranges[i] for i in range(f.rangeCount) if f.ranges[i].lightScope==3]
            check(len(peer_ranges)>5,'actual peer car/plate ranges present')
            check(all(r.flags&32 for r in peer_ranges),'ordinary peer faces preserve fix')
            check(f.viewCount==2,'multiplayer bumper mirror')
            report[client.root.name+'PeerRanges']=len(peer_ranges)
        before=[(bytes(c.snapshot())[16:],c.status().ticks) for c in clients]
        for _ in range(30):
            for c in clients:c.step()
        for c,(snap,ticks) in zip(clients,before):
            check(bytes(c.snapshot())[16:]==snap,'held complete pose/race/light snapshot frozen')
            check(c.status().ticks==ticks,'held bridge solver count frozen')
        # Same sequence is legal: the Unity interpolation callback sends it
        # repeatedly until the next network packet, with an updated pose.
        peer=b.snapshot();a.apply(peer);peer.bodyPosition[0]+=.25;a.apply(peer)
        for corrupt in ('car','nan','world','flags','size'):
            bad=Snapshot.from_buffer_copy(bytes(peer))
            if corrupt=='car':bad.car=34
            elif corrupt=='nan':bad.suspension[2]=float('nan')
            elif corrupt=='world':bad.actorPosition[1]=100001
            elif corrupt=='flags':bad.flags|=0x80000000
            else:bad.size=124
            check(a.dll.Idas3MultiplayerApplyRemoteSnapshot(C.byref(bad))==0,'reject '+corrupt)
            check(a.status().state==1,'malformed remote does not destroy game')
        a.go();b.go()
        for _ in range(179):
            for c in clients:c.step()
        check(a.snapshot().raceTicks==0 and b.snapshot().raceTicks==0,'no early GO')
        for c in clients:c.step()
        check(a.snapshot().raceTicks==1 and b.snapshot().raceTicks==1,'source GO exactly180')
        for n in range(600):
            for c in clients:c.step()
            if n%2==0:
                a.apply(b.snapshot());b.apply(a.snapshot())
        for c in clients:
            check(c.snapshot().raceTicks>=600,'race advances')
            check(hashes(c.saves)==c.before,'racing did not save')
        report['after600']=[{'car':c.snapshot().car,'ticks':c.snapshot().raceTicks,'speed':c.snapshot().speed} for c in clients]
        # Run the actual timer to a natural outcome. The remote test pose never
        # writes source AI state, and the local timer owns finish/timeout.
        for n in range(1800):
            if a.snapshot().flags&4:break
            a.step(.25,False)
        finished=a.snapshot();check(finished.flags&4,'natural native finish/timeout')
        check(hashes(a.saves)==a.before,'finish did not award/save/record')
        report['finish']={'ticks':finished.raceTicks,'timeUp':bool(finished.flags&8),'progress':finished.progress}
        for c in clients:
            c.leave();check(c.status().flags&1,'Leave returns to safe menu')
            check(not(c.status().flags&128),'Leave clears multiplayer')
            check(hashes(c.saves)==c.before,'Leave restores without writes')
        report['checks']=checks;report['seconds']=time.time()-started;report['result']='PASS'
    finally:
        for c in clients:c.stop()
    report['checks']=checks
    (destination/'report.json').write_text(json.dumps(report,indent=2))
    print(json.dumps(report,indent=2))
if __name__=='__main__':main()
