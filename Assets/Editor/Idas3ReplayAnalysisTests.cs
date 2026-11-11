using System;
using System.IO;
using UnityEngine;

// Synthetic IDR2/IDT1 fixtures: no live saves, native session or replay files.
public static class Idas3ReplayAnalysisTests
{
    static int checks;
    static void Check(bool ok, string description) { if (!ok) throw new Exception(description); checks++; }
    static void Near(float actual, float expected, float tolerance, string description) => Check(Mathf.Abs(actual - expected) <= tolerance, description + ": " + actual + " != " + expected);
    static void Reject(Action action)
    {
        try { action(); } catch (Exception) { checks++; return; }
        throw new Exception("Invalid replay telemetry accepted");
    }
    sealed class Frame { public Vector3 position; public float yaw, speed = 20; public int gear = 3; public readonly uint[] state = new uint[33]; }
    static uint Bits(float value) => unchecked((uint)BitConverter.SingleToInt32Bits(value));
    const int Count = 600;
    static Frame[] Frames()
    {
        var frames = new Frame[Count];
        for (int i = 0; i < Count; i++) { var f = frames[i] = new Frame(); f.position = new Vector3(0, 0, i * 20 / 60f); f.state[17] = (uint)(i + 1) * 100; f.state[24] = 4; }
        return frames;
    }
    static byte[] Raw(Frame[] frames)
    {
        var raw = new byte[96 + frames.Length * 160];
        void Word(int at, uint value) => BitConverter.GetBytes(value).CopyTo(raw, at);
        void Scalar(int at, float value) => BitConverter.GetBytes(value).CopyTo(raw, at);
        Word(0, Idas3ReplayCodec.Detailed); Word(4, (uint)frames.Length * 100); Word(8, (uint)frames.Length); Word(12, 60);
        Word(16, 96); Word(20, 160); Word(24, 1); Word(28, 12);
        for (int i = 0; i < frames.Length; i++)
        {
            int at = 96 + i * 160; var f = frames[i];
            Word(at, (uint)i + 1); Scalar(at + 4, f.position.x); Scalar(at + 8, f.position.y); Scalar(at + 12, f.position.z);
            Scalar(at + 16, f.yaw); Scalar(at + 20, f.speed); Word(at + 24, (uint)f.gear);
            for (int n = 0; n < 33; n++) Word(at + 28 + n * 4, f.state[n]);
        }
        return raw;
    }
    // version 3 = applied speed loss; version 2 = 32-byte samples with rotation memory; version 1 = first 24-byte build.
    static byte[] Telemetry(Idas3ReplayData.TelemetrySample[] samples, int count, uint version = 3, uint? strideOverride = null)
    {
        int stride = version == 1 ? 24 : 32;
        var raw = new byte[16 + samples.Length * stride];
        BitConverter.GetBytes(Idas3ReplayCodec.Telemetry).CopyTo(raw, 0); BitConverter.GetBytes(version).CopyTo(raw, 4); BitConverter.GetBytes(strideOverride ?? (uint)stride).CopyTo(raw, 8); BitConverter.GetBytes((uint)count).CopyTo(raw, 12);
        for (int i = 0; i < samples.Length; i++)
        {
            int at = 16 + i * stride; var t = samples[i];
            BitConverter.GetBytes(t.slip).CopyTo(raw, at); BitConverter.GetBytes(t.yawRate).CopyTo(raw, at + 4); BitConverter.GetBytes(t.speedLoss).CopyTo(raw, at + 8);
            BitConverter.GetBytes(t.penalty).CopyTo(raw, at + 12); BitConverter.GetBytes(t.wallImpact).CopyTo(raw, at + 16);
            if (version >= 2) { BitConverter.GetBytes(t.pendingRotation).CopyTo(raw, at + 20); BitConverter.GetBytes(t.applyingRotation).CopyTo(raw, at + 24); }
            BitConverter.GetBytes(t.flags).CopyTo(raw, at + stride - 4);
        }
        return raw;
    }
    static Idas3ReplayData.Details Metadata() => new Idas3ReplayData.Details { condition = 6, car = 0, manual = 1, ticks6000 = Count * 100 };
    static byte[] Package(byte[] raw, byte[] telemetry)
    {
        var metadata = Metadata(); var player = Idas3ReplayCodec.Encode(raw);
        var stored = telemetry == null ? null : Idas3ReplayCodec.Encode(telemetry, Idas3ReplayCodec.Telemetry);
        metadata.telemetryBytes = stored?.Length ?? 0;
        return Idas3ReplayLibrary.Package(metadata, player, Array.Empty<byte>(), stored);
    }

    public static void Run()
    {
        // Steering: 540 degrees lock to lock, original +input is a left turn.
        float fullLeft = (1f * Mathf.PI) * Idas3ReplayAnalysis.WheelAngleScale;
        Near(Idas3ReplayAnalysis.WheelDegreesFromInput(Idas3ReplayAnalysis.InputFromWheelAngle(fullLeft)), -270, .01f, "Full left lock is 270 degrees counter-clockwise");
        Near(Idas3ReplayAnalysis.WheelDegreesFromInput(Idas3ReplayAnalysis.InputFromWheelAngle(-fullLeft)), 270, .01f, "Full right lock mirrors left");
        Near(Idas3ReplayAnalysis.WheelDegreesFromInput(Idas3ReplayAnalysis.InputFromWheelAngle(-fullLeft * .5f)), 135, .01f, "Half input is 135 degrees");
        Check(Idas3ReplayAnalysis.WheelDegreesFromInput(4) == -270, "Wheel rotation clamps at lock");

        var frames = Frames();
        for (int i = 0; i < Count; i++)
        {
            var f = frames[i];
            if (i >= 500) f.speed = 70;
            if (i > 0) f.position = frames[i - 1].position + new Vector3(0, 0, f.speed / 60);
            f.state[0] = Bits(5000 + i); f.state[6] = Bits(-fullLeft * .5f);
            f.state[15] = Bits(1); f.gear = i < 300 ? 3 : i < 451 ? 4 : 3;
            if (i >= 200) { f.state[23] = 1; f.state[19] = 20000; }
            if (i >= 400) { f.state[23] = 2; f.state[20] = 40000; }
        }
        // Yaw across the +/-pi wrap: native yaw increases to the left.
        frames[50].yaw = 179 * Mathf.Deg2Rad; frames[51].yaw = -179 * Mathf.Deg2Rad;
        // A frame of travel forward-right with the nose straight ahead.
        frames[120].position = frames[119].position + new Vector3(-10 / 60f, 0, 10 / 60f);
        var raw = Raw(frames);

        var samples = new Idas3ReplayData.TelemetrySample[Count];
        for (int i = 0; i < Count; i++) samples[i].penalty = i < 100 ? 0 : 1.2f * (1 - (i - 100) / 700f);
        for (int i = 100; i < 105; i++) { samples[i].flags = 1; samples[i].wallImpact = i == 102 ? 5 : 1; }
        samples[250].flags = 1; samples[250].wallImpact = 2;
        samples[51].yawRate = 2 * Mathf.Deg2Rad * 60; // Recorded simulation rate, same sign as yaw.
        // Steering memory, original sign (+ is left): 12.5 left pending, 40 right applying.
        samples[60].pendingRotation = 12.5f; samples[60].applyingRotation = -40;
        // Same-frame down+up at 350 leaves gear 4 unchanged; held bits for 3 frames.
        samples[350].flags = 2 | 4 | 8 | 16; samples[351].flags = 8 | 16; samples[352].flags = 8;
        // Down press at 450, gear follows one frame later at 451.
        samples[450].flags = 2 | 8;
        samples[200].speedLoss = .01f; // m/s taken this frame

        // Package format: IDT1 rides after IDR2 and is optional.
        var replay = Idas3ReplayData.Parse(Package(raw, Telemetry(samples, Count)));
        Check(replay.Telemetry != null && replay.Telemetry.Length == Count && replay.Telemetry[102].wallImpact == 5 && replay.Telemetry[102].WallContact, "IDT1 telemetry parsed beside IDR2");
        Check(replay.SpeedLossRecorded && replay.RotationRecorded && replay.Telemetry[60].pendingRotation == 12.5f && replay.Telemetry[60].applyingRotation == -40, "IDT1 v2 keeps steering memory");
        var press = replay.Telemetry[350];
        Check(press.ShiftDown && press.ShiftUp && press.DownHeld && press.UpHeld && !replay.Telemetry[352].UpHeld && replay.Telemetry[352].DownHeld, "Shift press and held bits round-trip");
        var v1Samples = new Idas3ReplayData.TelemetrySample[Count];
        for (int i = 0; i < Count; i++) { v1Samples[i] = samples[i]; v1Samples[i].flags &= 1; }
        var first = Idas3ReplayData.Parse(Package(raw, Telemetry(v1Samples, Count, 1)));
        Check(first.Telemetry != null && !first.RotationRecorded && first.Telemetry[102].WallContact && first.Telemetry[60].pendingRotation == 0, "IDT1 v1 recordings open without rotation memory");
        Reject(() => Idas3ReplayData.Parse(Package(raw, Telemetry(samples, Count, 1)))); // v1 with shift bits
        Reject(() => Idas3ReplayData.Parse(Package(raw, Telemetry(samples, Count, 2, 24))));
        var second = Idas3ReplayData.Parse(Package(raw, Telemetry(samples, Count, 2)));
        Check(second.RotationRecorded && !second.SpeedLossRecorded && !new Idas3ReplayAnalysis(second).HasSpeedLoss && new Idas3ReplayAnalysis(second).SpeedLoss[200] == 0, "IDT1 v2 speed loss is treated as not recorded");
        Reject(() => Idas3ReplayData.Parse(Package(raw, Telemetry(samples, Count, 4))));
        Check(Idas3ReplayData.Parse(Package(raw, null)).Telemetry == null, "Recordings without IDT1 still open");
        Reject(() => Idas3ReplayData.Parse(Package(raw, Telemetry(samples[..^1], Count - 1))));
        var bad = (Idas3ReplayData.TelemetrySample[])samples.Clone(); bad[7].penalty = float.NaN; Reject(() => Idas3ReplayData.Parse(Package(raw, Telemetry(bad, Count))));
        bad = (Idas3ReplayData.TelemetrySample[])samples.Clone(); bad[7].flags = 32; Reject(() => Idas3ReplayData.Parse(Package(raw, Telemetry(bad, Count))));
        var truncated = Package(raw, Telemetry(samples, Count)); Array.Resize(ref truncated, truncated.Length - 1); Reject(() => Idas3ReplayData.Parse(truncated));
        string folder = Path.Combine(Path.GetTempPath(), "idas3-replay-analysis-" + Guid.NewGuid().ToString("N"));
        try
        {
            var saved = Idas3ReplayData.Load(Idas3ReplayLibrary.Save(folder, Metadata(), raw, Array.Empty<byte>(), Telemetry(samples, Count)));
            Check(saved.Telemetry != null && saved.Telemetry[250].wallImpact == 2, "Library saves IDT1 with the replay");
            var kept = Idas3ReplayData.Load(Idas3ReplayLibrary.Save(folder, Metadata(), raw, Array.Empty<byte>(), Telemetry(samples[..^1], Count - 1)));
            Check(kept.Telemetry == null && kept.Frames.Length == Count, "Unusable IDT1 never costs the replay");
        }
        finally { if (Directory.Exists(folder)) Directory.Delete(folder, true); }

        var a = new Idas3ReplayAnalysis(replay);
        Check(a.HasTelemetry && a.Detailed && a.Count == Count, "Analysis sees detailed telemetry");
        Near(a.WheelDegrees[10], 135, .01f, "Recorded wheel angle becomes steering-wheel rotation");
        Near(a.YawRateDegrees[51], -120, .5f, "Recorded yaw rate is left-negative");
        Near(a.DriftDegrees[120], -45, .5f, "Nose left of a forward-right travel is a left drift");
        Near(a.DriftDegrees[60], 0, .01f, "Straight travel has no drift");
        Check(a.HasRotation && a.HasShiftButtons, "Analysis sees rotation memory and shift buttons");
        Check(a.PendingRotation[60] == -12.5f && a.ApplyingRotation[60] == 40 && a.MaxAbsPending == 12.5f, "Rotation memory is right-positive like steering");
        Check(a.DownHeld[351] && a.UpHeld[351] && a.DownHeld[352] && !a.UpHeld[352] && !a.DownPress[351], "Held buttons separate from press edges");
        Check(a.HasSpeedLoss, "Analysis sees applied speed loss");
        Near(a.SpeedLoss[200], 2.16f, .001f, "Speed loss is km/h per second"); Check(a.SpeedLoss[199] == 0, "No loss on other frames");
        Near(a.SpeedKmh[0], 72, .01f, "Speed in km/h");

        int shifts = 0, walls = 0, sections = 0;
        foreach (var e in a.Events)
        {
            if (e.kind == Idas3ReplayAnalysis.EventKind.Shift)
            {
                shifts++;
                if (e.frame == 300) Check(e.from == 3 && e.to == 4 && e.rpm == 5299 && !e.down && !e.up, "Gear change without a press still counts, with RPM before the shift");
                else if (e.frame == 350) Check(Idas3ReplayAnalysis.SameFrameShift(e) && e.from == 4 && e.to == 4, "Same-frame down+up shows although the gear is unchanged");
                else Check(e.frame == 450 && e.down && !e.up && e.from == 4 && e.to == 3, "A gear change trailing a press joins that press");
            }
            if (e.kind == Idas3ReplayAnalysis.EventKind.Wall) { walls++; Check(e.frame == 100 ? e.impact == 5 : e.frame == 250 && e.impact == 2, "Wall hit peak impact"); }
            if (e.kind == Idas3ReplayAnalysis.EventKind.Section) { sections++; Check(e.frame == 200 ? e.split6000 == 20000 : e.frame == 400 && e.split6000 == 40000, "Section crossing split"); }
        }
        Check(shifts == 3 && walls == 2 && sections == 2, "Three shifts, two wall rising edges and two sections");
        Check(a.Sections.Length == 3 && a.Sections[2].split6000 == 60000 && a.Sections[1].duration6000 == 20000 && !a.Sections[0].hasDelta, "Sections close at the finish without a reference");
        Check(a.FrameAt(5) == 299 && a.FrameAt(-1) == 0 && a.FrameAt(99) == Count - 1, "Frame lookup");
        Check(a.Events[a.NextEvent(0)].frame == 100 && a.PreviousEvent(1) == -1 && a.Events[a.PreviousEvent(10)].frame == 450, "Event navigation");

        var reference = Frames();
        for (int i = 200; i < Count; i++) { reference[i].state[23] = 1; reference[i].state[19] = 21000; }
        for (int i = 400; i < Count; i++) { reference[i].state[23] = 2; reference[i].state[20] = 39000; }
        var compared = new Idas3ReplayAnalysis(replay, Idas3ReplayData.Parse(Package(Raw(reference), null)));
        Check(compared.Sections[0].hasDelta && compared.Sections[0].delta6000 == -1000 && compared.Sections[1].delta6000 == 1000 && compared.Sections[2].delta6000 == 0, "Section deltas against the opponent");
        var older = new Idas3ReplayAnalysis(Idas3ReplayData.Parse(Package(raw, null)));
        Check(!older.HasTelemetry && older.Penalty[150] == 0 && older.WheelDegrees[10] == a.WheelDegrees[10], "Older recordings keep derived steering without penalty");
        Check(!older.HasRotation && !older.HasShiftButtons && Array.FindAll(older.Events, e => e.kind == Idas3ReplayAnalysis.EventKind.Shift).Length == 2, "Older recordings derive shifts from gear changes");
        Check(Idas3ReplayAnalysis.ShiftLabel(Array.Find(a.Events, e => e.frame == 350)).StartsWith("↓↑"), "Same-frame shift label");
        Near(older.YawRateDegrees[51], -120, .5f, "Derived yaw rate crosses the wrap and is left-negative");
        Debug.Log("Replay analysis tests passed: " + checks);
    }
}
