using System;
using System.IO;
using System.Runtime.InteropServices;
using UnityEngine;

namespace Idas3.Multiplayer
{
    public sealed class Idas3OnlineCar
    {
        internal static readonly int[] AppearanceOffsets={16,24,44,48,52,56,60,64,76,152,156,160,164};
        public const int SelectionCount=215;
        public int Selection { get; internal set; }
        public int SaveSlot=>Selection<5?Selection:Selection>=40?(Selection-40)/35:-1;
        public bool Saved { get; internal set; }
        public bool Automatic {get;internal set;}
        internal uint[] Words=new uint[307];
        public int Car=>(int)Words[4];
        public string Label=>Idas3MultiplayerSession.CarNames[Car]+(SaveSlot>=0?" / SAVE "+(SaveSlot+1):Saved?" / LEGACY SAVE":" / STOCK");
        internal bool Same(Idas3OnlineCar other,bool full=false){
            if(other==null||Selection!=other.Selection||Saved!=other.Saved||Automatic!=other.Automatic)return false;
            if(full){for(int i=0;i<Words.Length;++i)if(Words[i]!=other.Words[i])return false;}
            else foreach(int offset in AppearanceOffsets)if(Words[offset/4]!=other.Words[offset/4])return false;
            return true;
        }
        internal void Write(BinaryWriter w){w.Write(Selection);w.Write(Saved);w.Write(Automatic);foreach(int offset in AppearanceOffsets)w.Write(Words[offset/4]);}
        internal static Idas3OnlineCar Read(BinaryReader r,int expected){
            var car=new Idas3OnlineCar{Selection=r.ReadInt32(),Saved=r.ReadBoolean(),Automatic=r.ReadBoolean()};
            foreach(int offset in AppearanceOffsets)car.Words[offset/4]=r.ReadUInt32();
            if(car.Selection<0||car.Selection>=SelectionCount||(car.Selection>=40&&(car.Selection-40)%35!=expected)||car.Car!=expected||expected<0||expected>=35||car.Words[19]>5||car.Words[16]>7)
                throw new InvalidDataException("Invalid saved car selection");
            for(int i=11;i<=15;++i)if(car.Words[i]>220)throw new InvalidDataException("Invalid saved driver glyph");
            for(int offset=156;offset<164;++offset)if(((car.Words[offset/4]>>((offset%4)*8))&255)>6)throw new InvalidDataException("Invalid saved body part");
            if(car.Words[6]>31||(car.Words[41]&255)>=76)throw new InvalidDataException("Invalid saved physics profile");
            if(((car.Words[41]>>8)&255)>3||((car.Words[41]>>16)&255)>3)throw new InvalidDataException("Invalid saved appearance flags");
            return car;
        }
    }
    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    public struct Idas3MultiplayerConfig
    {
        public uint size, version, course, reverse, wet, night, localCar, remoteCar, localSlot, automatic;
    }
    [Serializable, StructLayout(LayoutKind.Sequential, Pack = 8)]
    public struct Idas3CarSnapshot
    {
        public uint size, version;
        public ulong sequence, raceTicks;
        public uint flags, car;
        public Vector3 body, actor;
        public float yaw, pitch, roll, steering;
        public Vector4 suspension, rotation;
        public float speed, rpm, progress;
        public uint headlightPhase;
        public int headlightCounter;
        public uint headlightVisible;
        public bool Finished => (flags & 4) != 0;
        public bool TimeUp => (flags & 8) != 0;

        public bool Valid(uint expectedCar)
        {
            if (size != 128 || version != 1 || car != expectedCar || (flags & 1u) == 0 || (flags & ~127u) != 0 || headlightVisible > 1) return false;
            if (!Finite(body.x) || !Finite(body.y) || !Finite(body.z) || !Finite(actor.x) || !Finite(actor.y) || !Finite(actor.z)) return false;
            if (Mathf.Abs(body.x)>100000 || Mathf.Abs(body.y)>100000 || Mathf.Abs(body.z)>100000 || Mathf.Abs(actor.x)>100000 || Mathf.Abs(actor.y)>100000 || Mathf.Abs(actor.z)>100000) return false;
            if (!Finite(yaw) || !Finite(pitch) || !Finite(roll) || !Finite(steering) || !Finite(speed) || !Finite(rpm) || !Finite(progress)) return false;
            if (Mathf.Abs(speed) > 300 || rpm < 0 || rpm > 30000 || Mathf.Abs(progress) > 1000000) return false;
            for (int i = 0; i < 4; ++i) if (!Finite(suspension[i]) || !Finite(rotation[i]) || Mathf.Abs(suspension[i]) > 1000 || Mathf.Abs(rotation[i]) > 1e9f) return false;
            return true;
        }
        static bool Finite(float f) => !float.IsNaN(f) && !float.IsInfinity(f);
        internal void Write(BinaryWriter w)
        {
            w.Write(size); w.Write(version); w.Write(sequence); w.Write(raceTicks); w.Write(flags); w.Write(car);
            WriteVector(w, body); WriteVector(w, actor); w.Write(yaw); w.Write(pitch); w.Write(roll); w.Write(steering);
            for (int i = 0; i < 4; ++i) w.Write(suspension[i]);
            for (int i = 0; i < 4; ++i) w.Write(rotation[i]);
            w.Write(speed); w.Write(rpm); w.Write(progress); w.Write(headlightPhase); w.Write(headlightCounter); w.Write(headlightVisible);
        }
        internal static Idas3CarSnapshot Read(BinaryReader r)
        {
            var s = new Idas3CarSnapshot { size = r.ReadUInt32(), version = r.ReadUInt32(), sequence = r.ReadUInt64(), raceTicks = r.ReadUInt64(), flags = r.ReadUInt32(), car = r.ReadUInt32() };
            s.body = ReadVector(r); s.actor = ReadVector(r); s.yaw = r.ReadSingle(); s.pitch = r.ReadSingle(); s.roll = r.ReadSingle(); s.steering = r.ReadSingle();
            for (int i = 0; i < 4; ++i) s.suspension[i] = r.ReadSingle();
            for (int i = 0; i < 4; ++i) s.rotation[i] = r.ReadSingle();
            s.speed = r.ReadSingle(); s.rpm = r.ReadSingle(); s.progress = r.ReadSingle();
            s.headlightPhase = r.ReadUInt32(); s.headlightCounter = r.ReadInt32(); s.headlightVisible = r.ReadUInt32(); return s;
        }
        static void WriteVector(BinaryWriter w, Vector3 v) { w.Write(v.x); w.Write(v.y); w.Write(v.z); }
        static Vector3 ReadVector(BinaryReader r) => new Vector3(r.ReadSingle(), r.ReadSingle(), r.ReadSingle());
    }
    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    [Serializable]
    public struct Idas3AuthorityStatus
    {
        public uint size,version;
        public ulong frame,confirmed,verified,contactFrames,rollbacks,replayedFrames;
        public uint maxDepth,stalled;
        public float maxCorrection,maxReplayMs;
        public int winner;
        public uint reserved;
        public ulong hostFinishTicks,clientFinishTicks;
    }
    internal static class Idas3MultiplayerNative
    {
        const string Library = "Idas3Unity";
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3MultiplayerEnableAuthority(ulong race,int remoteAutomatic,int boost);
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3MultiplayerEnableAuthorityRules(ulong race,int remoteAutomatic,int boost,int collisions);
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3MultiplayerAuthorityPacket([Out] byte[] bytes,uint capacity);
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3MultiplayerAuthorityReceive([In] byte[] bytes,uint count);
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3MultiplayerSetRemoteHeadlights(ulong sequence,uint enabled);
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3MultiplayerAuthorityStatus(ref Idas3AuthorityStatus status);
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3MultiplayerGetRemoteSnapshot(ref Idas3CarSnapshot snapshot);

        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3MultiplayerCurrentGear();
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3MultiplayerReadCar(int selection,[Out] uint[] words,uint count);
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3MultiplayerCurrentCar();
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3MultiplayerStartSaved(ref Idas3MultiplayerConfig config,int selection,[In] uint[] local,[In] uint[] remote,uint count);
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3MultiplayerReadRaceCar(int side,[Out] uint[] words,uint count);
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3MultiplayerSetBattleRecords(ref Idas3BattleRecord local,ref Idas3BattleRecord remote);
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3MultiplayerGetBattleRecord(int side,out Idas3BattleRecord record);
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3SceneCopyPreRaceBattleRecord(int side,System.Text.StringBuilder destination,int capacity);
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3MultiplayerStart(ref Idas3MultiplayerConfig config);
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3MultiplayerSetGo(int released);
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3MultiplayerSetResult(int winner);
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3MultiplayerDisconnect();
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3MultiplayerGetLocalSnapshot(ref Idas3CarSnapshot snapshot);
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3MultiplayerApplyRemoteSnapshot(ref Idas3CarSnapshot snapshot);
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3MultiplayerLeave();
    }
}
