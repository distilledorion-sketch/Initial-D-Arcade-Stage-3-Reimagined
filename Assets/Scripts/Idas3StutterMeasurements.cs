using System;

// Main-thread counters only. No SDK data, player identity or network payloads.
internal static class Idas3StutterMeasurements
{
    internal struct Sample {
        internal int Frame, SdkTicks, Callbacks;
        internal double SdkMilliseconds, CallbackMilliseconds;
    }
    internal static bool Enabled;
    private static Sample current;
    internal static void Reset() { current = new Sample { Frame = -1 }; }
    private static void Frame(int frame) {
        if (current.Frame != frame) current = new Sample { Frame = frame };
    }
    internal static void Sdk(int frame, double seconds) {
        if (!Enabled) return;
        Frame(frame); ++current.SdkTicks;
        current.SdkMilliseconds += Math.Max(0, seconds) * 1000;
    }
    internal static void Callback(int frame, double seconds) {
        if (!Enabled) return;
        Frame(frame); ++current.Callbacks;
        current.CallbackMilliseconds += Math.Max(0, seconds) * 1000;
    }
    internal static Sample Read(int frame) => current.Frame == frame ? current : new Sample { Frame = frame };
}
