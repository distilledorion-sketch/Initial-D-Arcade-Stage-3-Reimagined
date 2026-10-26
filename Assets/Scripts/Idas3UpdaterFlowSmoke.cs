using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using UnityEngine;

// Only the isolated diagnostic scene instantiates this component. It uses the
// real Unity downloader, Mono staging, cache lease, native handoff and shutdown.
public sealed class Idas3UpdaterFlowSmoke : MonoBehaviour
{
    [Serializable] private sealed class Config {
        public string gameRoot,url,sha256,patchUrl,patchSha256,contentUrl,contentSha256,installedVersion,targetVersion;
        public long bytes,patchBytes,contentBytes;
        public int timeoutSeconds;
    }
    [Serializable] private sealed class Observation {public string state,message,session;public float elapsed;}
    [Serializable] private sealed class Report {
        public bool reachedShutdown;
        public string error,tempPath,cacheRoot,applicationVersion,installedVersion,targetVersion;
        public Observation[] observations;
    }
    private readonly List<Observation> observations=new List<Observation>();
    private Idas3Updates updates;
    private string reportPath,lastMessage,error;
    private float began;
    private int timeoutSeconds=120;
    private bool stopped;
    private static FieldInfo Field(string name)=>typeof(Idas3Updates).GetField(name,BindingFlags.Instance|BindingFlags.NonPublic);
    private static void RequireLoopback(string url){
        Uri uri;
        if(!Uri.TryCreate(url,UriKind.Absolute,out uri)||!uri.IsLoopback||
            (uri.Scheme!=Uri.UriSchemeHttp&&uri.Scheme!=Uri.UriSchemeHttps))
            throw new IOException("Updater fixture transport must be loopback HTTP.");
    }
    private void Start(){StartCoroutine(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-updater-error-ui-smoke")>=0?RenderFailureUI():Run());}
    [Serializable] private sealed class UIReport {public bool passed,clipboardVerified;public string[] screenshots;public string details;}
    private IEnumerator RenderFailureUI(){
        var args=Environment.GetCommandLineArgs();int at=Array.IndexOf(args,"-idas3-updater-error-ui-smoke");
        string folder=Path.GetFullPath(args[at+1]);
        if(!File.Exists(Path.Combine(folder,"ISOLATED_UPDATE_TEST.txt"))||File.Exists(Path.Combine(folder,"ui-report.json")))
            throw new IOException("Use a fresh isolated updater UI fixture.");
        updates=gameObject.AddComponent<Idas3Updates>();updates.Initialize(false);
        var captures=new List<string>();bool clipboardVerified=false;
        string longPath=@"C:\Users\Example\AppData\Local\Temp\InitialDUpdates\0123456789abcdef0123456789abcdef";
        string native="ApplicationName='"+longPath+"\\install.exe', CommandLine='\""+longPath+"\\install.plan\"', CurrentDirectory='"+longPath+"', Native error= Access is denied.";
        foreach(int language in new[]{0,1,2})foreach(var size in new[]{new Vector2Int(640,480),new Vector2Int(1280,720)}){
            Screen.SetResolution(size.x,size.y,false);yield return new WaitForSecondsRealtime(.5f);
            Idas3MenuLocalization.SetLanguage(language);
            updates.ApplyResponse(200,Idas3UpdateChecks.Fixture("v99.0.0"));updates.Activate();
            updates.SetInstallFailure(new Idas3Updates.InstallerLaunchException(new System.ComponentModel.Win32Exception(5,native)));
            yield return null;yield return new WaitForEndOfFrame();
            string name="error-"+language+"-"+Screen.width+"x"+Screen.height+".png";
            var picture=ScreenCapture.CaptureScreenshotAsTexture();File.WriteAllBytes(Path.Combine(folder,name),picture.EncodeToPNG());Destroy(picture);captures.Add(name);
            if(!clipboardVerified){
                string previous=GUIUtility.systemCopyBuffer;
                try{
                    updates.HandleWindowInput(false,true,false,false);updates.HandleWindowInput(false,false,true,false);
                    yield return null;
                    clipboardVerified=(GUIUtility.systemCopyBuffer??"").Replace("\r\n","\n")==updates.FailureDetails.Replace("\r\n","\n");
                }finally{GUIUtility.systemCopyBuffer=previous;}
                if(!clipboardVerified){
                    File.WriteAllText(Path.Combine(folder,"ui-report.json"),JsonUtility.ToJson(new UIReport{passed=false,screenshots=captures.ToArray(),details="Player clipboard did not preserve copied error details."},true));
                    Application.Quit(1);yield break;
                }
            }
            updates.ContinueToGame();
        }
        File.WriteAllText(Path.Combine(folder,"ui-report.json"),JsonUtility.ToJson(new UIReport{passed=true,clipboardVerified=clipboardVerified,screenshots=captures.ToArray(),details="Synthetic Windows error 5 used only to check rendering; not a diagnosis of the reported user's launch failure."},true));
        Application.Quit();
    }
    private IEnumerator Run()
    {
        try{
            var args=Environment.GetCommandLineArgs();int at=Array.IndexOf(args,"-idas3-updater-flow-smoke");
            if(at<0||at+1>=args.Length)throw new ArgumentException("Updater fixture configuration missing.");
            string configPath=Path.GetFullPath(args[at+1]);reportPath=Path.Combine(Path.GetDirectoryName(configPath),"unity-flow.json");
            if(File.Exists(reportPath))throw new IOException("Do not overwrite updater flow evidence.");
            var config=JsonUtility.FromJson<Config>(File.ReadAllText(configPath));
            string folder=Path.GetDirectoryName(configPath);
            if(config==null||!File.Exists(Path.Combine(folder,"ISOLATED_UPDATE_TEST.txt"))||
                !string.Equals(Path.GetDirectoryName(Path.GetFullPath(config.gameRoot)),folder,StringComparison.OrdinalIgnoreCase))
                throw new IOException("Updater game must be a direct child of its marked disposable fixture.");
            Idas3UpdateStaging.NoLinks(folder);Idas3UpdateStaging.NoLinks(config.gameRoot);
            RequireLoopback(config.url);
            if(!string.IsNullOrEmpty(config.patchUrl))RequireLoopback(config.patchUrl);
            if(!string.IsNullOrEmpty(config.contentUrl))RequireLoopback(config.contentUrl);
            timeoutSeconds=config.timeoutSeconds==0?120:config.timeoutSeconds;
            if(timeoutSeconds<30||timeoutSeconds>3600)throw new IOException("Updater fixture timeout must be 30 to 3600 seconds.");
            updates=gameObject.AddComponent<Idas3Updates>();updates.Initialize(false);
            // Explicitly recorded simulation of the old installed version. This
            // does not claim that the diagnostic executable is the old player.
            if(!string.IsNullOrEmpty(config.installedVersion))
                typeof(Idas3Updates).GetProperty("InstalledVersion").SetValue(updates,config.installedVersion);
            string target=string.IsNullOrEmpty(config.targetVersion)?"99.0.0":config.targetVersion;
            int order;
            if(!Idas3Updates.TryCompareVersions(target,updates.InstalledVersion,out order)||order<=0)
                throw new IOException("Fixture target must be newer than its installed version.");
            var release=JsonUtility.FromJson<Idas3Updates.Release>(Idas3UpdateChecks.Fixture("v"+target));
            release.assets[0].size=config.bytes;release.assets[0].digest="sha256:"+config.sha256;
            var assets=new List<Idas3Updates.Asset>(release.assets);
            if(!string.IsNullOrEmpty(config.patchUrl)){
                string name="Initial-D-Update-from-"+updates.InstalledVersion+"-to-"+target+"-Patch.zip";
                assets.Add(new Idas3Updates.Asset{name=name,state="uploaded",size=config.patchBytes,
                    digest="sha256:"+config.patchSha256,browser_download_url=Idas3Updates.RepositoryUrl+"/releases/download/v"+target+"/"+name});
            }
            if(!string.IsNullOrEmpty(config.contentUrl)){
                string name="Initial-D-Additional-Courses-"+target+".zip";
                assets.Add(new Idas3Updates.Asset{name=name,state="uploaded",size=config.contentBytes,
                    digest="sha256:"+config.contentSha256,browser_download_url=Idas3Updates.RepositoryUrl+"/releases/download/v"+target+"/"+name});
            }
            release.assets=assets.ToArray();
            updates.ApplyResponse(200,JsonUtility.ToJson(release));
            if(updates.State!=Idas3Updates.CheckState.Available)throw new IOException("Fixture release was not accepted as an available update.");
            // Only the private fixture transport is redirected. Release URL
            // validation stays unchanged for the production client.
            Field("fullUrl").SetValue(updates,config.url);
            if(!string.IsNullOrEmpty(config.patchUrl))Field("patchUrl").SetValue(updates,config.patchUrl);
            if(!string.IsNullOrEmpty(config.contentUrl)){
                if(Field("contentUrl").GetValue(updates)==null)throw new IOException("Fixture content asset was not accepted.");
                Field("contentUrl").SetValue(updates,config.contentUrl);
            }
            updates.DownloadOverride=patch=>updates.DownloadAndInstall(config.gameRoot);
            began=Time.realtimeSinceStartup;updates.AcceptUpdate();
        }catch(Exception e){error=e.ToString();stopped=true;Save(false);Application.Quit(1);yield break;}
        while(!stopped){
            Observe();
            if(updates.State==Idas3Updates.CheckState.Unavailable){error=string.IsNullOrEmpty(updates.FailureDetails)?updates.Message:updates.FailureDetails;stopped=true;Save(false);Application.Quit(1);yield break;}
            if(Time.realtimeSinceStartup-began>timeoutSeconds){error="Updater flow exceeded its "+timeoutSeconds+" second limit.";stopped=true;Save(false);Application.Quit(1);yield break;}
            yield return null;
        }
    }
    private void Observe()
    {
        if(updates==null||updates.Message==lastMessage)return;
        lastMessage=updates.Message;
        observations.Add(new Observation{state=updates.State.ToString(),message=lastMessage,session=Field("sessionFolder").GetValue(updates) as string,elapsed=Time.realtimeSinceStartup-began});
        Save(false);
    }
    private void Save(bool shutdown)
    {
        if(reportPath==null)return;
        File.WriteAllText(reportPath,JsonUtility.ToJson(new Report{reachedShutdown=shutdown,error=error,tempPath=Path.GetTempPath(),cacheRoot=Idas3UpdateCache.Root,
            applicationVersion=Application.version,installedVersion=updates?.InstalledVersion,targetVersion=updates?.AvailableVersion,observations=observations.ToArray()},true));
    }
    private void OnApplicationQuit(){Observe();Save(error==null&&updates!=null&&updates.Message=="Applying update and restarting…");}
}
