using System;
using System.Collections.Generic;
using System.IO;
using Idas3.Multiplayer;
using UnityEngine;

// Explicit diagnostic entrypoint; does not touch real controls or public rooms.
public static class Idas3PrivateShifterChecks {
    static int checks;
    static void Check(bool ok,string why){++checks;if(!ok)throw new Exception(why);}
    sealed class Platform:Idas3GameOptions.IPlatform {
        public int Width=>1280;public int Height=>720;public int DisplayMode=>0;public double Now=>0;
        public Idas3GameOptions.ResolutionChoice[] Resolutions=>new[]{new Idas3GameOptions.ResolutionChoice(1280,720)};
        public void Apply(Idas3GameOptions.Values a,Idas3GameOptions.Values b,bool display){}
    }
    sealed class FakeRooms:IIdas3MatchmakingTransport {
        public List<Idas3Room> entries=new List<Idas3Room>();public string joined;public bool hosted;
        public string Kind=>"Test";public bool Available=>true;public bool Connected=>false;
        public bool IsHost=>false;public string LocalId=>"test";public string LocalName=>"Test";
        public string RemoteId=>"";public string RemoteName=>"";public string RoomCode=>"";public string Status=>"";
        public IReadOnlyList<Idas3Room> Rooms=>entries;public bool IsBusy=>false;public bool InLobby=>false;
        public ulong RoomOrder=>0;public int RoomMembers=>0;public string BuildCompatibility{get;set;}
        public event Action<byte[]> Message {add{}remove{}}public event Action PeerChanged {add{}remove{}}public event Action<string> Error {add{}remove{}}
        public bool Initialize()=>true;public void Host(string name){hosted=true;}public void HostQuickMatch(string name)=>Host(name);
        public void Join(string code){joined=code;}public void Browse(){}public void Send(byte[] b,bool reliable){}public void Poll(){}public void Leave(){}public void Dispose(){}
    }
    public static void Run(){
        checks=0;string root=Path.GetFullPath("Verification/private-shifter-20261007/managed-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(root);
        var mapper=new Idas3ControlBindings();mapper.Initialize(root);
        Check(mapper.Current.actions.Length==16,"Direct gear actions missing");
        for(int gear=1;gear<=6;++gear)Check(mapper.TrySetDraftKey((Idas3ControlBindings.ActionId)(9+gear),Idas3ControlBindings.Slot.Primary,KeyCode.Keypad0+gear),"Bind gear key");
        Check(mapper.ApplyDraft(),"Save all six gear keys");
        mapper=new Idas3ControlBindings();mapper.Initialize(root);
        uint Read(params KeyCode[] held){mapper.Poll(k=>Array.IndexOf(held,k)>=0,default,0);var frame=new Idas3Native.FrameInput{flags=1};mapper.ApplyDriving(ref frame);Check((frame.flags&1)!=0,"Direct gear changed focus");return (frame.flags>>8)&7;}
        for(int gear=1;gear<=6;++gear){Check(Read(KeyCode.Keypad0+gear)==gear,"Saved gear did not reach native input");Check(Read()==0,"Released gear stayed held");}
        Check(Read(KeyCode.Keypad1,KeyCode.Keypad6)==0,"Overlapping shifter positions chose arbitrary gear");
        mapper.BeginCapture(Idas3ControlBindings.ActionId.Gear6,Idas3ControlBindings.Slot.Primary,0);Check(Read(KeyCode.Keypad1)==0,"Capture leaked a direct gear");mapper.CancelCapture();Read();
        string legacy=Path.Combine(root,"legacy");Directory.CreateDirectory(legacy);var old=Idas3ControlBindings.Defaults();old.version=3;Array.Resize(ref old.actions,10);old.actions[4].key1=KeyCode.R;
        old.controllerProfiles=new[]{new Idas3ControlBindings.ControllerProfile{key="shifter",label="Shifter",generic=true,actions=old.Clone().actions}};
        File.WriteAllText(Path.Combine(legacy,"controls.json"),JsonUtility.ToJson(old));var migrated=new Idas3ControlBindings();migrated.Initialize(legacy);
        Check(migrated.LastError==null&&migrated.Current.version==4,"Version 3 controls did not migrate");
        for(int i=0;i<10;++i)Check(JsonUtility.ToJson(old.actions[i])==JsonUtility.ToJson(migrated.Current.actions[i]),"Migration changed existing controls");
        migrated.SelectControllerProfile("shifter","Shifter",true);Check(migrated.Current.actions.Length==16&&migrated.ApplyDraft(),"Stored device profile did not migrate");
        // Use the real multi-device polling path, including its reconnect guard.
        var gate=new Idas3ControllerControl{path="button6",label="Sixth gear",minimum=0,maximum=1,button=true,value=0};
        Check(migrated.TrySetDraftControl(Idas3ControlBindings.ActionId.Gear6,gate,1,0)&&migrated.ApplyDraft(),"Save H-shifter device gate");
        migrated.SelectControllerProfile("wheel","Wheel",true);
        var sample=new Idas3ControllerDevices.RigSample{key="physical-shifter",profile="shifter",controls=new[]{gate}};
        uint Rig(bool connected){migrated.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true},0,Array.Empty<Idas3ControllerControl>(),connected?new[]{sample}:Array.Empty<Idas3ControllerDevices.RigSample>());var frame=new Idas3Native.FrameInput();migrated.ApplyDriving(ref frame);return(frame.flags>>8)&7;}
        Rig(true);gate.value=1;Check(Rig(true)==6,"Auxiliary H-shifter lost its gear");Check(Rig(false)==0,"Disconnected shifter retained stale gear");Check(Rig(true)==0,"Reconnected held shifter bypassed guard");gate.value=0;Rig(true);gate.value=1;Check(Rig(true)==6,"Shifter did not recover after release");
        Check(!Idas3SteamTransport.PublicNamespace(Idas3SteamTransport.RoomNamespace(true)),"Private room uses public discovery namespace");
        Check(Idas3SteamTransport.PublicNamespace(Idas3SteamTransport.RoomNamespace(false)),"Public room lost discovery");
        Check(Idas3SteamTransport.RoomType(true)==Steamworks.ELobbyType.k_ELobbyTypeInvisible,"Code-only room cannot be joined without invite");
        var transport=new FakeRooms();transport.entries.Add(new Idas3Room{Code="private",Private=true,Members=1,Capacity=2,Order=1});
        transport.entries.Add(new Idas3Room{Code="public",Members=1,Capacity=2,Order=2});
        var queue=new Idas3QuickMatch(transport,()=>0,()=>0,"Test");queue.Start();queue.Tick(false);Check(transport.joined=="public","Quick Match selected private room");queue.Cancel();
        transport.entries.RemoveAt(1);transport.joined=null;queue.Start();queue.Tick(false);Check(transport.hosted&&transport.joined==null,"Private-only search did not host a public match");queue.Cancel();
        var go=new GameObject("Private shifter menu check");
        try{
            var options=new Idas3GameOptions(new Platform());options.Initialize(Path.Combine(root,"options"));
            var menu=go.AddComponent<Idas3PauseMenu>();menu.Initialize(options);menu.InitializeBindings(mapper);menu.OpenAttractOptions();menu.SelectTab(3);
            Check(menu.OptionRows==16,"Controls navigation excludes gear bindings");
            for(int i=0;i<15;++i)menu.Navigate(1);menu.Activate();Check(menu.BindingChoiceVisible,"Controller cannot reach Gear 6");
            menu.Activate();Check(mapper.IsCapturing,"Gear 6 rebind action failed");mapper.CancelCapture();
        }finally{UnityEngine.Object.DestroyImmediate(go);}
        Idas3WheelRigChecks.Run();Idas3HeadlightBindingChecks.Run();
        File.WriteAllText(Path.Combine(root,"PASS.txt"),"PASS "+checks+" private room and direct gear checks. Synthetic devices; no physical H-shifter or second Steam account.\n");Debug.Log("PASS "+checks+" private room and direct gear checks");
    }
}

