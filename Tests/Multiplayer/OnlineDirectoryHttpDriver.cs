using System;
using System.Collections.Generic;
using System.Text;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using Epic.OnlineServices;
using Epic.OnlineServices.Connect;
using Idas3.Multiplayer;
using UnityEngine;
using UnityEngine.Networking;

internal static class OnlineDirectoryHttpDriver {
    const string Self=Idas3EosPlatform.Self,Peer="22222222222222222222222222222222";
    static int checks;
    static UnityWebRequest Last=>UnityWebRequest.Created[UnityWebRequest.Created.Count-1];
    static void Check(bool value,string text){if(!value)throw new Exception(text);checks++;Console.WriteLine("PASS\t"+text);}
    static void Allowed(Task task,string text){Idas3EosPlatform.Pump();Check(task.IsCompleted&&!task.IsFaulted&&!task.IsCanceled,text);}
    static Exception Failed<T>(Task task,string text)where T:Exception{Idas3EosPlatform.Pump();Check(task.IsCompleted,text+" completes");try{task.GetAwaiter().GetResult();}catch(T error){Check(true,text);return error;}throw new Exception(text+" did not fail as expected");}
    static void Released(UnityWebRequest request,string text){Check(request.Disposals>0&&request.uploadHandler.Disposed&&request.downloadHandler.Disposed,text);}
    static void RequestEncoding(){
        var http=new Idas3OnlinePlayerDirectoryHttp();string name="Deon \"quoted\"\n日本語 <tag>";var task=http.Query(Self,Peer,name);var request=Last;
        Check(request.Url==Idas3CommunityTimes.ServiceUrl+"/api/v1/online/player"&&request.Method=="POST","Adapter posts only to configured player-directory endpoint");
        Check(request.Headers["Authorization"]=="Bearer fixture-secret-token"&&request.Headers["Content-Type"]=="application/json","Connect token goes in bearer header with JSON content type");
        var payload=new JavaScriptSerializer().Deserialize<Dictionary<string,string>>(Encoding.UTF8.GetString(request.uploadHandler.data));
        Check(payload.Count==3&&payload["username"]==name&&payload["targetPlayerId"]==Peer&&payload["build"]==Application.version,"UTF-8 JSON payload preserves quotes, Unicode and line breaks and contains only registration fields");
        Check(!Encoding.UTF8.GetString(request.uploadHandler.data).Contains("fixture-secret-token")&&Idas3EosPlatform.Handle.Connect.RequestedUser==Self,"Secret token is not placed in body and is copied for the authenticated player");
        Check(request.timeout==15&&request.redirectLimit==0,"HTTP timeout is bounded and bearer redirects are disabled");
        request.Complete();Check(!task.IsCompleted,"HTTP completion defers access decision until normal EOS dispatch");Allowed(task,"Valid response completes after deferred dispatch");Check(task.Result.version==1&&task.Result.status=="allowed","Success parses response into expected DTO");Released(request,"Success disposes request and handlers");http.Dispose();
        Check(Application.Subscribers==0&&UnityEditor.AssemblyReloadEvents.Subscribers==0,"Explicit disposal removes both shutdown hooks");
    }
    static void TokensAndFailures(){
        var connect=Idas3EosPlatform.Handle.Connect;
        foreach(string invalid in new[]{"signed-out","identity","missing","wrong-user","empty","copy-failure"}){
            Idas3EosPlatform.SignedIn=true;connect.Result=Result.Success;connect.Token=new IdToken{ProductUserId=new ProductUserId(Self),JsonWebToken="fixture-secret-token"};
            if(invalid=="signed-out")Idas3EosPlatform.SignedIn=false;
            if(invalid=="missing")connect.Token=null;
            if(invalid=="wrong-user")connect.Token=new IdToken{ProductUserId=new ProductUserId(Peer),JsonWebToken="fixture-secret-token"};
            if(invalid=="empty")connect.Token=new IdToken{ProductUserId=new ProductUserId(Self),JsonWebToken=""};
            if(invalid=="copy-failure")connect.Result=Result.AccessDenied;
            var http=new Idas3OnlinePlayerDirectoryHttp();int before=UnityWebRequest.Created.Count;Failed<InvalidOperationException>(http.Query(invalid=="identity"?Peer:Self,Peer,"Deon"),"Invalid credential precondition fails closed: "+invalid);Check(UnityWebRequest.Created.Count==before,"Invalid credential does not construct an HTTP request: "+invalid);http.Dispose();
        }
        Idas3EosPlatform.SignedIn=true;connect.Result=Result.Success;connect.Token=new IdToken{ProductUserId=new ProductUserId(Self),JsonWebToken="fixture-secret-token"};
        foreach(string invalid in new[]{"status","connection","oversized","missing-body","malformed"}){
            var http=new Idas3OnlinePlayerDirectoryHttp();var task=http.Query(Self,Peer,"Deon");var request=Last;
            switch(invalid){case "status":request.Complete("sensitive-service-error",403);break;case "connection":request.Complete("sensitive-service-error",0,UnityWebRequest.Result.ConnectionError);break;case "oversized":request.Complete(new string('x',8193));break;case "missing-body":request.Complete(null);break;case "malformed":request.Complete("{broken-json");break;}
            var error=Failed<Exception>(task,"Invalid HTTP response is rejected: "+invalid);Check(!error.Message.Contains("fixture-secret-token")&&!error.Message.Contains("sensitive-service-error"),"HTTP failure message does not disclose token or service error body: "+invalid);Released(request,"Failure disposes request and handlers: "+invalid);http.Dispose();
        }
        var sending=new Idas3OnlinePlayerDirectoryHttp();UnityWebRequest.ThrowOnSend=true;Failed<InvalidOperationException>(sending.Query(Self,Peer,"Deon"),"Synchronous SendWebRequest failure settles task");Released(Last,"Synchronous send failure disposes the allocated request");UnityWebRequest.ThrowOnSend=false;sending.Dispose();
        Check(Debug.Logs.Count==0,"Adapter never logs credentials, response bodies or network failures");
    }
    static void Lifetimes(){
        var http=new Idas3OnlinePlayerDirectoryHttp();var first=http.Query(Self,Peer,"Deon");var one=Last;var second=http.Query(Self,"","Deon");var two=Last;
        UnityWebRequest.CompleteOnAbort=true;http.Dispose();Failed<OperationCanceledException>(first,"Disposal cancels first concurrent pending request");Failed<OperationCanceledException>(second,"Disposal cancels second concurrent pending request");
        Check(one.Aborts==1&&two.Aborts==1,"Disposal aborts every pending native request once, including callbacks from abort");Released(one,"First canceled request and handlers are disposed");Released(two,"Second canceled request and handlers are disposed");http.Dispose();Check(one.Aborts==1&&two.Aborts==1,"Repeated Dispose is idempotent");
        int before=UnityWebRequest.Created.Count;Failed<InvalidOperationException>(http.Query(Self,Peer,"Deon"),"Disposed adapter rejects reuse");Check(UnityWebRequest.Created.Count==before,"Disposed adapter does not allocate another request");
        one.Complete();two.Complete();Check(first.IsCanceled&&second.IsCanceled,"Late native completion cannot resurrect canceled tasks");
        http=new Idas3OnlinePlayerDirectoryHttp();var quitting=http.Query(Self,Peer,"Deon");var quitRequest=Last;Application.Quit();Failed<OperationCanceledException>(quitting,"Application quitting cancels pending HTTP");Released(quitRequest,"Application quitting disposes native request");Check(Application.Subscribers==0&&UnityEditor.AssemblyReloadEvents.Subscribers==0,"Quit cleanup detaches both lifecycle events");
        http=new Idas3OnlinePlayerDirectoryHttp();var reload=http.Query(Self,Peer,"Deon");var reloadRequest=Last;UnityEditor.AssemblyReloadEvents.Reload();Failed<OperationCanceledException>(reload,"Editor assembly reload cancels pending HTTP");Released(reloadRequest,"Assembly reload disposes native request");Check(Application.Subscribers==0&&UnityEditor.AssemblyReloadEvents.Subscribers==0,"Assembly reload cleanup detaches lifecycle events");
    }
    static void ShutdownOrdering(){
        var http=new Idas3OnlinePlayerDirectoryHttp();var task=http.Query(Self,Peer,"Deon");var request=Last;request.Complete();Check(Idas3EosPlatform.Deferred.Count==1&&!task.IsCompleted,"Completed response may wait in EOS deferred queue");
        Idas3EosPlatform.Stop();Check(Idas3EosPlatform.Deferred.Count==0,"EOS shutdown discards pending deferred callbacks");Application.Quit();Failed<OperationCanceledException>(task,"Quit hook still settles task after EOS discarded its completion");Released(request,"Dropped deferred completion is released during quit");
        Idas3EosPlatform.IsStopping=false;http=new Idas3OnlinePlayerDirectoryHttp();task=http.Query(Self,Peer,"Deon");request=Last;Idas3EosPlatform.Stop();request.Complete();Failed<OperationCanceledException>(task,"HTTP callback arriving after EOS stop cancels directly without Defer");Released(request,"Post-shutdown callback disposes native request");http.Dispose();
        Idas3EosPlatform.IsStopping=false;http=new Idas3OnlinePlayerDirectoryHttp();task=http.Query(Self,Peer,"Deon");request=Last;request.Complete();Idas3EosPlatform.Stop();http.Dispose();Failed<OperationCanceledException>(task,"Owning transport disposal settles task after EOS discarded completion");Released(request,"Owning transport disposal releases deferred HTTP request");
        Idas3EosPlatform.IsStopping=false;UnityWebRequest.CompleteSynchronously=true;http=new Idas3OnlinePlayerDirectoryHttp();task=http.Query(Self,Peer,"Deon");Allowed(task,"Already-completed native operation settles without waiting for an event");Released(Last,"Synchronous completion releases request");http.Dispose();
    }
    static int Main(string[] args){try{switch(args[0]){case "encoding":RequestEncoding();break;case "failures":TokensAndFailures();break;case "lifetime":Lifetimes();break;case "shutdown":ShutdownOrdering();break;default:throw new Exception("Unknown scenario");}Console.WriteLine("TOTAL\t"+checks);return 0;}catch(Exception error){Console.Error.WriteLine(error);return 1;}}
}
