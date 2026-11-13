using System;
using System.Globalization;
using System.IO;
using System.Text;
using UnityEngine;

// Optional observation of ordinary play. Does not enable smoke-test input,
// redirect saves, disable services, change audio, or manually render a camera.
[DefaultExecutionOrder(32000)]
public sealed class Idas3StutterTrace : MonoBehaviour
{
    internal static readonly bool Requested = Array.IndexOf(Environment.GetCommandLineArgs(), "-idas3-stutter-trace") >= 0;
    private const int Capacity = 36000;
    private struct Row {
        internal int frame, hostFrame, stage, phase, course, gc0, gc1, gc2;
        internal uint flags;
        internal ulong ticks;
        internal double seconds, precedingFrameMs, nativeMs, sceneMs, uiMs, networkMs, commandMs;
        internal bool ready, focused;
        internal Idas3StutterMeasurements.Sample eos;
    }
    private Row[] rows;
    private int count, next;
    private long observed;
    private double began;
    private bool saved;
    private string folder;
    private Idas3SceneGame host;

    internal void Initialize(Idas3SceneGame game, string directory) {
        host = game; folder = directory; rows = new Row[Capacity];
        began = Time.realtimeSinceStartupAsDouble;
        Idas3StutterMeasurements.Reset(); Idas3StutterMeasurements.Enabled = true;
    }
    private void LateUpdate() {
        if (rows == null || saved || host == null) return;
        var state = host.Status;
        bool measured = host.PerformanceSampleFrame == Time.frameCount;
        rows[next] = new Row {
            frame = Time.frameCount, hostFrame = host.PerformanceSampleFrame,
            seconds = Time.realtimeSinceStartupAsDouble - began,
            precedingFrameMs = Time.unscaledDeltaTime * 1000,
            ready = host.Ready, focused = Application.isFocused,
            stage = state.frontendStage, phase = state.racePhase, course = state.course,
            flags = state.flags, ticks = state.simulationTicks,
            nativeMs = measured ? host.NativeStepMilliseconds : 0,
            sceneMs = measured ? host.RendererMilliseconds : 0,
            uiMs = measured ? host.UiMilliseconds : 0,
            networkMs = measured ? host.NetworkMilliseconds : 0,
            commandMs = measured ? host.CommandMilliseconds : 0,
            eos = Idas3StutterMeasurements.Read(Time.frameCount),
            gc0 = GC.CollectionCount(0), gc1 = GC.CollectionCount(1), gc2 = GC.CollectionCount(2)
        };
        next = (next + 1) % Capacity; count = Math.Min(count + 1, Capacity); ++observed;
    }
    private void OnApplicationQuit() => Save();
    private void OnDestroy() => Save();
    private void Save() {
        if (saved || rows == null) return;
        saved = true; Idas3StutterMeasurements.Enabled = false;
        // One bounded write on exit, never during a measured gameplay frame.
        // A hard process kill cannot guarantee a report; normal Quit can.
        try {
            Directory.CreateDirectory(folder);
            string path = Path.Combine(folder, "stutter-" + DateTime.UtcNow.ToString("yyyyMMdd-HHmmss", CultureInfo.InvariantCulture) + "-" + Guid.NewGuid().ToString("N").Substring(0, 8) + ".csv");
            using (var output = new StreamWriter(path, false, new UTF8Encoding(false))) {
                output.WriteLine("# schema=1; local timings only; retained=" + count + "; observed=" + observed);
                output.WriteLine("# precedingFrameMs is Unity's previous-frame interval; other timings belong to unityFrame. EOS callbacks can include session work: columns overlap and must not be summed. No GPU-completion measurement.");
                output.WriteLine("unityFrame,hostMeasuredFrame,elapsedSeconds,precedingFrameMs,ready,focused,stage,phase,course,flags,simulationTicks,nativeMs,sceneSubmissionMs,uiSubmissionMs,sessionNetworkMs,commandMs,eosSdkTicks,eosSdkMs,eosCallbacks,eosCallbackMs,gc0,gc1,gc2");
                int start = count == Capacity ? next : 0;
                for (int i = 0; i < count; ++i) {
                    var r = rows[(start + i) % Capacity];
                    output.WriteLine(string.Format(CultureInfo.InvariantCulture,
                        "{0},{1},{2:R},{3:R},{4},{5},{6},{7},{8},{9},{10},{11:R},{12:R},{13:R},{14:R},{15:R},{16},{17:R},{18},{19:R},{20},{21},{22}",
                        r.frame,r.hostFrame,r.seconds,r.precedingFrameMs,r.ready?1:0,r.focused?1:0,r.stage,r.phase,r.course,r.flags,r.ticks,
                        r.nativeMs,r.sceneMs,r.uiMs,r.networkMs,r.commandMs,r.eos.SdkTicks,r.eos.SdkMilliseconds,r.eos.Callbacks,r.eos.CallbackMilliseconds,r.gc0,r.gc1,r.gc2));
                }
            }
            Debug.Log("Stutter trace saved: " + path);
        } catch (Exception error) { Debug.LogWarning("Stutter trace could not be saved: " + error.GetType().Name); }
        finally { rows = null; }
    }
}
