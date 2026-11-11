using System;
using System.Collections.Generic;
using UnityEngine;

// Per-frame driving reference derived from one replay track. IDR2 fields are
// available for every detailed recording; IDT1 adds penalty, slip and walls.
// Signs: steering, drift and yaw rate are positive to the RIGHT (clockwise).
public sealed class Idas3ReplayAnalysis
{
    // original_contact.cpp writes actor+0x3C = (drive+0x1CC * pi) * 0.235f, and
    // original_controls.cpp negates host steering, so +0x1CC is a LEFT input.
    public const float WheelAngleScale = 0.235f;
    public const float LockDegrees = 270; // 540 degrees lock to lock.
    public enum EventKind { Shift, Wall, Section }
    // down/up are recorded shift-button press edges; both on one frame is a
    // same-frame shift that may leave the gear unchanged.
    public struct Event { public EventKind kind; public int frame; public double seconds; public int from, to, section; public float rpm, impact; public uint split6000; public bool down, up; }
    public struct Section { public int index; public uint split6000, duration6000; public int delta6000; public bool hasDelta; }
    public const float ApplyingLimit = 70; // original_dynamics.cpp clamps +284 to [-70, 70].

    public readonly int Count;
    public readonly bool Detailed, HasTelemetry, HasRotation, HasShiftButtons, HasSpeedLoss;
    public readonly double[] Seconds;
    public readonly int[] Gear;
    public readonly float[] SpeedKmh, Rpm, Throttle, Brake, SteeringInput, WheelDegrees, DriftDegrees, YawRateDegrees, HeadingDegrees;
    // SpeedLoss is the deceleration the loss applied, in km/h per second.
    public readonly float[] Penalty, SpeedLoss, HeadingError, WallImpact;
    // Steering memory (+288 pending, +284 applying), right-positive like steering.
    public readonly float[] PendingRotation, ApplyingRotation;
    public readonly bool[] Wall, DownPress, UpPress, DownHeld, UpHeld;
    public readonly Event[] Events;
    public readonly Section[] Sections;
    public readonly float MaxSpeedKmh, MaxPenalty, MaxSpeedLoss, MaxAbsDrift, MaxAbsYawRate, MaxAbsPending;

    public static float InputFromWheelAngle(float radians) => radians / (Mathf.PI * WheelAngleScale);
    // Clockwise-positive steering-wheel rotation for an original input value.
    public static float WheelDegreesFromInput(float input) => -Mathf.Clamp(input, -1, 1) * LockDegrees;

    public Idas3ReplayAnalysis(Idas3ReplayData replay, Idas3ReplayData sectionReference = null)
    {
        var frames = replay.Frames; Count = frames.Length; Detailed = replay.Detailed;
        var telemetry = replay.Telemetry; HasTelemetry = telemetry != null && telemetry.Length == Count;
        HasRotation = HasShiftButtons = HasTelemetry && replay.RotationRecorded;
        HasSpeedLoss = HasTelemetry && replay.SpeedLossRecorded;
        Seconds = new double[Count]; Gear = new int[Count];
        SpeedKmh = new float[Count]; Rpm = new float[Count]; Throttle = new float[Count]; Brake = new float[Count];
        SteeringInput = new float[Count]; WheelDegrees = new float[Count]; DriftDegrees = new float[Count]; YawRateDegrees = new float[Count];
        HeadingDegrees = new float[Count];
        Penalty = new float[Count]; SpeedLoss = new float[Count]; HeadingError = new float[Count]; WallImpact = new float[Count]; Wall = new bool[Count];
        PendingRotation = new float[Count]; ApplyingRotation = new float[Count];
        DownPress = new bool[Count]; UpPress = new bool[Count]; DownHeld = new bool[Count]; UpHeld = new bool[Count];
        double last = frames[^1].tick;
        for (int i = 0; i < Count; i++)
        {
            var p = frames[i];
            // Same timeline mapping as Idas3ReplayData.Sample.
            Seconds[i] = Detailed ? p.tick / 60.0 : last <= 0 ? 0 : p.tick / last * replay.Duration;
            Gear[i] = p.gear; SpeedKmh[i] = p.speed * 3.6f; HeadingDegrees[i] = Mathf.Repeat(p.yaw * Mathf.Rad2Deg, 360);
            if (Detailed)
            {
                Rpm[i] = Idas3ReplayData.Scalar(p.state[0]);
                SteeringInput[i] = InputFromWheelAngle(Idas3ReplayData.Scalar(p.state[6])); WheelDegrees[i] = WheelDegreesFromInput(SteeringInput[i]);
                Throttle[i] = Idas3ReplayData.Scalar(p.state[15]); Brake[i] = Idas3ReplayData.Scalar(p.state[16]);
            }
            if (HasTelemetry)
            {
                var t = telemetry[i];
                // +238 is m/s and the loss is taken once per 60 Hz frame.
                if (HasSpeedLoss) SpeedLoss[i] = t.speedLoss * 3.6f * 60;
                Penalty[i] = t.penalty; HeadingError[i] = t.slip; WallImpact[i] = t.wallImpact; Wall[i] = t.WallContact;
                // Memory shares the original steering sign: positive is LEFT.
                PendingRotation[i] = -t.pendingRotation; ApplyingRotation[i] = -t.applyingRotation;
                DownPress[i] = t.ShiftDown; UpPress[i] = t.ShiftUp; DownHeld[i] = t.DownHeld; UpHeld[i] = t.UpHeld;
            }
            int a = Math.Max(0, i - 1), b = Math.Max(1, i); if (b >= Count) b = Count - 1;
            float dt = (float)(Seconds[b] - Seconds[a]);
            if (dt > 0)
            {
                float yawRate = DeltaRadians(frames[a].yaw, frames[b].yaw) / dt;
                // Native yaw increases to the left (screen right is cross(forward, up)).
                YawRateDegrees[i] = -(HasTelemetry ? telemetry[i].yawRate : yawRate) * Mathf.Rad2Deg;
                DriftDegrees[i] = Drift(frames[a].position, frames[b].position, p.yaw, dt);
            }
        }
        Events = BuildEvents(replay);
        Sections = BuildSections(replay, sectionReference);
        for (int i = 0; i < Count; i++)
        {
            MaxSpeedKmh = Mathf.Max(MaxSpeedKmh, SpeedKmh[i]); MaxPenalty = Mathf.Max(MaxPenalty, Penalty[i]); MaxSpeedLoss = Mathf.Max(MaxSpeedLoss, SpeedLoss[i]);
            MaxAbsDrift = Mathf.Max(MaxAbsDrift, Mathf.Abs(DriftDegrees[i])); MaxAbsYawRate = Mathf.Max(MaxAbsYawRate, Mathf.Abs(YawRateDegrees[i]));
            MaxAbsPending = Mathf.Max(MaxAbsPending, Mathf.Abs(PendingRotation[i]));
        }
    }

    static float DeltaRadians(float from, float to) => Mathf.DeltaAngle(from * Mathf.Rad2Deg, to * Mathf.Rad2Deg) * Mathf.Deg2Rad;

    // Angle from the travel direction to the nose, clockwise positive. Below
    // walking pace the travel direction is noise, so report no drift.
    static float Drift(Vector3 from, Vector3 to, float yaw, float dt)
    {
        float vx = (to.x - from.x) / dt, vz = (to.z - from.z) / dt;
        if (vx * vx + vz * vz < 9) return 0;
        float forward = vx * Mathf.Sin(yaw) + vz * Mathf.Cos(yaw), right = -vx * Mathf.Cos(yaw) + vz * Mathf.Sin(yaw);
        return -Mathf.Atan2(right, forward) * Mathf.Rad2Deg;
    }

    Event[] BuildEvents(Idas3ReplayData replay)
    {
        var events = new List<Event>();
        int edgeEvent = -1, edgeFrame = int.MinValue;
        for (int i = 0; i < Count; i++)
        {
            // Button edges are the shift; a same-frame down+up can leave the
            // gear unchanged, so gear comparison alone misses it.
            int previousGear = Gear[Math.Max(0, i - 1)];
            if (DownPress[i] || UpPress[i])
            {
                events.Add(new Event { kind = EventKind.Shift, frame = i, seconds = Seconds[i], from = previousGear, to = Gear[i], rpm = Detailed ? Rpm[Math.Max(0, i - 1)] : float.NaN, down = DownPress[i], up = UpPress[i] });
                edgeEvent = events.Count - 1; edgeFrame = i;
            }
            else if (i > 0 && Gear[i] != previousGear)
            {
                // A gear change trailing a recorded press belongs to that press.
                if (edgeEvent >= 0 && i - edgeFrame <= 3 && events[edgeEvent].to == previousGear) { var e = events[edgeEvent]; e.to = Gear[i]; events[edgeEvent] = e; edgeEvent = -1; }
                else events.Add(new Event { kind = EventKind.Shift, frame = i, seconds = Seconds[i], from = previousGear, to = Gear[i], rpm = Detailed ? Rpm[i - 1] : float.NaN });
            }
            if (i == 0) continue;
            if (Wall[i] && !Wall[i - 1])
            {
                float impact = 0; for (int n = i; n < Count && Wall[n]; n++) impact = Mathf.Max(impact, WallImpact[n]);
                events.Add(new Event { kind = EventKind.Wall, frame = i, seconds = Seconds[i], impact = impact });
            }
            if (Detailed)
            {
                uint before = replay.Frames[i - 1].state[23], now = replay.Frames[i].state[23];
                if (now > before && now >= 1 && now <= 4)
                    events.Add(new Event { kind = EventKind.Section, frame = i, seconds = Seconds[i], section = (int)now, split6000 = replay.Frames[i].state[18 + now] });
            }
        }
        if (!Detailed && replay.Metadata.splits != null)
            for (int s = 0; s < replay.Metadata.splits.Length; s++)
            {
                double at = replay.Metadata.splits[s] / 6000.0; int frame = FrameAt(at);
                events.Add(new Event { kind = EventKind.Section, frame = frame, seconds = at, section = s + 1, split6000 = (uint)replay.Metadata.splits[s] });
            }
        events.Sort((x, y) => x.seconds.CompareTo(y.seconds));
        return events.ToArray();
    }

    // Cumulative checkpoint stamps from the final sample, closed by the result.
    static List<uint> Stamps(Idas3ReplayData replay)
    {
        var stamps = new List<uint>();
        if (replay.Detailed)
        {
            var last = replay.Frames[^1].state;
            for (int s = 0; s < Math.Min(4u, last[23]); s++) if (last[19 + s] > 0 && (stamps.Count == 0 || last[19 + s] > stamps[^1])) stamps.Add(last[19 + s]);
        }
        else if (replay.Metadata.splits != null) foreach (int split in replay.Metadata.splits) stamps.Add((uint)split);
        uint finish = (uint)replay.Metadata.ticks6000;
        if (replay.Metadata.outcome == 0 && (stamps.Count == 0 || stamps[^1] < finish)) stamps.Add(finish);
        return stamps;
    }

    static Section[] BuildSections(Idas3ReplayData replay, Idas3ReplayData reference)
    {
        var mine = Stamps(replay); var theirs = reference != null ? Stamps(reference) : null;
        var sections = new Section[mine.Count]; uint previous = 0;
        for (int s = 0; s < mine.Count; s++)
        {
            sections[s] = new Section { index = s + 1, split6000 = mine[s], duration6000 = mine[s] - previous };
            if (theirs != null && s < theirs.Count) { sections[s].delta6000 = (int)mine[s] - (int)theirs[s]; sections[s].hasDelta = true; }
            previous = mine[s];
        }
        return sections;
    }

    // Last frame at or before the playback position.
    public int FrameAt(double seconds)
    {
        int lo = 0, hi = Count - 1;
        while (lo < hi) { int mid = (lo + hi + 1) / 2; if (Seconds[mid] <= seconds + 1e-9) lo = mid; else hi = mid - 1; }
        return lo;
    }
    public int PreviousEvent(double seconds) { for (int i = Events.Length - 1; i >= 0; i--) if (Events[i].seconds < seconds - .05) return i; return -1; }
    public int NextEvent(double seconds) { for (int i = 0; i < Events.Length; i++) if (Events[i].seconds > seconds + .05) return i; return -1; }
    public static bool SameFrameShift(Event e) => e.kind == EventKind.Shift && e.down && e.up;
    static string GearName(int gear) => gear == 0 ? "N" : gear.ToString();
    public static string ShiftLabel(Event e) =>
        SameFrameShift(e) ? "↓↑ " + Idas3MenuLocalization.T("same frame") + (e.from == e.to ? " (" + GearName(e.to) + ")" : " " + GearName(e.from) + "→" + GearName(e.to)) :
        e.from == e.to ? (e.down ? "↓ " : e.up ? "↑ " : "") + GearName(e.to) : GearName(e.from) + "→" + GearName(e.to);
    public int LastEvent(EventKind kind, double seconds) { for (int i = Events.Length - 1; i >= 0; i--) if (Events[i].kind == kind && Events[i].seconds <= seconds + 1e-9) return i; return -1; }
}
