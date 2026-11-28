#if UNITY_EDITOR
using System;
using System.IO;
using System.Reflection;
using Idas3.Multiplayer;
using UnityEngine;

// Actual result owner, binding mapper, pause router and final neutralizer;
// only native result I/O is substituted. The public-entry check samples
// Unity's existing keyboard/mouse state. No native load, room, device opening,
// preference writes, game window or audio device.
internal static class Idas3OnlineFinishInputRuntimeChecks
{
    const uint Original=16,Finish=0x10000000;
    static int checks;
    static void Check(bool ok,string text){++checks;if(!ok)throw new Exception(text);}
    static void Put(object obj,string name,object value)=>obj.GetType().GetField(name,BindingFlags.Instance|BindingFlags.Public|BindingFlags.NonPublic).SetValue(obj,value);
    static bool Key(Idas3Native.FrameInput f,int key)=>( (uint)typeof(Idas3Native.FrameInput).GetField("key"+(key/32)).GetValue(f)&(1u<<(key%32)))!=0;
    static void Neutralize(ref Idas3Native.FrameInput f){var args=new object[]{f};typeof(Idas3SceneGame).GetMethod("NeutralizeControls",BindingFlags.Static|BindingFlags.NonPublic).Invoke(null,args);f=(Idas3Native.FrameInput)args[0];}
    static Idas3Native.FrameInput Frame(Idas3ControlBindings.PadState pad)=>new Idas3Native.FrameInput{
        size=88,flags=1,padConnected=1,padButtons=pad.buttons,leftTrigger=pad.leftTrigger,rightTrigger=pad.rightTrigger,thumbLX=pad.thumbLX};
    static void FinishRoute(ref Idas3Native.FrameInput frame,Idas3MultiplayerMenu menu,int tick,bool focused=true,bool capture=false,bool pause=false)
    {
        // SceneGame.Update owns capture/focus neutralization around its pause
        // route. Execute those same neutralizers, then its shared final owner.
        if(capture){Neutralize(ref frame);return;}
        var router=menu.InputRouter;
        router.Process(new Idas3MenuInputRouter.Sample{Buttons=frame.padButtons,Confirm=Key(frame,13),Escape=Key(frame,27),Back=Key(frame,8)},
            new Idas3MenuInputRouter.Context{Focused=focused,OnlineBlocksInput=menu.BlocksGameInputAt(tick),PauseOpen=pause,CanOpenPause=false,Now=tick/60.0});
        Idas3SceneGame.ApplyPauseOwnership(ref frame,router,pause,false,tick);
        if(!focused)Neutralize(ref frame);
    }
    static void Neutral(Idas3Native.FrameInput frame,Idas3PrecisePedals.Sample sample,string why)
    {
        Check((frame.flags&2)!=0&&frame.padButtons==0&&frame.padConnected==0&&frame.rightTrigger==0&&frame.leftTrigger==0&&frame.thumbLX==0,why);
        Check(Idas3PrecisePedals.ForSubmission(frame,sample,false).validMask==0,why+" precise ADC escaped");
    }
    public static int Run(string root)
    {
        checks=0;
        var records=new Idas3MultiplayerRecords(Path.Combine(root,"online-finish-records"));
        var session=new Idas3MultiplayerSession(records,0);
        Put(session,"nativeRace",true);Put(session,"state","Results");
        GameObject go=null;Idas3MultiplayerMenu menu=null;
        try{
            go=new GameObject("Isolated online finish checks"){hideFlags=HideFlags.HideAndDontSave};
            menu=go.AddComponent<Idas3MultiplayerMenu>();menu.enabled=false;menu.Initialize(session);
            // Avoid SetOpen services/cursor effects; this is an already-open
            // native results owner, not a mock session transport.
            Put(menu,"<IsOpen>k__BackingField",true);menu.NativeFinishDriving=true;
            int page=0,confirms=0,cancels=0,writes=0;uint lastFlags=0;
            menu.ResultReadForChecks=field=>field==0?page:0;
            menu.ResultWriteForChecks=(confirm,cancel,direction,flags)=>{++writes;confirms+=confirm;cancels+=cancel;lastFlags=flags;return 1;};
            int tick=Time.frameCount+100;
            menu.ProcessResultsInput(false,false,false,tick++,false);
            menu.ProcessResultsInput(false,false,false,tick++,false);
            Check(menu.ResultsVisible&&!menu.BlocksGameInputAt(tick),"Actual page0 still blocks driving");
            var bindings=new Idas3ControlBindings();bindings.Initialize(Path.Combine(root,"online-finish-controls"));
            var values=(Idas3ControlBindings.Values)typeof(Idas3ControlBindings).GetField("current",BindingFlags.Instance|BindingFlags.NonPublic).GetValue(bindings);
            values.actions[4].pad=Idas3ControlBindings.PadInput.RightShoulder;
            bindings.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true},0);
            var pad=new Idas3ControlBindings.PadState{connected=true,rightTrigger=65,leftTrigger=32,thumbLX=12000,buttons=0x200,
                preciseTriggers=true,preciseRightTrigger=8289/32767f,preciseLeftTrigger=.125f};
            bindings.Poll(_=>false,pad,1);
            for(int i=0;i<5;++i){
                var frame=Frame(pad);var sample=Idas3SceneGame.ApplyControlBindings(ref frame,pad,bindings,Original|Finish,3,false,menu.BlocksGameInputAt(tick),false);
                Check((frame.padButtons&0x2000)!=0,"Mapped shoulder did not become canonical upshiftB");
                menu.ProcessResultsInput(false,(frame.padButtons&0x2000)!=0,false,tick,false);
                FinishRoute(ref frame,menu,tick++);
                Check(frame.thumbLX==12000&&frame.rightTrigger==65&&(frame.padButtons&0x2000)!=0,"Page0 altered steering/pedal/shift mapping");
                Check(Idas3PrecisePedals.ForSubmission(frame,sample,false).throttleAdc==16578,"Page0 precise pedal lost");
            }
            menu.ProcessResultsInput(true,true,false,tick++,false);
            Check(confirms==0&&cancels==0,"Driving controls skipped linked cinematic");
            menu.ProcessResultsInput(false,false,false,tick++,false,true);
            menu.ProcessResultsInput(false,false,false,tick++,false,true);
            Check(confirms==1&&cancels==0,"Fresh Finish skip should submit once");
            menu.ProcessResultsInput(false,false,false,tick++,false,false);
            menu.AdvanceResultsPage();
            Check(confirms==2&&cancels==0,"Finish pointer skip did not submit");
            confirms=0;
            // Critical same-Update race: mapper sees old page0/capability;
            // native page read now reports1; final route must neutralize it.
            var changed=Frame(pad);var changedSample=Idas3SceneGame.ApplyControlBindings(ref changed,pad,bindings,Original|Finish,3,false,menu.BlocksGameInputAt(tick),false);
            Check((changed.padButtons&0x2000)!=0,"Transition fixture did not begin as driving");
            page=1;menu.ProcessResultsInput(false,true,false,tick,false);FinishRoute(ref changed,menu,tick++);
            Neutral(changed,changedSample,"Same-Update points transition leaked controls");
            for(int i=0;i<3;++i)menu.ProcessResultsInput(false,true,false,tick++,false);
            Check(confirms==0&&cancels==0,"Held shift became points cancellation");
            menu.ProcessResultsInput(false,false,false,tick++,false);
            menu.ProcessResultsInput(true,false,false,tick++,false);
            Check(confirms==1&&cancels==0,"Fresh points confirm did not work after release");
            page=2;menu.ProcessResultsInput(true,false,false,tick++,false);
            menu.ProcessResultsInput(true,false,false,tick++,false);
            Check(confirms==1,"Held points confirm accepted Continue");
            menu.ProcessResultsInput(false,false,false,tick++,false);menu.ProcessResultsInput(false,true,false,tick++,false);
            Check(cancels==1,"Fresh Continue cancel was lost");
            // Public existing signature still uses the actual neutral/rearm
            // owner; no new default optional parameter changes standalone UI.
            Put(menu,"resultsOpenedFrame",-1);menu.ProcessResultsInput(false,false);
            menu.ProcessResultsInput(true,false);Check(confirms==2,"Public standalone results entry lost confirmation");
            page=0;menu.ProcessResultsInput(false,false,false,tick++,false);
            int count=confirms+cancels;
            var pending=new Idas3BattleRecordSettlement(records,0,123,0,true,Idas3BattleRecord.Fresh,Idas3BattleRecord.Fresh,600,620);
            Put(session,"settlement",pending);
            Check(session.ResultSavePending&&!menu.ResultRecoveryVisible&&!menu.BlocksGameInputAt(tick),"Nonmodal save lifecycle incorrectly consumed driving");
            menu.ProcessResultsInput(true,true,false,tick++,false);
            Check((lastFlags&16)!=0&&(lastFlags&1)==0,"Nonmodal save froze source finish instead of holding only EOS handoff");
            Put(session,"<ErrorText>k__BackingField","Synthetic save failure");
            Check(menu.ResultRecoveryVisible&&menu.BlocksGameInputAt(tick),"Visible recovery did not own driving");
            var recovery=Frame(pad);var recoverySample=Idas3SceneGame.ApplyControlBindings(ref recovery,pad,bindings,Original|Finish,3,false,menu.BlocksGameInputAt(tick),false);
            menu.ProcessResultsInput(true,true,false,tick,false);FinishRoute(ref recovery,menu,tick++);Neutral(recovery,recoverySample,"Recovery modal leaked controls");
            Put(session,"settlement",null);Put(session,"<ErrorText>k__BackingField","");
            Check(!menu.BlocksGameInputAt(tick),"Recovery completion retained stale driving block");
            foreach(string state in new[]{"Disconnected","Returning","Lobby","Loading"}){
                Put(session,"state",state);Check(menu.BlocksGameInputAt(tick),"Stale finish capability leaked through "+state);
                var f=Frame(pad);var s=Idas3SceneGame.ApplyControlBindings(ref f,pad,bindings,Original|Finish,3,false,menu.BlocksGameInputAt(tick),false);
                menu.ProcessResultsInput(true,true,false,tick,false);FinishRoute(ref f,menu,tick++);Neutral(f,s,"Stale ResultsPage bypassed "+state);
            }
            Put(session,"state","Results");Put(session,"<DisconnectedFinish>k__BackingField",true);
            Check(menu.BlocksGameInputAt(tick),"Disconnected flag with stale Results state leaked controls");
            Put(session,"<DisconnectedFinish>k__BackingField",false);
            menu.ProcessResultsInput(false,false,false,tick++,false);
            menu.NativeFinishDriving=false;Check(menu.BlocksGameInputAt(tick),"Old native DLL accidentally opted into finish driving");menu.NativeFinishDriving=true;
            for(int gate=0;gate<6;++gate){
                menu.InputCovered=gate==0;Put(menu,"chatEditing",gate==1);
                if(gate==2)menu.InputRouter.OnlineTextEnded(tick);
                var f=Frame(pad);var s=Idas3SceneGame.ApplyControlBindings(ref f,pad,bindings,Original|Finish,3,gate==5,menu.BlocksGameInputAt(tick),false);
                menu.ProcessResultsInput(true,true,gate==3||gate==4,tick,false);
                FinishRoute(ref f,menu,tick,gate!=3,gate==4,gate==5);Neutral(f,s,"Modal/focus/capture gate "+gate+" leaked");
                menu.InputCovered=false;Put(menu,"chatEditing",false);
                tick+=3;
                // Normal release outside terminal ownership clears text guard.
                menu.InputRouter.ProcessOnline(default,new Idas3MenuInputRouter.OnlineContext{Frame=tick});
            }
            Check(confirms+cancels==count,"Recovery/terminal/covered controls activated result actions");
            Check(writes>15,"Result API substitution was never reached");
            Check(!Directory.Exists(Path.Combine(root,"online-finish-records")),"Routing fixture wrote result history");
            return checks;
        }finally{
            if(menu!=null)Put(menu,"<IsOpen>k__BackingField",false);
            if(go!=null)UnityEngine.Object.DestroyImmediate(go);
            Put(session,"nativeRace",false);Put(session,"settlement",null);session.Dispose();
        }
    }
}
#endif
