using System;
using System.Diagnostics;
using System.Text;
using System.Threading;
using Epic.OnlineServices;
using Epic.OnlineServices.P2P;
using Idas3.Multiplayer;

// The real route, framing, buffering and dispatch code, with an in-memory SDK
// boundary. Synthetic callback costs isolate dispatch; they are not game FPS.
static class EosP2PBurstDriver
{
    static int checks;
    static void Check(bool ok,string why){if(!ok)throw new Exception(why);++checks;Console.WriteLine("PASS\t"+why);}
    static void Frame(){++UnityEngine.Time.frameCount;Idas3EosPlatform.Now+=1.0/60;}
    static void Scenario(Action<Idas3EosTransport,Idas3EosP2P,P2PInterface> action,bool host=true){
        Idas3EosPlatform.Handle=new P2PTestPlatform();Idas3EosPlatform.Now=100;UnityEngine.Time.frameCount=0;
        var owner=new Idas3EosTransport();
        using(var route=new Idas3EosP2P(owner,new ProductUserId("peer"),"fixture","fixture","compatible",host)){
            var sdk=Idas3EosPlatform.Handle.P2P;
            sdk.Inject(Encoding.UTF8.GetBytes("ID3EOS2:compatible:"+(host?"C":"H")+":peer-reset-1"),0);sdk.Inject(new byte[]{1},0);
            route.Poll();Check(route.Connected&&owner.Admissions==1,"Authenticated fixture admission");Frame();action(owner,route,sdk);
        }
    }
    static void Message(P2PInterface sdk,uint id,int size=4,byte channel=1){
        var bytes=new byte[size];Array.Copy(BitConverter.GetBytes(id),bytes,4);
        for(int i=4;i<size;i++)bytes[i]=(byte)(i+id);
        for(int i=0;i<Idas3EosPackets.Count(size);i++)sdk.Inject(Idas3EosPackets.Fragment(bytes,id,i),channel);
    }
    static void Exact(Idas3EosTransport owner,int count,int size){
        Check(owner.Received.Count==count,"All queued messages delivered once");
        for(int i=0;i<count;i++){
            var bytes=owner.Received[i];Check(bytes.Length==size&&BitConverter.ToUInt32(bytes,0)==i+1,"Reliable FIFO identity "+i);
            for(int n=4;n<size;n++)if(bytes[n]!=(byte)(n+i+1))throw new Exception("Fragmented payload changed");
        }
    }
    static void Drain(Idas3EosTransport owner,Idas3EosP2P route,int count){
        for(int frame=0;owner.Received.Count<count&&frame<180;frame++){Frame();route.Poll();Check(owner.Failure==null,"Backlog does not disconnect");}
        Check(owner.Received.Count==count,"Bounded dispatcher drains backlog");
    }
    static void Observe(){
        Scenario((owner,route,sdk)=>{
            for(uint i=1;i<=1100;i++)Message(sdk,i);
            route.Poll();Console.WriteLine("OBSERVE\tbacklog\tdelivered="+owner.Received.Count+"\tfailure="+(owner.Failure??"none")+"\tsdkRemaining="+sdk.QueuedPackets);
        });
        Scenario((owner,route,sdk)=>{
            for(uint i=1;i<=16;i++)Message(sdk,i);
            owner.OnDeliver=(r,b)=>{Thread.Sleep(5);Idas3EosPlatform.Now+=.005;};
            var clock=Stopwatch.StartNew();route.Poll();clock.Stop();
            Console.WriteLine("OBSERVE\tsynthetic-slow-callbacks\tdelivered="+owner.Received.Count+"\tmilliseconds="+clock.Elapsed.TotalMilliseconds.ToString("R",System.Globalization.CultureInfo.InvariantCulture));
        });
    }
    static void Verify(){
        Scenario((owner,route,sdk)=>{
            for(uint i=1;i<=1100;i++)Message(sdk,i);route.KeepAliveOnly();
            for(int i=0;i<20;i++){Idas3EosPlatform.Now+=3;sdk.Inject(new byte[]{3},0);route.KeepAliveOnly();}
            Check(route.Connected&&owner.Received.Count==0,"Long loading backlog still receives heartbeats without game delivery");
            Drain(owner,route,1100);Exact(owner,1100,4);
        });
        Scenario((owner,route,sdk)=>{
            for(uint i=1;i<=1100;i++)Message(sdk,i);route.KeepAliveOnly();sdk.Inject(new byte[]{2},0);route.Poll();
            Check(owner.Failure!=null&&owner.Failure.Contains("left")&&owner.Received.Count==0,"Peer leave is received even behind a full game backlog");
        });
        foreach(bool host in new[]{true,false})Scenario((owner,route,sdk)=>{
            for(uint i=1;i<=1100;i++)Message(sdk,i);
            route.KeepAliveOnly();Check(owner.Received.Count==0,"Loading pump never delivers game callbacks");
            route.Poll();Check(owner.Failure==null,"Valid backlog larger than delivery buffer is retained by SDK");
            Check(owner.Received.Count>0&&owner.Received.Count<=64,"One frame has a finite message budget");
            int first=owner.Received.Count;route.Poll();route.Poll();Check(owner.Received.Count==first,"Repeated pumps share one frame budget");
            Drain(owner,route,1100);Exact(owner,1100,4);
            Check(sdk.QueuedPackets==0&&route.Connected,"Entire backlog drains with connection intact");
        },host);
        Scenario((owner,route,sdk)=>{
            for(uint i=1;i<=20;i++)Message(sdk,i,65536);
            route.Poll();Check(owner.Failure==null,"Maximum-sized fragmented messages respect byte backpressure");
            Drain(owner,route,20);Exact(owner,20,65536);
        });
        Scenario((owner,route,sdk)=>{
            for(uint i=1;i<=16;i++)Message(sdk,i);
            owner.OnDeliver=(r,b)=>Idas3EosPlatform.Now+=.005;route.Poll();
            Check(owner.Received.Count==1,"One expensive callback exhausts dispatch time budget");
            route.Poll();Check(owner.Received.Count==1,"Second pump cannot restart time budget in same frame");
            Drain(owner,route,16);Exact(owner,16,4);
        });
        Scenario((owner,route,sdk)=>{
            for(uint i=1;i<=100;i++)Message(sdk,i);
            int depth=0,maxDepth=0;owner.OnDeliver=(r,b)=>{maxDepth=Math.Max(maxDepth,++depth);r.Poll();--depth;};
            route.Poll();Check(maxDepth==1&&owner.Received.Count<=64,"Nested pumps cannot re-enter application delivery");
            Drain(owner,route,100);Exact(owner,100,4);
        });
        Scenario((owner,route,sdk)=>{
            for(uint i=1;i<=100;i++)Message(sdk,i);
            owner.OnDeliver=(r,b)=>r.Dispose();route.Poll();Frame();route.Poll();
            Check(owner.Received.Count==1&&sdk.Closed==1,"Disposal during delivery stops all remaining messages");
        });
        Scenario((owner,route,sdk)=>{
            for(uint i=1;i<=90;i++){Message(sdk,i);Message(sdk,i,4,2);}
            Drain(owner,route,180);
            for(int i=0;i<180;i++)Check(BitConverter.ToUInt32(owner.Received[i],0)==(uint)(i/2+1),"Mixed-channel FIFO preserved "+i);
        });
        Scenario((owner,route,sdk)=>{
            for(uint i=1;i<=120;i++){Frame();Message(sdk,i);route.Poll();Check(owner.Received.Count==i,"Normal one-input-per-frame stream has no dispatch delay");}
        });
    }
    static int Main(string[] args){try{Observe();if(args.Length==0||args[0]!="--observe")Verify();Console.WriteLine("Completed "+checks+" burst checks");return 0;}catch(Exception e){Console.Error.WriteLine(e);return 1;}}
}
