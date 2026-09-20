using System;
using System.Collections;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;
using System.Security.Cryptography;
using System.Threading.Tasks;
using UnityEngine.InputSystem;
using UnityEngine;
using UnityEngine.Networking;

// Startup gate and explicit opt-in updater. A separate bundled helper waits for
// this process to exit before replacing files, with a rollback copy of each one.
public sealed class Idas3Updates : MonoBehaviour
{
    public static Idas3Updates Instance {get;private set;}
    public static bool StartupFinished {get;private set;}=true;
    public const string RepositoryUrl="https://github.com/distilledorion-sketch/Initial-D-Arcade-Stage-3-Reimagined";
    public const string LatestReleaseUrl=RepositoryUrl+"/releases/latest";
    public const string ApiUrl="https://api.github.com/repos/distilledorion-sketch/Initial-D-Arcade-Stage-3-Reimagined/releases/latest";
    internal const int MaximumResponseBytes=256*1024;
    public enum CheckState {Idle,Checking,Current,Available,Unavailable,Downloading,Preparing}
    public CheckState State {get;private set;}
    public string InstalledVersion {get;private set;}
    public string AvailableVersion {get;private set;}
    public string Message {get;private set;}="Check GitHub for the latest Windows release.";
    public string ReleaseUrl {get;private set;}
    public bool CanCheck=>!Busy&&Time.realtimeSinceStartupAsDouble>=nextCheck;
    public bool CanActivate=>State==CheckState.Available||CanCheck;
    public string ButtonLabel=>Busy?"PLEASE WAIT…":State==CheckState.Available?"INSTALL UPDATE":"CHECK FOR UPDATES";
    public bool WindowVisible {get;private set;}
    private bool Busy=>State==CheckState.Checking||State==CheckState.Downloading||State==CheckState.Preparing;
    private double nextCheck;
    private UnityWebRequest activeRequest;
    private string downloadUrl,downloadHash,sessionFolder;
    private long downloadBytes;
    private bool startupWindow,cancelled,yesSelected,previousCursor;
    private int windowOpenedFrame;
    private CursorLockMode previousLock;
    private GUIStyle windowTitle,windowText,windowButton;
    private System.Diagnostics.Process installer;
    internal Action InstallOverride;
    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
    private static void Bootstrap(){
        Instance=null;StartupFinished=true;
        if(Application.isEditor)return;
        bool startupTest=Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-updates-startup-check")>=0&&Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-attract-options-smoke")>=0;
        foreach(string arg in Environment.GetCommandLineArgs())if(!startupTest&&(arg.StartsWith("-idas3-",StringComparison.Ordinal)||arg.StartsWith("-hakone-",StringComparison.Ordinal)))return;
        var go=new GameObject("Game updates");DontDestroyOnLoad(go);
        Instance=go.AddComponent<Idas3Updates>();Instance.Initialize(true);
    }

    [Serializable] internal sealed class Release {
        public string tag_name,html_url;public bool draft,prerelease;public Asset[] assets;
    }
    [Serializable] internal sealed class Asset {
        public string name,state,browser_download_url,digest;public long size;
    }
    internal struct Result {
        public CheckState state;public string version,url,message,downloadUrl,hash;public long bytes;
    }

    public void Initialize(bool checkOnStartup=true){
        InstalledVersion=Application.version;
        if(checkOnStartup){StartupFinished=false;startupWindow=true;ShowWindow();CheckNow();}
    }
    public void Activate(){
        if(State==CheckState.Available)ShowWindow();
        else if(CanCheck){ShowWindow();CheckNow();}
    }
    private void ShowWindow(){
        if(WindowVisible)return;
        previousCursor=Cursor.visible;previousLock=Cursor.lockState;Cursor.visible=true;Cursor.lockState=CursorLockMode.None;
        WindowVisible=true;yesSelected=false;windowOpenedFrame=Time.frameCount;
    }
    internal void ContinueToGame(){
        WindowVisible=false;startupWindow=false;StartupFinished=true;
        Cursor.visible=previousCursor;Cursor.lockState=previousLock;
    }
    public void CheckNow(){
        if(!CanCheck)return;
        nextCheck=Time.realtimeSinceStartupAsDouble+60;
        State=CheckState.Checking;Message="Checking GitHub for updates…";
        StartCoroutine(FetchLatest());
    }
    private IEnumerator FetchLatest(){
        using(var request=new UnityWebRequest(ApiUrl,"GET")){
            activeRequest=request;
            var response=new LimitedResponse();request.downloadHandler=response;
            request.timeout=8;request.redirectLimit=0;
            request.SetRequestHeader("Accept","application/vnd.github+json");
            request.SetRequestHeader("X-GitHub-Api-Version","2026-03-10");
            request.SetRequestHeader("User-Agent","Initial-D-Arcade-Stage-3-Reimagined");
            UnityWebRequestAsyncOperation operation=null;
            try{operation=request.SendWebRequest();}catch(Exception){/* Network errors never affect gameplay. */}
            if(operation!=null)yield return operation;
            ApplyResponse(request.responseCode,response.Text,operation==null||request.result!=UnityWebRequest.Result.Success||response.Exceeded);
            activeRequest=null;
            if(State==CheckState.Current)ContinueToGame();
            else if(State==CheckState.Unavailable&&startupWindow){yield return new WaitForSecondsRealtime(1.5f);ContinueToGame();}
        }
    }
    internal void ApplyResponse(long code,string json,bool failed=false){
        var result=Evaluate(InstalledVersion,code,json,failed);
        State=result.state;AvailableVersion=result.version;ReleaseUrl=result.url;Message=result.message;
        downloadUrl=result.downloadUrl;downloadHash=result.hash;downloadBytes=result.bytes;
    }
    internal static Result Evaluate(string installed,long code,string json,bool failed=false){
        var unavailable=new Result{state=CheckState.Unavailable,message="Could not check GitHub. You can keep playing; try again in a minute."};
        if(code==403||code==429){unavailable.message="GitHub is limiting update checks. Try again later.";return unavailable;}
        if(code==404){unavailable.message="No public Windows release is available yet.";return unavailable;}
        if(failed||code!=200||string.IsNullOrWhiteSpace(json)||Encoding.UTF8.GetByteCount(json)>MaximumResponseBytes)return unavailable;
        Release release;
        try{release=JsonUtility.FromJson<Release>(json);}catch(Exception){return unavailable;}
        if(release==null||release.draft||release.prerelease||!TryCompareVersions(release.tag_name,installed,out int order))return unavailable;
        string url=RepositoryUrl+"/releases/tag/"+Uri.EscapeDataString(release.tag_name);
        if(!string.Equals(release.html_url,url,StringComparison.Ordinal))return unavailable;
        Asset windows=null;
        if(release.assets!=null)foreach(var asset in release.assets){
            if(asset==null||asset.state!="uploaded"||asset.size<=0||asset.name==null||
                !asset.name.StartsWith("Initial-D-Arcade-Stage-3-Reimagined-",StringComparison.Ordinal)||
                !asset.name.EndsWith("-Windows-x64.zip",StringComparison.Ordinal))continue;
            string expected=RepositoryUrl+"/releases/download/"+Uri.EscapeDataString(release.tag_name)+"/"+Uri.EscapeDataString(asset.name);
            if(string.Equals(asset.browser_download_url,expected,StringComparison.Ordinal)&&asset.digest!=null&&Regex.IsMatch(asset.digest,@"\Asha256:[0-9a-fA-F]{64}\z")){windows=asset;break;}
        }
        if(windows==null){unavailable.message="The latest release does not have a verified Windows download yet. Try again later.";return unavailable;}
        string version=release.tag_name.TrimStart('v');
        return new Result{state=order>0?CheckState.Available:CheckState.Current,version=version,url=url,
            downloadUrl=windows.browser_download_url,hash=windows.digest.Substring(7).ToLowerInvariant(),bytes=windows.size,
            message=order>0?"Version "+version+" is available. Download and install it now?":
                order==0?"You have the latest public Windows release.":"Your installed build is newer than the latest public Windows release."};
    }

    // SemVer compares numeric components numerically (including .9 -> .10),
    // ignores build metadata, and places stable releases after prereleases.
    internal static bool TryCompareVersions(string left,string right,out int order){
        order=0;
        if(!ParseVersion(left,out string[] a,out string[] ap)||!ParseVersion(right,out string[] b,out string[] bp))return false;
        for(int i=0;i<3;i++){order=NumericCompare(a[i],b[i]);if(order!=0)return true;}
        if(ap==null||bp==null){order=ap==bp?0:ap==null?1:-1;return true;}
        for(int i=0;i<Math.Min(ap.Length,bp.Length);i++){
            bool an=IsNumeric(ap[i]),bn=IsNumeric(bp[i]);
            order=an&&bn?NumericCompare(ap[i],bp[i]):an!=bn?(an?-1:1):string.CompareOrdinal(ap[i],bp[i]);
            if(order!=0)return true;
        }
        order=ap.Length.CompareTo(bp.Length);return true;
    }
    private static bool ParseVersion(string value,out string[] core,out string[] pre){
        core=null;pre=null;if(string.IsNullOrEmpty(value)||value.Length>96)return false;
        var match=Regex.Match(value,@"\Av?(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?(?:\+[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?\z");
        if(!match.Success)return false;
        core=new[]{match.Groups[1].Value,match.Groups[2].Value,match.Groups[3].Value};
        if(match.Groups[4].Success){pre=match.Groups[4].Value.Split('.');foreach(var part in pre)if(IsNumeric(part)&&part.Length>1&&part[0]=='0')return false;}
        return true;
    }
    private static bool IsNumeric(string value){foreach(char c in value)if(c<'0'||c>'9')return false;return true;}
    private static int NumericCompare(string a,string b)=>a.Length==b.Length?string.CompareOrdinal(a,b):a.Length.CompareTo(b.Length);

    [Serializable] private sealed class InstallPlan {
        public string installRoot,archive,sha256,parentStartTicks;public int parentId;
    }
    internal void AcceptUpdate(){
        if(State!=CheckState.Available)return;
        if(InstallOverride!=null){InstallOverride();return;}
        cancelled=false;StartCoroutine(GuardInstall(DownloadAndInstall()));
    }
    private IEnumerator GuardInstall(IEnumerator routine){
        while(true){
            object next=null;Exception failure=null;bool more=false;
            try{more=routine.MoveNext();if(more)next=routine.Current;}catch(Exception error){failure=error;}
            if(failure!=null){
                State=CheckState.Unavailable;Message="The update could not be applied. Your current game is unchanged. "+failure.Message;
                yield break;
            }
            if(!more)yield break;
            yield return next;
        }
    }
    private IEnumerator DownloadAndInstall(){
        string root=Path.GetFullPath(Path.Combine(Application.dataPath,".."));
        if(Application.isEditor||!File.Exists(Path.Combine(root,"InitialDUnity.exe")))throw new IOException("Run the Windows game to install updates.");
        // Check access before downloading. Never request elevation or alter ACLs.
        string probe=Path.Combine(root,".update-write-check-"+Guid.NewGuid().ToString("N"));
        using(var stream=new FileStream(probe,FileMode.CreateNew,FileAccess.Write,FileShare.None,1,FileOptions.DeleteOnClose)){}
        var script=Resources.Load<TextAsset>("UpdateInstaller.ps1");
        if(script==null)throw new IOException("The bundled update installer is missing.");
        sessionFolder=Path.Combine(Path.GetTempPath(),"InitialDUpdates",Guid.NewGuid().ToString("N"));Directory.CreateDirectory(sessionFolder);
        string archive=Path.Combine(sessionFolder,"game.zip");
        if(new DriveInfo(Path.GetPathRoot(sessionFolder)).AvailableFreeSpace<downloadBytes+64L*1024*1024)throw new IOException("Not enough disk space for the update download.");
        State=CheckState.Downloading;Message="Downloading update…";
        using(var request=UnityWebRequest.Get(downloadUrl)){
            activeRequest=request;request.downloadHandler=new DownloadHandlerFile(archive){removeFileOnAbort=true};
            request.redirectLimit=5;request.timeout=0;
            var operation=request.SendWebRequest();ulong previousBytes=0;double lastProgress=Time.realtimeSinceStartupAsDouble;
            while(!operation.isDone){
                if(cancelled){request.Abort();break;}
                if(request.downloadedBytes!=previousBytes){previousBytes=request.downloadedBytes;lastProgress=Time.realtimeSinceStartupAsDouble;}
                if(request.downloadedBytes>(ulong)downloadBytes||Time.realtimeSinceStartupAsDouble-lastProgress>45){request.Abort();break;}
                yield return null;
            }
            activeRequest=null;
            if(cancelled){ContinueToGame();State=CheckState.Available;yield break;}
            if(request.result!=UnityWebRequest.Result.Success)throw new IOException("Download interrupted. Please try again.");
        }
        State=CheckState.Preparing;Message="Verifying the downloaded update…";
        var verification=Task.Run(()=>{
            if(new FileInfo(archive).Length!=downloadBytes)return false;
            using(var hash=SHA256.Create())using(var input=File.OpenRead(archive))
                return string.Equals(BitConverter.ToString(hash.ComputeHash(input)).Replace("-","").ToLowerInvariant(),downloadHash,StringComparison.Ordinal);
        });
        while(!verification.IsCompleted)yield return null;
        if(!verification.GetAwaiter().GetResult())throw new IOException("Download verification failed. Nothing was installed.");
        string helper=Path.Combine(sessionFolder,"install.ps1"),planPath=Path.Combine(sessionFolder,"install.json");
        File.WriteAllText(helper,script.text,new UTF8Encoding(true));
        using(var current=System.Diagnostics.Process.GetCurrentProcess()){
            File.WriteAllText(planPath,JsonUtility.ToJson(new InstallPlan{installRoot=root,archive=archive,sha256=downloadHash,
                parentId=current.Id,parentStartTicks=current.StartTime.ToUniversalTime().Ticks.ToString()},true));
        }
        var start=new System.Diagnostics.ProcessStartInfo(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.System),"WindowsPowerShell/v1.0/powershell.exe")){
            Arguments="-NoProfile -NonInteractive -ExecutionPolicy Bypass -File "+Quote(helper)+" -ManifestPath "+Quote(planPath),
            UseShellExecute=false,CreateNoWindow=true,WorkingDirectory=sessionFolder
        };
        // PowerShell 5 needs its own default module path, even when the game was
        // launched by PowerShell 7 during development.
        start.EnvironmentVariables.Remove("PSModulePath");
        installer=System.Diagnostics.Process.Start(start);
        Message="Preparing the update. The game will restart automatically…";
        while(!File.Exists(Path.Combine(sessionFolder,"ready"))){
            if(installer.HasExited){
                string error=Path.Combine(sessionFolder,"error.txt");
                throw new IOException(File.Exists(error)?File.ReadAllText(error):"The installer could not start.");
            }
            yield return null;
        }
        Message="Applying update and restarting…";
        // Normal Unity shutdown flushes pending replays and saves. The helper
        // waits for this exact process to exit before changing any game files.
        Application.Quit();
    }
    private static string Quote(string value)=>"\""+value.Replace("\"","\\\"")+"\"";
    private void Update(){
        if(!WindowVisible||Time.frameCount<=windowOpenedFrame||!Application.isFocused)return;
        var keyboard=Keyboard.current;var pad=Gamepad.current;
        bool left=(keyboard?.leftArrowKey.wasPressedThisFrame??false)||(pad?.dpad.left.wasPressedThisFrame??false);
        bool right=(keyboard?.rightArrowKey.wasPressedThisFrame??false)||(pad?.dpad.right.wasPressedThisFrame??false);
        bool confirm=(keyboard?.enterKey.wasPressedThisFrame??false)||(pad?.buttonSouth.wasPressedThisFrame??false)||(pad?.startButton.wasPressedThisFrame??false);
        bool back=(keyboard?.escapeKey.wasPressedThisFrame??false)||(pad?.buttonEast.wasPressedThisFrame??false);
        HandleWindowInput(left,right,confirm,back);
    }
    internal void HandleWindowInput(bool left,bool right,bool confirm,bool back){
        if(State==CheckState.Available){
            if(left)yesSelected=true;if(right)yesSelected=false;
            if(back||confirm&&!yesSelected)ContinueToGame();else if(confirm)AcceptUpdate();
        }else if(State==CheckState.Downloading&&back)cancelled=true;
        else if(State==CheckState.Unavailable&&(back||confirm))ContinueToGame();
    }
    private void OnGUI(){
        if(!WindowVisible)return;
        if(Event.current.type==EventType.KeyDown||Event.current.type==EventType.KeyUp)Event.current.Use();
        if(windowTitle==null){
            windowTitle=new GUIStyle(GUI.skin.label){fontSize=27,fontStyle=FontStyle.Bold,alignment=TextAnchor.MiddleCenter};
            windowText=new GUIStyle(GUI.skin.label){fontSize=17,wordWrap=true,alignment=TextAnchor.MiddleCenter};
            windowButton=new GUIStyle(GUI.skin.button){fontSize=20,fontStyle=FontStyle.Bold};
        }
        GUI.depth=-32000;var matrix=GUI.matrix;var color=GUI.color;
        GUI.color=new Color(0,0,0,.96f);GUI.DrawTexture(new Rect(0,0,Screen.width,Screen.height),Texture2D.whiteTexture);GUI.color=Color.white;
        float scale=Mathf.Min(1.5f,Mathf.Min(Screen.width/680f,Screen.height/390f));
        GUI.matrix=Matrix4x4.TRS(new Vector3((Screen.width-640*scale)/2,(Screen.height-330*scale)/2,0),Quaternion.identity,new Vector3(scale,scale,1));
        GUI.Box(new Rect(0,0,640,330),GUIContent.none);
        GUI.Label(new Rect(20,18,600,50),State==CheckState.Checking?"CHECKING FOR UPDATES":State==CheckState.Available?"UPDATE AVAILABLE":State==CheckState.Downloading?"DOWNLOADING UPDATE":State==CheckState.Preparing?"INSTALLING UPDATE":"UPDATE CHECK",windowTitle);
        GUI.Label(new Rect(30,76,580,80),Message,windowText);
        if(State==CheckState.Available){
            GUI.Label(new Rect(30,157,580,58),"The game will close, install the update, and restart.\nYour saves, settings, music and replays will be kept.",windowText);
            GUI.color=yesSelected?new Color(1,.85f,.3f):Color.white;
            if(GUI.Button(new Rect(110,240,190,52),"YES",windowButton))AcceptUpdate();
            GUI.color=!yesSelected?new Color(1,.85f,.3f):Color.white;
            if(GUI.Button(new Rect(340,240,190,52),"NO",windowButton))ContinueToGame();GUI.color=Color.white;
        }else if(State==CheckState.Downloading){
            float progress=activeRequest==null?0:Mathf.Clamp01((float)(activeRequest.downloadedBytes/(double)downloadBytes));
            GUI.Box(new Rect(60,171,520,25),GUIContent.none);GUI.DrawTexture(new Rect(64,175,512*progress,17),Texture2D.whiteTexture);
            GUI.Label(new Rect(60,204,520,30),(progress*100).ToString("0")+"%  /  "+(downloadBytes/1048576d).ToString("0")+" MB",windowText);
            if(GUI.Button(new Rect(220,256,200,45),"CANCEL",windowButton))cancelled=true;
        }else if(State==CheckState.Unavailable){if(GUI.Button(new Rect(200,245,240,48),"CONTINUE TO GAME",windowButton))ContinueToGame();}
        GUI.matrix=matrix;GUI.color=color;
    }

    private sealed class LimitedResponse : DownloadHandlerScript {
        private readonly MemoryStream body=new MemoryStream();
        internal bool Exceeded {get;private set;}
        internal string Text=>Exceeded?"":Encoding.UTF8.GetString(body.GetBuffer(),0,(int)body.Length);
        internal LimitedResponse():base(new byte[8192]){}
        protected override bool ReceiveData(byte[] data,int length){
            if(data==null||length<0||body.Length+length>MaximumResponseBytes){Exceeded=true;return false;}
            body.Write(data,0,length);return true;
        }
        public override void Dispose(){body.Dispose();base.Dispose();}
    }
    private void OnDestroy(){StopAllCoroutines();if(activeRequest!=null){activeRequest.Abort();activeRequest.Dispose();activeRequest=null;}}
}
