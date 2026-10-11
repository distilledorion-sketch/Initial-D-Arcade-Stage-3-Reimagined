using System;
using System.IO;
using System.Reflection;
using System.Text;

// Loads the actual built assemblies. No MonoBehaviour/client is instantiated,
// no native game entry point is invoked, and no Discord IPC/presence is sent.
internal static class DiscordJsonCompatibilityDriver
{
    const BindingFlags Hidden=BindingFlags.Static|BindingFlags.NonPublic;
    static string managed;
    static int checks;
    static void Check(bool value,string label){if(!value)throw new Exception(label);++checks;Console.WriteLine("PASS\t"+label);}
    static object Read(object value,string field)=>value.GetType().GetField(field).GetValue(value);
    static void Set(object value,string field,object data)=>value.GetType().GetField(field).SetValue(value,data);
    static Assembly Load(string file)=>Assembly.LoadFrom(Path.Combine(managed,file));
    static int Main(string[] args)
    {
        try{
            managed=Path.GetFullPath(args[0]);
            AppDomain.CurrentDomain.AssemblyResolve+=(sender,eventArgs)=>{
                var path=Path.Combine(managed,new AssemblyName(eventArgs.Name).Name+".dll");
                return File.Exists(path)?Assembly.LoadFrom(path):null;
            };
            var json=Load("Newtonsoft.Json.dll");var rpc=Load("DiscordRPC.dll");
            Check(Path.GetFullPath(json.Location)==Path.Combine(managed,"Newtonsoft.Json.dll"),"Actual player Newtonsoft assembly loaded");
            Check(json.GetType("Newtonsoft.Json.Utilities.AotHelper")!=null,"Shared JSON includes Unity Services AotHelper");
            var game=Load("Assembly-CSharp.dll");var presence=game.GetType("Idas3DiscordPresence",true);
            var describe=presence.GetMethod("Describe",Hidden);var build=presence.GetMethod("Build",Hidden);var replay=presence.GetMethod("DescribeReplay",Hidden);
            var snapshot=Activator.CreateInstance(presence.GetNestedType("Snapshot",BindingFlags.NonPublic));
            var status=Activator.CreateInstance(game.GetType("Idas3Native+Status",true));
            Set(status,"racePhase",2);Set(snapshot,"car",0);Set(snapshot,"ticks6000",804000);
            var courseNames=new[]{"Myogi","Usui","Akagi","Akina","Happogahara","Irohazaka","Shomaru","Tsuchisaka","Akina Snow","Hakone","Sadamine","Enna Skyline","Myogi (Special Stage)","Usui (Special Stage)","Momiji Line","Tsubaki Line","Gunsai","Odawara"};
            var forward=new[]{"Counterclockwise","Counterclockwise","Downhill","Downhill","Outbound","Downhill","Outbound","Outbound","Downhill","Downhill","Downhill","Downhill","Downhill","Downhill","Downhill","Downhill","Outbound","Clockwise"};
            var reverse=new[]{"Clockwise","Clockwise","Uphill","Uphill","Inbound","Reverse","Inbound","Inbound","Uphill","Uphill","Uphill","Uphill","Uphill","Uphill","Uphill","Uphill","Inbound","Counterclockwise"};
            var replayData=Activator.CreateInstance(game.GetType("Idas3ReplayData+Details",true));
            for(int course=0;course<courseNames.Length;++course)for(int direction=0;direction<2;++direction){
                int condition=course*2+direction;Set(snapshot,"condition",condition);Set(replayData,"condition",condition);
                string expected=courseNames[course]+" "+(direction==0?forward[course]:reverse[course]);
                object value=describe.Invoke(null,new[]{snapshot,status,false,false});
                Check((string)Read(value,"details")=="Time Attack · "+expected,"Live course direction "+condition);
                value=replay.Invoke(null,new[]{replayData});
                Check((string)Read(value,"state")==expected+" · Day / Dry · Time Attack","Replay course direction "+condition);
            }
            Set(snapshot,"condition",6);Set(snapshot,"night",1);Set(snapshot,"weather",1);
            object description=describe.Invoke(null,new[]{snapshot,status,false,false});
            Check((double)Read(description,"elapsedSeconds")==134&&(bool)Read(description,"timed"),"Original race clock retained");
            Set(snapshot,"mode",1);Set(snapshot,"opponentName","TAK");
            Check((string)Read(describe.Invoke(null,new[]{snapshot,status,false,false}),"details")=="Online Battle · vs TAK","Online opponent retained");
            Check((string)Read(describe.Invoke(null,new[]{snapshot,status,true,true}),"session")=="matchmaking","Online matchmaking activity retained");
            Set(status,"flags",1u);
            Check((string)Read(describe.Invoke(null,new[]{snapshot,status,true,false}),"session")=="lobby","Online lobby activity retained");
            object rich=build.Invoke(null,new[]{description,(object)new DateTime(2026,10,11,0,0,0,DateTimeKind.Utc)});
            Type convert=json.GetType("Newtonsoft.Json.JsonConvert",true);
            var serialize=convert.GetMethod("SerializeObject",new[]{typeof(object)});
            var deserialize=convert.GetMethod("DeserializeObject",new[]{typeof(string),typeof(Type)});
            Func<object,string> encoded=value=>(string)serialize.Invoke(null,new[]{value});
            string payload=encoded(rich);
            Check(payload.Contains("Time Attack · Akina Downhill")&&payload.Contains("large_image")&&payload.Contains("View Leaderboard"),"Presence serializes with actual shared JSON");
            Check(!payload.Contains("secrets")&&!payload.Contains("party"),"No joining secrets or room identity in presence");
            object decoded=deserialize.Invoke(null,new[]{payload,(object)rpc.GetType("DiscordRPC.RichPresence",true)});
            Check((string)decoded.GetType().GetProperty("Details").GetValue(decoded)=="Time Attack · Akina Downhill","Presence JSON round trip");
            var commandType=rpc.GetType("DiscordRPC.RPC.Commands.PresenceCommand",true);var command=Activator.CreateInstance(commandType,true);
            commandType.GetProperty("PID").SetValue(command,1234);commandType.GetProperty("Presence").SetValue(command,rich);
            var wire=commandType.GetMethod("PreparePayload").Invoke(command,new object[]{42L});string wireJson=encoded(wire);
            Check(wireJson.Contains("\"cmd\":\"SET_ACTIVITY\"")&&wireJson.Contains("\"nonce\":\"42\"")&&wireJson.Contains("\"pid\":1234"),"Discord command converter remains compatible");
            var responseType=rpc.GetType("DiscordRPC.RichPresenceResponse",true);
            var merge=rich.GetType().GetMethod("Merge",BindingFlags.Instance|BindingFlags.NonPublic);
            foreach(string assets in new[]{"{\"large_image\":\"1234567\",\"large_text\":\"Stage 3\"}","{}"}){
                var response=deserialize.Invoke(null,new object[]{"{\"name\":\"Stage 3\",\"application_id\":\"1551296157567819807\",\"assets\":"+assets+"}",responseType});
                merge.Invoke(rich,new[]{response});
                Check(rich.GetType().GetProperty("Assets").GetValue(rich)!=null,"Missing optional Discord image fields merge safely");
            }
            Set(description,"details",new string('\u754c',100));rich=build.Invoke(null,new[]{description,(object)DateTime.UtcNow});
            Check(Encoding.UTF8.GetByteCount((string)rich.GetType().GetProperty("Details").GetValue(rich))<=128,"UTF8 presence limit preserved");
            Console.WriteLine("TOTAL\t"+checks);return 0;
        }catch(Exception error){Console.Error.WriteLine(error);return 1;}
    }
}
