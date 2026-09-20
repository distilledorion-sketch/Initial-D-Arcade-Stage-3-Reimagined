using System;
using System.IO;
using System.Linq;
using System.Text;
using UnityEngine;

public static class Idas3ReplayDetailTests
{
    static int checks;
    static void Check(bool ok, string description) { if (!ok) throw new Exception(description); checks++; }
    static byte[] Package(byte[] raw)
    {
        var json = Encoding.UTF8.GetBytes("{\"condition\":6,\"weather\":0,\"night\":0,\"car\":0,\"manual\":1,\"ticks6000\":60000}");
        using var output = new MemoryStream(); using var writer = new BinaryWriter(output);
        writer.Write(json.Length); writer.Write(json); writer.Write(raw); return output.ToArray();
    }
    static void Reject(Action action)
    {
        try { action(); } catch (Exception) { checks++; return; }
        throw new Exception("Invalid detailed replay accepted");
    }
    public static void Run()
    {
        Idas3ReplayViewerTests.Run();
        var raw = new byte[96 + 600 * 160];
        void Word(int at, uint value) => BitConverter.GetBytes(value).CopyTo(raw, at);
        void Scalar(int at, float value) => BitConverter.GetBytes(value).CopyTo(raw, at);
        Word(0, 0x32524449); Word(4, 60000); Word(8, 600); Word(12, 60);
        Word(16, 96); Word(20, 160); Word(24, 1); Word(28, 12);
        for (uint i = 0; i < 600; i++)
        {
            int at = 96 + (int)i * 160;
            Word(at, i + 1); Scalar(at + 4, i); Scalar(at + 20, 30 + i / 10f); Word(at + 24, 3);
            Scalar(at + 28, 4321.125f + i); Scalar(at + 32, i + .125f); Scalar(at + 44, i / 1000f);
            Scalar(at + 56, .0025f); Scalar(at + 72, i / 10f);
            Word(at + 96, (i + 1) * 100); Word(at + 100, 600000 - i * 100); Word(at + 124, 4);
        }
        var compressed = Idas3ReplayCodec.Encode(raw);
        Check(raw.SequenceEqual(Idas3ReplayCodec.Decode(compressed)), "Compression preserves every recorded bit");
        Check(compressed.Length < raw.Length, "Detailed replay compresses");
        var plain = Idas3ReplayData.Parse(Package(raw)); var replay = Idas3ReplayData.Parse(Package(compressed));
        Check(replay.Detailed && replay.Frames.Length == 600 && replay.Appearance.Length == 15, "Detailed timeline and appearance parsed");
        for (int i = 0; i < 600; i++)
        {
            var sample = replay.Sample((i + 1) / 60.0);
            Check(sample.speed == plain.Frames[i].speed && sample.state.SequenceEqual(plain.Frames[i].state), "Sample restores actual RPM, speed, clocks and complete pose " + i);
        }
        var between = replay.Sample(100.5 / 60.0);
        Check(between.speed == replay.Frames[99].speed && between.state[0] == replay.Frames[99].state[0], "HUD retains recorded values between samples");
        Check(Math.Abs(Idas3ReplayData.Scalar(between.state[1]) - 99.625f) < .0001f, "Body movement interpolates between recorded transforms");
        Check(replay.Sample(50).state.SequenceEqual(replay.Frames[^1].state) && replay.Sample(-1).state.SequenceEqual(replay.Frames[0].state), "Endpoint and backwards seek");
        var bad = (byte[])compressed.Clone(); BitConverter.GetBytes(raw.Length - 1).CopyTo(bad, 4); Reject(() => Idas3ReplayCodec.Decode(bad));
        Reject(() => Idas3ReplayCodec.Decode(compressed[..^20]));
        bad = (byte[])raw.Clone(); BitConverter.GetBytes(float.NaN).CopyTo(bad, 96 + 28); Reject(() => Idas3ReplayData.Parse(Package(bad)));
        bad = (byte[])raw.Clone(); BitConverter.GetBytes(3).CopyTo(bad, 96 + 160); Reject(() => Idas3ReplayData.Parse(Package(bad)));
        string evidence = Path.GetFullPath("Verification/replay-detail-20260919");
        foreach (int course in new[] { 0, 1, 3, 9, 10 })
        {
            var captured = Idas3ReplayData.Load(Path.Combine(evidence, "course-" + course + ".idreplay"));
            Check(captured.Detailed && captured.Frames[^1].state[17] == captured.Metadata.ticks6000, "Original finish gate clock settlement retained: " + course);
        }
        File.WriteAllText(Path.Combine(evidence, "parser-tests.json"), JsonUtility.ToJson(new Report { passed = true, checks = checks, rawBytes = raw.Length, compressedBytes = compressed.Length }, true));
        Debug.Log("Detailed replay and codec tests passed: " + checks);
    }
    public static void BuildVerified() { Run(); Idas3Build.RebuildSadamineStagingScripts(); }
    [Serializable] sealed class Report { public bool passed; public int checks, rawBytes, compressedBytes; }
}
