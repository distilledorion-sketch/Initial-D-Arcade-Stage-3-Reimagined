using System;
using System.Reflection;
using System.Threading.Tasks;
using Epic.OnlineServices;
using Epic.OnlineServices.Platform;
using Idas3.Multiplayer;

internal static class EosPlatformDriver {
    static int checks;
    static void Check(bool condition,string reason){if(!condition)throw new Exception(reason);++checks;Console.WriteLine("PASS\t"+reason);}
    static void Pump()=>Idas3EosPlatform.Tick(true);
    static PlatformInterface Platform=>PlatformInterface.Instance;
    static Epic.OnlineServices.Connect.ConnectInterface Connect=>Platform.Connect;
    static Task Begin()=>Idas3EosPlatform.InitializeAsync(new Idas3EosOnlineConfig());
    static void Fault(Task task,string detail){Check(task.IsFaulted,"The authentication task is faulted");Check(task.Exception.GetBaseException().Message.Contains(detail),"Authentication failure preserves its actionable reason: "+detail);}
    static Task SignIn(){var task=Begin();Connect.CompleteDevice(Result.DuplicateNotAllowed);Pump();Connect.CompleteLogin(Result.Success,"user-one");Pump();Check(task.Status==TaskStatus.RanToCompletion&&Idas3EosPlatform.SignedIn,"Existing Device ID signs in without creating a replacement identity");return task;}
    static void ExistingIdentity(){
        var task=Begin();Check(!task.IsCompleted&&!Idas3EosPlatform.SignedIn&&Connect.Creates==1,"Initialization waits for Device ID completion");
        Check(object.ReferenceEquals(task,Begin())&&Connect.Creates==1&&PlatformInterface.Creations==1,"Concurrent initialization shares one platform and authentication task");
        Check(PlatformInterface.LastOptions.CacheDirectory.EndsWith("EOS"+System.IO.Path.DirectorySeparatorChar+"fixture"),"Separate diagnostic profile selects its own cache path");
        Check((PlatformInterface.LastOptions.Flags&(PlatformFlags.DisableOverlay|PlatformFlags.DisableSocialOverlay))==(PlatformFlags.DisableOverlay|PlatformFlags.DisableSocialOverlay),"Client initialization disables account/social overlays");
        Check(Connect.LastDevice.DeviceModel==UnityEngine.Application.platform,"Device model describes the platform and is not used as an identity override");
        Connect.CompleteDevice(Result.DuplicateNotAllowed);Check(Connect.Logins==0,"SDK callback cannot advance authentication before normal callback dispatch");Pump();
        Check(Connect.Logins==1&&Connect.LastLogin.Credentials.Type==ExternalCredentialType.DeviceidAccessToken,"Existing Device ID uses the automatic Connect credential type");
        Connect.CompleteLogin(Result.Success,"user-one");Pump();Check(task.Status==TaskStatus.RanToCompletion&&Idas3EosPlatform.SignedIn&&Connect.Users==0,"Existing product user completes without CreateUser or an interactive Epic login");
    }
    static void NewIdentity(){
        var task=Begin();Connect.CompleteDevice(Result.Success);Pump();var token=new ContinuanceToken();Connect.CompleteLogin(Result.InvalidUser,null,token);Pump();
        Check(Connect.Users==1&&ReferenceEquals(Connect.LastUser.ContinuanceToken,token)&&!task.IsCompleted,"Only InvalidUser with its SDK continuation token creates a product user");
        Connect.CompleteUser(Result.Success,"new-user");Pump();Check(task.Status==TaskStatus.RanToCompletion&&Idas3EosPlatform.User.ToString()=="new-user"&&Idas3EosPlatform.SignedIn,"New Device ID completes authenticated product-user creation");
    }
    static void NativeLoad(){
        var task=Begin();Platform.NativeEvents.Enqueue(()=>Connect.CompleteDevice(Result.DuplicateNotAllowed));Idas3EosPlatform.Tick(false);
        Check(Platform.Ticks==1&&Connect.Logins==0&&!task.IsCompleted,"Loading pump services SDK Tick without dispatching the Device ID callback");Pump();
        Platform.NativeEvents.Enqueue(()=>Connect.CompleteLogin(Result.Success,"user-one"));Idas3EosPlatform.Tick(false);
        Check(Idas3EosPlatform.User==null&&!task.IsCompleted,"Loading pump defers identity mutation until native loading releases its lock");Pump();
        Check(Idas3EosPlatform.SignedIn&&task.Status==TaskStatus.RanToCompletion,"Deferred authentication completes on the next normal frame");
        int delivered=0;Idas3EosPlatform.Defer(()=>{++delivered;Idas3EosPlatform.Tick(true);});Idas3EosPlatform.Defer(()=>++delivered);Pump();
        Check(delivered==2,"Reentrant callback pumping delivers queued work exactly once");
    }
    static void FailureRetry(){
        var task=Begin();Connect.CompleteDevice(Result.AccessDenied);Pump();Fault(task,"AccessDenied");
        Check(Connect.Logins==0,"Device-ID authorization failure cannot attempt Login");
        var retry=Begin();Check(!ReferenceEquals(task,retry)&&Connect.Creates==2&&PlatformInterface.Creations==1,"A failed authentication retries on the same platform");
        Connect.CompleteDevice(Result.DuplicateNotAllowed);Pump();Connect.CompleteLogin(Result.ServiceFailure);Pump();Fault(retry,"ServiceFailure");
        Check(Connect.Users==0,"An ordinary sign-in error never creates a different identity");
        var success=Begin();Connect.CompleteDevice(Result.DuplicateNotAllowed);Pump();Connect.CompleteLogin(Result.Success,"user-one");Pump();Check(success.Status==TaskStatus.RanToCompletion&&Idas3EosPlatform.SignedIn,"A later successful retry recovers availability");
    }
    static void InvalidUser(){var task=Begin();Connect.CompleteDevice(Result.Success);Pump();Connect.CompleteLogin(Result.InvalidUser);Pump();Fault(task,"InvalidUser");Check(Connect.Users==0,"InvalidUser without a continuation token cannot create a user");}
    static void InvalidIdentity(){var task=Begin();Connect.CompleteDevice(Result.Success);Pump();Connect.CompleteLogin(Result.Success,"");Pump();Fault(task,"invalid player identity");}
    static void Renewal(){
        SignIn();Connect.Expire();Idas3EosPlatform.Tick(false);Check(Connect.Logins==1,"Loading cannot launch token renewal from a deferred expiration callback");Pump();
        Check(Connect.Logins==2,"Normal callback dispatch starts renewal after auth expiration");Connect.CompleteLogin(Result.Success,"user-one");Pump();
        Check(Idas3EosPlatform.SignedIn&&Idas3EosPlatform.User.ToString()=="user-one"&&Connect.Creates==1&&Connect.Users==0,"Renewal keeps the authenticated identity without creating another Device ID");
        Connect.LoseLogin();Pump();Check(Connect.Logins==3&&!Idas3EosPlatform.SignedIn,"A real login-status loss triggers reauthentication and clears availability");
        Connect.CompleteLogin(Result.Success,"user-one");Pump();Check(Idas3EosPlatform.SignedIn,"Successful reauthentication restores the same identity");
    }
    static void ChangedIdentity(){
        SignIn();Connect.Expire();Pump();Connect.CompleteLogin(Result.Success,"user-two");Pump();
        Check(Idas3EosPlatform.User.ToString()=="user-one"&&!Idas3EosPlatform.SignedIn,"Renewal cannot silently replace an existing authenticated product user");
        Check(Idas3EosPlatform.Failure!=null&&Idas3EosPlatform.Failure.Contains("identity changed")&&Idas3EosPlatform.Failure.Contains("Restart"),"Fatal identity replacement reports the recovery action instead of silently retrying");
        var next=(double)typeof(Idas3EosPlatform).GetField("nextRenew",BindingFlags.NonPublic|BindingFlags.Static).GetValue(null);
        Check(double.IsPositiveInfinity(next),"A changed identity cannot start an endless background retry loop");
    }
    static void BoundedAndShutdown(){
        SignIn();for(int i=0;i<4097;++i)Idas3EosPlatform.Defer(()=>{});Check(Idas3EosPlatform.Failure!=null&&Idas3EosPlatform.Failure.Contains("buffer is full"),"Excessive deferred SDK work reports a bounded-buffer failure");
        var queue=typeof(Idas3EosPlatform).GetField("callbacks",BindingFlags.NonPublic|BindingFlags.Static).GetValue(null);int count=(int)queue.GetType().GetProperty("Count").GetValue(queue);
        Check(count==4096,"Deferred SDK work is bounded to 4096 callbacks");UnityEngine.Application.Quit();
        Check(Platform.Releases==1&&PlatformInterface.Shutdowns==1&&Connect.RemovedNotify==2&&Idas3EosPlatform.Handle==null,"Shutdown unregisters SDK notifications and releases the platform exactly once");
        Check(Idas3EosTransport.Shutdowns==1&&Idas3EosTransport.HandleWasLive,"Live routes are asked to close before the native platform is released");
        UnityEngine.Application.Quit();Idas3EosPlatform.Tick(true);Check(Platform.Releases==1&&PlatformInterface.Shutdowns==1,"Repeated shutdown and later frame pumps are harmless");
    }
    static int Main(string[] args){try{
        switch(args[0]){case "existing":ExistingIdentity();break;case "new":NewIdentity();break;case "load":NativeLoad();break;case "retry":FailureRetry();break;case "missing-token":InvalidUser();break;case "invalid-identity":InvalidIdentity();break;case "renew":Renewal();break;case "changed-identity":ChangedIdentity();break;case "shutdown":BoundedAndShutdown();break;default:throw new Exception("Unknown test case");}
        Console.WriteLine("TOTAL\t"+checks);return 0;
    }catch(Exception error){Console.Error.WriteLine(error);return 1;}}
}
