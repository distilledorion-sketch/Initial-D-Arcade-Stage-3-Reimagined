using System;
using System.Globalization;
using System.IO;
using System.Reflection;

namespace UnityEngine {
    public class MonoBehaviour {}
    public sealed class DefaultExecutionOrderAttribute : Attribute { public DefaultExecutionOrderAttribute(int ignored) {} }
    public static class Time { public static int frameCount; public static double realtimeSinceStartupAsDouble; public static float unscaledDeltaTime; }
    public static class Application { public static bool isFocused=true; }
    public static class Debug { public static void Log(string value){Console.WriteLine(value);} public static void LogWarning(string value){Console.WriteLine(value);} }
}
internal sealed class Idas3SceneGame {
    internal struct State { public int frontendStage,racePhase,course; public uint flags;public ulong simulationTicks; }
    public State Status;
    public bool Ready=true;
    public int PerformanceSampleFrame;
    public float NativeStepMilliseconds,RendererMilliseconds,UiMilliseconds,NetworkMilliseconds,CommandMilliseconds;
}
internal static class StutterTraceDriver {
    private static int checks;
    private static void Check(bool value,string text){if(!value)throw new Exception(text);++checks;Console.WriteLine("PASS\t"+text);}
    private static void Call(Idas3StutterTrace trace,string name)=>typeof(Idas3StutterTrace).GetMethod(name,BindingFlags.NonPublic|BindingFlags.Instance).Invoke(trace,null);
    private static int Main(string[] args){try{
        string folder=args[0];Directory.CreateDirectory(folder);
        var host=new Idas3SceneGame();var trace=new Idas3StutterTrace();
        Check(!Idas3StutterTrace.Requested,"Trace is not enabled in ordinary launches");
        CultureInfo.CurrentCulture=new CultureInfo("de-DE");trace.Initialize(host,folder);
        for(int frame=1;frame<=40001;++frame){
            UnityEngine.Time.frameCount=frame;UnityEngine.Time.realtimeSinceStartupAsDouble=frame/60.0;UnityEngine.Time.unscaledDeltaTime=1f/60;
            host.PerformanceSampleFrame=frame;host.Status=new Idas3SceneGame.State{frontendStage=1,racePhase=2,course=3,flags=17,simulationTicks=(ulong)frame};
            host.NativeStepMilliseconds=4;host.RendererMilliseconds=3;host.UiMilliseconds=2;host.NetworkMilliseconds=1;host.CommandMilliseconds=0.5f;
            if(frame==40000){Idas3StutterMeasurements.Sdk(frame,.25);Idas3StutterMeasurements.Callback(frame,.125);}
            if(frame==40001){host.Ready=false;host.PerformanceSampleFrame=40000;}
            Call(trace,"LateUpdate");
        }
        Check(Directory.GetFiles(folder).Length==0,"No trace file is written during measured frames");
        Call(trace,"OnApplicationQuit");Call(trace,"OnDestroy");
        var files=Directory.GetFiles(folder,"*.csv");Check(files.Length==1,"Quit plus destruction writes exactly one trace");
        var lines=File.ReadAllLines(files[0]);Check(lines.Length==36003&&lines[0].Contains("retained=36000; observed=40001"),"Trace memory and output remain bounded across a long session");
        Check(lines[3].StartsWith("4002,")&&lines[lines.Length-1].StartsWith("40001,"),"Ring wrap preserves the most recent frames in chronological order");
        string[] measured=lines[lines.Length-2].Split(','),empty=lines[lines.Length-1].Split(',');
        Check(measured.Length==23&&measured[16]=="1"&&measured[17]=="250"&&measured[18]=="1"&&measured[19]=="125","Report preserves independent SDK and callback measurements");
        Check(empty[4]=="0"&&empty[11]=="0"&&empty[16]=="0"&&empty[19]=="0","Inactive host and SDK frames cannot repeat previous measurements");
        Check(measured[15]=="0.5"&&double.Parse(measured[2],CultureInfo.InvariantCulture)>600,"CSV numbers are locale independent");
        Check(!Idas3StutterMeasurements.Enabled,"Trace shutdown stops measurement");
        string blocked=Path.Combine(folder,"blocked");File.WriteAllText(blocked,"fixture");
        var bad=new Idas3StutterTrace();bad.Initialize(host,blocked);Call(bad,"LateUpdate");Call(bad,"OnApplicationQuit");
        Check(!Idas3StutterMeasurements.Enabled&&File.ReadAllText(blocked)=="fixture","Unwritable report location leaves existing files and game shutdown intact");
        Console.WriteLine("TOTAL\t"+checks);return 0;
    }catch(Exception error){Console.Error.WriteLine(error);return 1;}}
}
