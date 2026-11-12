using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using Idas3.Multiplayer;

namespace UnityEngine {
    public static class Time {
        static readonly Stopwatch Clock=Stopwatch.StartNew();
        public static double realtimeSinceStartupAsDouble=>Clock.Elapsed.TotalSeconds;
    }
}
internal static class TcpTransportDriver {
    static void Check(bool value,string text){if(!value)throw new InvalidOperationException(text);Console.WriteLine("PASS\t"+text);}
    static void Until(Func<bool> done,Action pump){
        var clock=Stopwatch.StartNew();
        while(!done()&&clock.Elapsed.TotalSeconds<3){pump();Thread.Sleep(1);}
        if(!done())throw new TimeoutException("Local socket condition was not met");
    }
    static int Main(){try{
        var reservation=new TcpListener(IPAddress.Loopback,0);reservation.Start();
        int port=((IPEndPoint)reservation.LocalEndpoint).Port;reservation.Stop();
        using(var host=new Idas3TcpTransport(port,true,"Host"))
        using(var guest=new Idas3TcpTransport(port,true,"Guest")){
            var received=new List<byte[]>();var replies=new List<byte[]>();var errors=new List<string>();
            host.Error+=errors.Add;guest.Error+=errors.Add;
            guest.Message+=received.Add;host.Message+=replies.Add;
            host.Initialize();guest.Initialize();host.Host("Private fixture");guest.Join(host.RoomCode);
            Until(()=>host.Connected&&guest.Connected,()=>{host.Poll();guest.Poll();});
            Check(errors.Count==0,"Real loopback peers connect without transport errors");
            byte[] load={4,17,23,42};host.Send(load,true);
            // StartRace immediately enters native loading after Send. Only the
            // guest can poll during that synchronous host load.
            var deadline=Stopwatch.StartNew();
            while(received.Count==0&&deadline.Elapsed.TotalSeconds<1){guest.Poll();Thread.Sleep(1);}
            Check(received.Count==1&&received[0].SequenceEqual(load),"Host sends the loading notification before its next Poll");
            guest.Send(new byte[]{5,99},true);
            Check(replies.Count==0,"Send does not dispatch incoming callbacks during native initialization");
            Until(()=>replies.Count==1,host.Poll);
            Check(replies[0].SequenceEqual(new byte[]{5,99}),"Pending loading acknowledgement arrives on the normal host Poll");
            for(int i=0;i<32;i++){var payload=new byte[4096];payload[0]=(byte)i;payload[4095]=(byte)(255-i);host.Send(payload,true);}
            Until(()=>received.Count==33,()=>{host.Poll();guest.Poll();});
            Check(received.Skip(1).Select((p,i)=>p.Length==4096&&p[0]==i&&p[4095]==255-i).All(v=>v),"Immediate and queued sends preserve framing and order");
            Check(errors.Count==0,"Burst delivery leaves both peers healthy");
            guest.Leave();Until(()=>!host.Connected,host.Poll);
            Check(!host.Connected&&!guest.Connected,"Peer leave closes the direct connection cleanly");
        }
        LanAddressChecks.Run();
        return 0;
    }catch(Exception error){Console.Error.WriteLine(error);return 1;}}
}
