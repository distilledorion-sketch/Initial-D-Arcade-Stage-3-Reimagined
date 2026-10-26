using System;
using System.IO;
using UnityEngine.Profiling;

public sealed partial class Idas3SceneSmoke
{
    // Opt-in, bounded development-player diagnostic. GC.Alloc's marker value
    // is elapsed time, NOT bytes. Allocation sizes live in the trace metadata.
    private bool allocationTraceActive;
    private int previousProfilerMemory;

    private void StartAllocationTrace()
    {
        if (Profiler.enabled) throw new InvalidOperationException("A profiler session is already active.");
        previousProfilerMemory = Profiler.maxUsedMemory;
        Profiler.logFile = Path.Combine(root, "allocations.raw");
        Profiler.maxUsedMemory = 64 * 1024 * 1024;
        Profiler.enableBinaryLog = true;
        Profiler.enableAllocationCallstacks = true;
        allocationTraceActive = true;
        Profiler.enabled = true;
    }

    private static void AllocationTraceMarker(string name)
    {
        Profiler.BeginSample(name);
        Profiler.EndSample();
    }

    private static void ProbeAllocationTrace()
    {
        Profiler.BeginSample("IDAS3.AllocationProbe.Empty");
        Profiler.EndSample();
        Profiler.BeginSample("IDAS3.AllocationProbe.64K");
        GC.KeepAlive(new byte[65536]);
        Profiler.EndSample();
        Profiler.BeginSample("IDAS3.AllocationProbe.256K");
        GC.KeepAlive(new byte[262144]);
        Profiler.EndSample();
    }

    private void StopAllocationTrace()
    {
        if (!allocationTraceActive) return;
        allocationTraceActive = false;
        Profiler.enabled = false;
        Profiler.enableAllocationCallstacks = false;
        Profiler.enableBinaryLog = false;
        Profiler.logFile = "";
        Profiler.maxUsedMemory = previousProfilerMemory;
    }
}
