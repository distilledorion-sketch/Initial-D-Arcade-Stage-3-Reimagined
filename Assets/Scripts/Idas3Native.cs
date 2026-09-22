using System;
using System.Runtime.InteropServices;
using System.Text;

internal static class Idas3Native
{
    private const string Library = "Idas3Unity";
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] internal static extern int Idas3SceneShowImportedCourseMenu([MarshalAs(UnmanagedType.LPUTF8Str)] string root);
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] internal static extern int Idas3SceneRegisterImportedCourse([MarshalAs(UnmanagedType.LPUTF8Str)] string root);
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] internal static extern int Idas3SceneStartImportedCourse([MarshalAs(UnmanagedType.LPUTF8Str)] string root,int reverse);
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] internal static extern int Idas3SceneStartImportedCourseConditions([MarshalAs(UnmanagedType.LPUTF8Str)] string root,int reverse,int night,int wet);
    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    internal struct FrameInput
    {
        public uint size, flags;
        public double deltaSeconds;
        public uint key0, key1, key2, key3, key4, key5, key6, key7;
        public uint padButtons;
        public int thumbLX, thumbLY, thumbRX, thumbRY;
        public uint leftTrigger, rightTrigger, padConnected;
        public int width, height;

        public void SetKey(int key)
        {
            uint bit = 1u << (key & 31);
            switch (key >> 5)
            {
                case 0: key0 |= bit; break; case 1: key1 |= bit; break;
                case 2: key2 |= bit; break; case 3: key3 |= bit; break;
                case 4: key4 |= bit; break; case 5: key5 |= bit; break;
                case 6: key6 |= bit; break; case 7: key7 |= bit; break;
            }
        }
    }
    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    internal struct Status
    {
        public uint size, state;
        public ulong renderedFrames, simulationTicks, textureGeneration;
        public int width, height, frontendStage, attractChild, course, car, racePhase;
        public uint flags;
        public float speedMetresPerSecond, rpm;
        public uint lastEventId, reserved;
    }
    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    internal struct Options
    {
        public uint size, version;
        public float masterGain, musicGain, engineGain, effectsGain;
        public uint cameraView, paused, managedPauseOverlay, reserved;
    }
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3SceneGetOptions(ref Options options);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3SceneApplyOptions(ref Options options);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3SceneSetControllerResponse(int response);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3SceneSetTireVolume(float value);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern float Idas3SceneGetTireVolume();
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3SceneGetControllerResponse();
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3SceneSetSteeringDeadzone(float value);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern float Idas3SceneGetSteeringDeadzone();
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3SceneSetSteeringSmoothing(float value);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3SceneSetPerformance(int rainDetail);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3SceneSetMapSize(int size);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3SceneSetMapZoom(int zoom);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3SceneSetAiDifficulty(int difficulty);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern float Idas3SceneGetSteeringSmoothing();
    [StructLayout(LayoutKind.Sequential, Pack=8)]
    internal struct WheelState {
        public uint size,version;
        public ulong simulationTicks;
        public float speed,steering,headingError,wallLateral,impact;
        public uint flags;
    }
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3SceneGetWheelState(ref WheelState state);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3SceneSetPaused(int paused);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3SceneRestart();
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3SceneReturnToCourse();
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3SceneCanFullTune();
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3SceneFullTune();
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3SceneChallenger(int action);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3SceneRetire();
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int Idas3SceneSetPreRaceNames(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string local,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string opponent);
    internal static Options ReadOptions()
    {
        var options = new Options { size = (uint)Marshal.SizeOf<Options>() };
        if (options.size != 40 || Idas3SceneGetOptions(ref options) != 1 || options.version != 1)
            throw new InvalidOperationException("Native options ABI mismatch. " + Error());
        return options;
    }
    [StructLayout(LayoutKind.Sequential)]
    internal struct Gamepad
    {
        public ushort buttons;
        public byte leftTrigger, rightTrigger;
        public short thumbLX, thumbLY, thumbRX, thumbRY;
    }
    [StructLayout(LayoutKind.Sequential)]
    internal struct PadState { public uint packet; public Gamepad gamepad; }

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern uint Idas3UnityVersion();
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int Idas3UnityQueueInitialize(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string assetRoot,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string saveRoot,
        IntPtr texture, int width, int height, int audioEnabled);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3UnityQueueFrame(ref FrameInput input);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3UnityQueueShutdown();
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3UnityWaitForEvent(int token, int timeoutMilliseconds);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern IntPtr Idas3UnityGetRenderEventFunc();
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern IntPtr Idas3UnityGetTexture();
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int Idas3UnityGetStatus(ref Status status);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] private static extern int Idas3UnityCopyError([Out] byte[] target, int capacity);
    [DllImport("xinput9_1_0.dll", EntryPoint = "XInputGetState")] internal static extern uint ReadGamepad(uint user, out PadState state);

    internal static string Error()
    {
        var bytes = new byte[8192];
        int count = Idas3UnityCopyError(bytes, bytes.Length);
        return Encoding.UTF8.GetString(bytes, 0, Math.Max(0, Math.Min(count, bytes.Length - 1)));
    }
    // The size is fixed; recomputing it per call is pure per-frame cost.
    private static readonly uint StatusSize = (uint)Marshal.SizeOf<Status>();
    internal static Status ReadStatus()
    {
        var status = new Status { size = StatusSize };
        Idas3UnityGetStatus(ref status);
        return status;
    }
}
