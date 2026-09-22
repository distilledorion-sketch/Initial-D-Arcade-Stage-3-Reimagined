using System;
using System.IO;
using System.Text;
using UnityEngine;

// A replay package is length-prefixed JSON metadata followed by recorded poses.
// It contains no executable code, asset paths or account credentials.
public sealed class Idas3ReplayData
{
    [Serializable] public sealed class Details
    {
        public string id, build;
        public int condition, weather, night, car, manual, ticks6000;
        public int mode,outcome,opponentBytes,opponentCar=-1,opponentEnemy=-1,opponentManual,opponentTelemetry;
        public string playerName,opponentName;
        public int[] nameGlyphs, splits;
    }
    public struct Pose { public uint tick; public Vector3 position; public float yaw, speed; public int gear; public uint[] state; }
    public bool Detailed { get; private set; }
    public uint[] Appearance { get; private set; }
    public Idas3ReplayData Opponent { get; private set; }
    public Details Metadata { get; private set; }
    public Pose[] Frames { get; private set; }
    public double Duration => Metadata.ticks6000 / 6000.0;
    public static readonly string[] Courses = Idas3CourseCatalog.Names;
    static bool Finite(float x) => !float.IsNaN(x) && !float.IsInfinity(x);
    public static Idas3ReplayData Load(string path)
    {
        var size = new FileInfo(path).Length;
        if (size < 80 || size > 2L*Idas3ReplayCodec.MaxStored + 8196) throw new InvalidDataException("Invalid replay package size.");
        return Parse(File.ReadAllBytes(path));
    }
    public static Idas3ReplayData Parse(byte[] data) => ParseInternal(data,false);
    static Idas3ReplayData ParseInternal(byte[] data,bool opponentTrack)
    {
        if (data == null || data.Length < 80 || data.Length > 2L*Idas3ReplayCodec.MaxStored + 8196) throw new InvalidDataException("Invalid replay package size.");
        using var stream = new MemoryStream(data, false);
        using var reader = new BinaryReader(stream, Encoding.UTF8);
        int jsonLength = reader.ReadInt32();
        if (jsonLength < 2 || jsonLength > 8192 || jsonLength + 20 > data.Length) throw new InvalidDataException("Invalid replay header.");
        var metadata = JsonUtility.FromJson<Details>(new UTF8Encoding(false, true).GetString(reader.ReadBytes(jsonLength)));
        if (metadata == null || metadata.condition < 0 || metadata.condition >= Idas3CourseCatalog.ConditionCount || metadata.car < 0 || metadata.car > 34 ||
            metadata.weather < 0 || metadata.weather > 1 || metadata.night < 0 || metadata.night > 1 || metadata.manual < 0 || metadata.manual > 1 ||
            metadata.ticks6000 < 100 || metadata.ticks6000 >= 10800000 || metadata.mode<0||metadata.mode>2||metadata.outcome<0||metadata.outcome>2)
            throw new InvalidDataException("Invalid replay course, car or finish time.");
        if(metadata.opponentBytes<0||metadata.opponentBytes>Idas3ReplayCodec.MaxStored||metadata.opponentBytes>stream.Length-stream.Position-16||
            (metadata.opponentBytes>0&&(metadata.mode==0||metadata.opponentCar<0||metadata.opponentCar>34||metadata.opponentEnemy< -1||metadata.opponentEnemy>99||metadata.opponentManual<0||metadata.opponentManual>1))||
            (!opponentTrack&&metadata.mode>0&&metadata.opponentBytes==0))throw new InvalidDataException("Invalid battle replay tracks.");
        foreach(var name in new[]{metadata.playerName,metadata.opponentName})
            if(name!=null&&(Encoding.UTF8.GetByteCount(name)>128||Array.Exists(name.ToCharArray(),char.IsControl)))throw new InvalidDataException("Invalid replay driver name.");
        if (metadata.splits != null)
        {
            if (metadata.splits.Length > 4) throw new InvalidDataException("Too many replay checkpoints.");
            // The service uses four slots; shorter courses have trailing zeros.
            int used = metadata.splits.Length;
            while (used > 0 && metadata.splits[used - 1] == 0) used--;
            if (used != metadata.splits.Length) Array.Resize(ref metadata.splits, used);
            for (int i = 0; i < metadata.splits.Length; i++)
                if (metadata.splits[i] <= 0 || metadata.splits[i] > metadata.ticks6000 || (i > 0 && metadata.splits[i] <= metadata.splits[i - 1])) throw new InvalidDataException("Invalid replay checkpoints.");
        }
        if (metadata.nameGlyphs != null)
        {
            if (metadata.nameGlyphs.Length > 5) throw new InvalidDataException("Invalid replay driver name.");
            foreach (int glyph in metadata.nameGlyphs) if (glyph < 0 || glyph > 221) throw new InvalidDataException("Invalid replay driver name.");
        }
        var raw = Idas3ReplayCodec.Decode(reader.ReadBytes((int)(stream.Length - stream.Position)-metadata.opponentBytes));
        var opponentBytes=reader.ReadBytes(metadata.opponentBytes);
        using var binaryStream = new MemoryStream(raw, false); using var binary = new BinaryReader(binaryStream);
        uint magic = binary.ReadUInt32(); bool detailed = magic == 0x32524449;
        if ((!detailed && magic != 0x31524449) || binary.ReadUInt32() != metadata.ticks6000) throw new InvalidDataException("Replay does not match its result.");
        uint count = binary.ReadUInt32();
        if (binary.ReadUInt32() != 60 || count < 2 || count > (detailed ? 108001 : 36001)) throw new InvalidDataException("Unsupported replay timeline.");
        uint[] appearance = null;
        if (detailed)
        {
            if (binary.ReadUInt32() != 96 || binary.ReadUInt32() != 160 || binary.ReadUInt32() != 1 || binary.ReadUInt32() != 12) throw new InvalidDataException("Unsupported detailed replay.");
            appearance = new uint[15]; for (int i = 0; i < 15; i++) appearance[i] = binary.ReadUInt32();
            if (binary.ReadUInt32() != 0 || appearance[0] > 1 || appearance[1] > 15 || appearance[7] > 5) throw new InvalidDataException("Invalid replay appearance.");
            for (int i = 2; i < 7; i++) if (appearance[i] > 221) throw new InvalidDataException("Invalid recorded name.");
        }
        if (binaryStream.Length - binaryStream.Position != count * (detailed ? 160L : 28L))
            throw new InvalidDataException("Unsupported or incomplete replay.");
        var frames = new Pose[count];
        for (int i = 0; i < count; i++)
        {
            var p = new Pose { tick = binary.ReadUInt32(), position = new Vector3(binary.ReadSingle(), binary.ReadSingle(), binary.ReadSingle()), yaw = binary.ReadSingle(), speed = binary.ReadSingle(), gear = binary.ReadInt32() };
            if (p.tick > 108000 || (i == 0 ? p.tick > 1 : p.tick <= frames[i - 1].tick || p.tick - frames[i - 1].tick > (detailed ? 1 : 3)) ||
                !Finite(p.position.x) || !Finite(p.position.y) || !Finite(p.position.z) || !Finite(p.yaw) || !Finite(p.speed) ||
                Mathf.Abs(p.position.x) > 1000000 || Mathf.Abs(p.position.y) > 1000000 || Mathf.Abs(p.position.z) > 1000000 || p.gear < 0 || p.gear > 6)
                throw new InvalidDataException("Replay contains an invalid or missing frame.");
            if (detailed)
            {
                p.state = new uint[33]; for (int n = 0; n < 33; n++) p.state[n] = binary.ReadUInt32();
                for (int n = 0; n < 17; n++) if (!Finite(Scalar(p.state[n]))) throw new InvalidDataException("Invalid recorded vehicle state.");
                // The original finish gate settles the final clock up to two
                // ticks behind the last running HUD sample. Preserve both.
                if (!Finite(Scalar(p.state[29])) || !Finite(Scalar(p.state[31])) || p.state[17] >= 10800000 || (metadata.mode==0&&p.state[17]>metadata.ticks6000+200) || p.state[23] > 4 || p.state[24] < 1 || p.state[24] > 4 || p.state[28] > 1 || p.state[30] > 1) throw new InvalidDataException("Invalid recorded HUD or lights.");
                if (metadata.mode==0&&i > 0 && p.state[17] < frames[i - 1].state[17] && (i != count - 1 || frames[i - 1].state[17] - p.state[17] > 200)) throw new InvalidDataException("Invalid recorded clock sequence.");
            }
            frames[i] = p;
        }
        if (Math.Abs((long)frames[frames.Length - 1].tick * 100 - metadata.ticks6000) > 200) throw new InvalidDataException("Replay ends before the finish.");
        if (detailed && metadata.mode==0&&metadata.outcome==0&&frames[^1].state[17] != metadata.ticks6000) throw new InvalidDataException("Recorded finish clock differs from the result.");
        var result=new Idas3ReplayData { Metadata = metadata, Frames = frames, Detailed = detailed, Appearance = appearance };
        if(opponentBytes.Length>0){
            var other=JsonUtility.FromJson<Details>(JsonUtility.ToJson(metadata));other.car=metadata.opponentCar;other.manual=metadata.opponentManual;other.opponentBytes=0;other.splits=null;other.nameGlyphs=null;
            result.Opponent=ParseInternal(Idas3ReplayLibrary.Package(other,opponentBytes,Array.Empty<byte>()),true);
            if(!detailed||!result.Opponent.Detailed||result.Opponent.Frames.Length!=frames.Length||result.Opponent.Frames[0].tick!=frames[0].tick||result.Opponent.Frames[^1].tick!=frames[^1].tick)throw new InvalidDataException("Battle replay timelines do not match.");
        }
        return result;
    }
    public static float Scalar(uint bits) => BitConverter.Int32BitsToSingle(unchecked((int)bits));
    public Pose Sample(double seconds)
    {
        double tick = Detailed ? Math.Clamp(seconds * 60, Frames[0].tick, Frames[^1].tick) : Math.Max(0, Math.Min(Duration, seconds)) / Duration * Frames[Frames.Length - 1].tick;
        if (seconds >= Duration) tick = Frames[^1].tick;
        int lo = 0, hi = Frames.Length - 1;
        while (lo < hi) { int mid = (lo + hi) / 2; if (Frames[mid].tick < tick) lo = mid + 1; else hi = mid; }
        if (lo == 0) return Frames[0];
        var a = Frames[lo - 1]; var b = Frames[lo]; float t = (float)((tick - a.tick) / (b.tick - a.tick));
        uint[] state = null;
        if (Detailed)
        {
            if (t >= 1) return b;
            state = (uint[])a.state.Clone();
            // Match ordinary render interpolation for body and wheel poses.
            // HUD RPM/speed/gear/clocks remain the actual simulation sample.
            for (int i = 1; i <= 14; i++)
            {
                float x = Scalar(a.state[i]), y = Scalar(b.state[i]);
                float v = i >= 4 && i <= 6 || i >= 11 ? x + Mathf.DeltaAngle(x * Mathf.Rad2Deg, y * Mathf.Rad2Deg) * Mathf.Deg2Rad * t : Mathf.Lerp(x, y, t);
                state[i] = unchecked((uint)BitConverter.SingleToInt32Bits(v));
            }
        }
        return new Pose { tick = (uint)tick, position = Vector3.Lerp(a.position, b.position, t), yaw = a.yaw + Mathf.DeltaAngle(a.yaw * Mathf.Rad2Deg, b.yaw * Mathf.Rad2Deg) * Mathf.Deg2Rad * t, speed = Detailed ? a.speed : Mathf.Lerp(a.speed, b.speed, t), gear = t >= 1 ? b.gear : a.gear, state = state };
    }
}
