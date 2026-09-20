using System;
using System.IO;
using System.Text;
using UnityEngine;

public static class Idas3ReplayViewerTests
{
    static int checks;
    static void Check(bool value, string description) { if (!value) throw new Exception(description); checks++; }
    static byte[] Fixture(string fields = "")
    {
        var json = Encoding.UTF8.GetBytes("{\"condition\":6,\"weather\":0,\"night\":0,\"car\":0,\"manual\":1,\"ticks6000\":60000" + fields + "}");
        using var stream = new MemoryStream(); using var writer = new BinaryWriter(stream);
        writer.Write(json.Length); writer.Write(json); writer.Write(0x31524449); writer.Write(60000); writer.Write(201); writer.Write(60);
        for (int tick = 0; tick <= 600; tick += 3) { writer.Write(tick); writer.Write((float)tick); writer.Write(0f); writer.Write(0f); writer.Write((tick == 0 ? 179f : -179f) * Mathf.Deg2Rad); writer.Write(20f); writer.Write(tick == 600 ? 0 : 3); }
        return stream.ToArray();
    }
    static void Reject(byte[] data)
    {
        try { Idas3ReplayData.Parse(data); } catch (Exception) { checks++; return; }
        throw new Exception("Invalid replay was accepted");
    }
    public static void Run()
    {
        var bytes = Fixture(); var replay = Idas3ReplayData.Parse(bytes);
        Check(replay.Frames.Length == 201 && replay.Duration == 10, "Header and duration");
        Check(replay.Sample(-1).position.x == 0 && replay.Sample(11).position.x == 600 && replay.Sample(10).gear == 0, "Clamped endpoints and neutral final gear");
        Check(Math.Abs(Math.Abs(replay.Sample(.025).yaw) - Mathf.PI) < .001, "Yaw interpolates across wrap using shortest angle");
        Check(replay.Sample(7).position.x == 420 && replay.Sample(2).position.x == 120, "Arbitrary forward and backward seeks");
        Reject(new byte[100]); Reject(new byte[1020001]); Reject(bytes[..^1]);
        int at = 4 + BitConverter.ToInt32(bytes, 0); var bad = (byte[])bytes.Clone(); BitConverter.GetBytes(float.NaN).CopyTo(bad, at + 20); Reject(bad);
        bad = (byte[])bytes.Clone(); BitConverter.GetBytes(7).CopyTo(bad, at + 44); Reject(bad);
        bad = (byte[])bytes.Clone(); BitConverter.GetBytes(60001).CopyTo(bad, at + 4); Reject(bad);
        Check(Idas3ReplayData.Parse(Fixture(",\"splits\":[20000,40000,60000,0]")).Metadata.splits.Length == 3, "Short-course trailing empty section");
        Reject(Fixture(",\"splits\":[40000,20000,60000]"));
        Reject(Fixture(",\"splits\":[20000,0,60000]"));
        Reject(Fixture(",\"nameGlyphs\":[999]"));
        Check(Idas3ReplayData.Parse(Fixture(",\"splits\":[0,0,0,0]")).Metadata.splits.Length == 0, "Absent historical split metadata");
        string evidence = Path.GetFullPath("Verification/replay-viewer-20260919");
        foreach (int course in new[] { 6, 19, 20 }) { var real = Idas3ReplayData.Load(Path.Combine(evidence, "course-" + course + ".idreplay")); Check(real.Metadata.condition == course && real.Sample(real.Duration).position == real.Frames[^1].position, "Recorded course " + course); }
        File.WriteAllText(Path.Combine(evidence, "parser-tests.json"), "{\"passed\":true,\"checks\":" + checks + "}");
        Debug.Log("Replay parser and seek tests passed: " + checks);
    }
}
