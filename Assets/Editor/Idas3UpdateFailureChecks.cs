using System;
using System.Collections;
using System.ComponentModel;
using System.IO;
using System.Reflection;
using UnityEngine;

// Only invalid executable fixtures are launched. No installer, game or download runs.
public static class Idas3UpdateFailureChecks {
    const BindingFlags Members=BindingFlags.Instance|BindingFlags.Static|BindingFlags.Public|BindingFlags.NonPublic;
    const string LaunchMessage="Windows could not start the update installer. Your current game has not been changed.";
    static int checks;
    static void Check(bool ok,string why){if(!ok)throw new Exception(why);++checks;}
    static object Call(Idas3Updates updater,string method,params object[] args)=>typeof(Idas3Updates).GetMethod(method,Members).Invoke(updater,args);
    static object Field(Idas3Updates updater,string name)=>typeof(Idas3Updates).GetField(name,Members).GetValue(updater);
    static void SetField(Idas3Updates updater,string name,object value)=>typeof(Idas3Updates).GetField(name,Members).SetValue(updater,value);
    static object Property(Idas3Updates updater,string name)=>typeof(Idas3Updates).GetProperty(name,Members).GetValue(updater);
    static void SetProperty(Idas3Updates updater,string name,object value)=>typeof(Idas3Updates).GetProperty(name,Members).GetSetMethod(true).Invoke(updater,new[]{value});
    sealed class FailingRoutine : IEnumerator,IDisposable {
        readonly Exception error;
        internal int Moves,Disposals;
        internal FailingRoutine(Exception error){this.error=error;}
        public object Current=>null;
        public bool MoveNext(){++Moves;throw error;}
        public void Reset(){throw new NotSupportedException();}
        public void Dispose(){++Disposals;}
    }
    static Exception LaunchFailure(string helper,string plan,string session,params int[] expectedCodes){
        Exception failure=null;
        try{Call(null,"StartInstaller",helper,plan,session);}
        catch(TargetInvocationException caught){failure=caught.InnerException;}
        Check(failure!=null&&failure.GetType().Name=="InstallerLaunchException","Native launch failure was not wrapped");
        var native=failure.InnerException as Win32Exception;
        Check(native!=null,"Original Win32 exception was not preserved");
        Check(Array.IndexOf(expectedCodes,native.NativeErrorCode)>=0,"Unexpected native launch code: "+native.NativeErrorCode);
        Check((int)failure.GetType().GetField("NativeCode",Members).GetValue(failure)==native.NativeErrorCode,"Wrapper changed the native error code");
        Check(failure.Message==LaunchMessage,"Launch summary leaked the raw process error");
        Check(failure.ToString().Contains(native.Message),"Full launch exception lost the original Windows detail");
        return failure;
    }
    static void Input(Idas3Updates updater,bool left=false,bool right=false,bool confirm=false,bool back=false)=>Call(updater,"HandleWindowInput",left,right,confirm,back);
    static void GuardFailure(Idas3Updates updater,Exception failure){
        int fallbackCalls=0;
        SetField(updater,"usingPatch",true);
        SetField(updater,"DownloadOverride",(Func<bool,IEnumerator>)(_=>{++fallbackCalls;throw new Exception("Unexpected download fallback");}));
        SetProperty(updater,"State",Idas3Updates.CheckState.Preparing);
        var routine=new FailingRoutine(failure);
        var guard=(IEnumerator)Call(updater,"GuardInstall",routine);
        bool logging=Debug.unityLogger.logEnabled;
        try{
            // GuardInstall deliberately logs the expected exception. Do not leave
            // that expected error in the console before a diagnostic player build.
            Debug.unityLogger.logEnabled=false;
            Check(!guard.MoveNext(),"Failed installer coroutine remained active");
        }finally{Debug.unityLogger.logEnabled=logging;}
        Check(!guard.MoveNext()&&routine.Moves==1&&routine.Disposals==1,"Failure retried or failed to dispose the operation");
        (guard as IDisposable)?.Dispose();
        Check(fallbackCalls==0,"Launch failure incorrectly fell back to another download");
        Check(Field(updater,"pendingInstallation")==null&&Field(updater,"activeRequest")==null,"Failure retained an active installation");
        Check(Field(updater,"installer")==null&&!(bool)Field(updater,"installerLaunched"),"Failure marked an installer as launched");
        Check(updater.State==Idas3Updates.CheckState.Unavailable&&updater.CanCheck,"Failure left the updater busy");
        Check(((string)Property(updater,"FailureDetails")).Contains(failure.ToString()),"Failure UI lost full troubleshooting details");
        Check((int)Field(updater,"actionSelected")==2,"Failure default action must continue playing");
        SetField(updater,"DownloadOverride",null);
    }
    public static void Run(){
        if(Application.platform!=RuntimePlatform.WindowsEditor)throw new PlatformNotSupportedException("These checks require Windows native process errors.");
        checks=0;
        string output=Path.GetFullPath("Verification/updater-launch-error-20261011");
        string fixture=Path.Combine(output,"launch fixture "+Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(fixture);
        string plan=Path.Combine(fixture,"plan with spaces.json"),missing=Path.Combine(fixture,"missing.exe"),invalid=Path.Combine(fixture,"invalid.exe");
        File.WriteAllText(plan,"{\"isolatedTest\":true}");
        const string invalidBytes="This is a deliberately invalid executable for an isolated updater test.";
        File.WriteAllText(invalid,invalidBytes);
        Exception missingFailure=LaunchFailure(missing,plan,fixture,2);
        Exception invalidFailure=LaunchFailure(invalid,plan,fixture,193,216);
        bool startup=Idas3Updates.StartupFinished,cursor=Cursor.visible;
        CursorLockMode cursorLock=Cursor.lockState;
        string clipboard=GUIUtility.systemCopyBuffer;
        var originalInstance=Idas3Updates.Instance;
        var go=new GameObject("Isolated updater failure checks");
        try{
            var updater=go.AddComponent<Idas3Updates>();
            updater.Initialize(false);
            SetProperty(updater,"AvailableVersion","0.3.95-community-replays.999");
            SetProperty(null,"StartupFinished",false);
            Call(updater,"ShowWindow");
            GuardFailure(updater,missingFailure);
            Check(updater.WindowVisible&&!Idas3Updates.StartupFinished,"Launch failure closed the game gate before the player chose Continue");
            Check(updater.Message.StartsWith(LaunchMessage,StringComparison.Ordinal)&&updater.Message.EndsWith(": 2",StringComparison.Ordinal),"Concise launch error omitted the native code");
            string details=(string)Property(updater,"FailureDetails");
            Check(details.Contains("Installed: "+updater.InstalledVersion)&&details.Contains("Update: "+updater.AvailableVersion)&&details.Contains("Stage: starting installer"),"Launch details omitted useful update context");
            Input(updater,right:true);Input(updater,confirm:true);
            Check((bool)Field(updater,"errorCopied")&&updater.WindowVisible,"Controller Copy Details did not stay open");
            // Batch-mode editor has no clipboard window. The diagnostic player
            // checks real clipboard contents after rendering the failure screen.
            if(!Application.isBatchMode)Check((GUIUtility.systemCopyBuffer??"").Replace("\r\n","\n")==details.Replace("\r\n","\n"),"Controller Copy Details did not preserve complete clipboard text");
            Input(updater,left:true);Input(updater,confirm:true);
            Check(!updater.WindowVisible&&Idas3Updates.StartupFinished,"Controller Continue did not return to the game");
            Call(updater,"ShowWindow");GuardFailure(updater,invalidFailure);Input(updater,back:true);
            Check(!updater.WindowVisible,"Controller Back could not dismiss an installer failure");
            var generic=new IOException("Isolated integrity failure: "+new string('x',1024));
            GuardFailure(updater,generic);
            Check(updater.Message=="The update could not be applied. Your current game is unchanged.","Generic failure did not use a concise independent summary");
            Check(!((string)Property(updater,"FailureDetails")).Contains("Stage: starting installer"),"Generic failure was mislabeled as an installer launch failure");
            SetField(updater,"failureScroll",new Vector2(0,200));SetField(updater,"errorCopied",true);
            Call(updater,"ApplyResponse",404L,"",false);
            Check((string)Property(updater,"FailureDetails")==""&&!(bool)Field(updater,"errorCopied")&&(Vector2)Field(updater,"failureScroll")==Vector2.zero,"A subsequent update response retained stale failure state");
            Check(updater.Message=="No public Windows release is available yet.","Normal update response failed after an installer error");
            Check(File.ReadAllText(invalid)==invalidBytes&&!File.Exists(missing)&&File.ReadAllText(plan)=="{\"isolatedTest\":true}","Failed launch changed its private input fixtures");
            Check(ReferenceEquals(originalInstance,Idas3Updates.Instance),"Private checks replaced the normal updater instance");
        }finally{
            UnityEngine.Object.DestroyImmediate(go);
            SetProperty(null,"StartupFinished",startup);
            Cursor.visible=cursor;Cursor.lockState=cursorLock;
            GUIUtility.systemCopyBuffer=clipboard;
        }
        File.WriteAllText(Path.Combine(output,"launch-checks-PASS.txt"),"PASS "+checks+" checks; missing helper Windows error 2; invalid executable Windows error "+((Win32Exception)invalidFailure.InnerException).NativeErrorCode+".\nFixture: "+fixture+"\nNo game, installer, network or fallback launch performed.\n");
        Debug.Log("UPDATE_FAILURE_CHECKS_PASS "+checks);
    }
}
