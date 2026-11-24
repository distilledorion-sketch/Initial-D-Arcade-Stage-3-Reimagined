#if UNITY_EDITOR
using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using UnityEngine;

// Real managed binding/pause/router methods, isolated synthetic sources only.
// Source counter/timer ordering belongs to the separate native/original tests.
internal static class Idas3ForcedQuitRuntimeChecks
{
    const uint Reserve=Idas3Native.SourceCabinetForcedQuitFlag,Card=134217728u,Start=1u<<14;
    static int checks;
    static readonly BindingFlags Private=BindingFlags.Instance|BindingFlags.NonPublic;
    static void Check(bool ok,string why){++checks;if(!ok)throw new Exception(why);}
    static Idas3ControlBindings New(string root,string name)
    {var b=new Idas3ControlBindings();b.Initialize(Path.Combine(root,"forced-quit-"+name));return b;}
    static Idas3ControlBindings.Values Values(Idas3ControlBindings b)=>(Idas3ControlBindings.Values)
        typeof(Idas3ControlBindings).GetField("current",Private).GetValue(b);
    static bool Key(Idas3Native.FrameInput f,int k)=>
        (((uint)typeof(Idas3Native.FrameInput).GetField("key"+(k/32)).GetValue(f))&(1u<<(k%32)))!=0;
    static Idas3Native.FrameInput Route(Idas3ControlBindings b,Idas3ControlBindings.PadState pad=default,
        uint status=Reserve|Card|16u,int phase=2,uint flags=1,bool pause=false,bool online=false,bool music=false,bool generic=false)
    {
        var f=new Idas3Native.FrameInput{size=88,flags=flags,padConnected=pad.connected?1u:0u,padButtons=pad.buttons};
        Idas3SceneGame.ApplyControlBindings(ref f,pad,b,status,phase,pause,online,generic);
        Idas3SceneGame.ApplyCabinetPauseRouting(ref f,b,status,pause,online,music);return f;
    }
    static Idas3MenuInputRouter.Action Command(Idas3Native.FrameInput f,bool reserve,bool view)
    {
        var router=new Idas3MenuInputRouter();
        return router.Process(new Idas3MenuInputRouter.Sample{Escape=Key(f,27),Buttons=f.padButtons,ViewChange=view},
            new Idas3MenuInputRouter.Context{Focused=true,CanOpenPause=true,CardForcedQuit=true,SourceCabinetStartReserved=reserve}).Kind;
    }
    static void Neutralize(ref Idas3Native.FrameInput f)
    {
        object[] args={f};typeof(Idas3SceneGame).GetMethod("NeutralizeControls",BindingFlags.Static|BindingFlags.NonPublic).Invoke(null,args);
        f=(Idas3Native.FrameInput)args[0];
    }
    public static int Run(string root)
    {
        checks=0;var defaults=New(root,"defaults");
        var startPad=new Idas3ControlBindings.PadState{connected=true,buttons=0x10};
        for(int phase=0;phase<3;++phase){
            foreach(var key in new[]{KeyCode.Return,KeyCode.KeypadEnter,KeyCode.Escape,KeyCode.None}){
                defaults.Poll(k=>k==key,default,phase);var f=Route(defaults,phase:phase);
                Check(((f.flags&Start)!=0)==(key==KeyCode.Return||key==KeyCode.KeypadEnter),"Wrong default Start phase "+phase);
                Check(Key(f,27)==(key==KeyCode.Escape),"Reserved Start became Pause or lost Escape");
                Check(Command(f,true,true)==(key==KeyCode.Escape?Idas3MenuInputRouter.Action.OpenPause:Idas3MenuInputRouter.Action.None),
                    "Source reserve swallowed explicit Escape+View or paused Start");
            }
            defaults.Poll(_=>false,startPad,10+phase);var pad=Route(defaults,startPad,phase:phase);
            Check((pad.flags&Start)==0&&Key(pad,27)&&Command(pad,true,false)==Idas3MenuInputRouter.Action.OpenPause,
                "Bound Menu button did not open Pause independently of cabinet Start");
        }
        // Exactly the old host policy when the native source capability is absent.
        foreach(uint status in new[]{16u,16u|Card,16u|512u,16u|16384u}){
            defaults.Poll(_=>false,startPad,15);var f=Route(defaults,startPad,status);
            Check(Key(f,27),"Excluded/disabled/legacy context changed physical Start Pause");
        }
        var legacy=new Idas3Native.FrameInput{size=88,flags=1};legacy.SetKey(27);
        Check(Command(legacy,false,true)==Idas3MenuInputRouter.Action.None,"Legacy card chord routing changed");
        Check(Command(legacy,true,true)==Idas3MenuInputRouter.Action.OpenPause,"Explicit source-context Escape got legacy chord ownership");
        defaults.Poll(k=>k==KeyCode.C||k==KeyCode.Return,default,16);
        var chord=Route(defaults);Check((chord.flags&Start)!=0&&Key(chord,'C')&&!Key(chord,27),"Default keyboard chord failed mapping");
        var startViewPad=new Idas3ControlBindings.PadState{connected=true,buttons=0x8040};
        defaults.Poll(_=>false,startViewPad,17);chord=Route(defaults,startViewPad);
        Check((chord.flags&Start)!=0&&(chord.padButtons&0x8000)!=0&&!Key(chord,27),"Default pad chord lost View");
        for(int n=0;n<100;++n)Check((Route(defaults,startViewPad).flags&Start)!=0&&!Key(Route(defaults,startViewPad),27),
            "Host render reads consumed Start or invented delayed Pause");

        var custom=New(root,"custom");var v=Values(custom);
        v.actions[(int)Idas3ControlBindings.ActionId.Start].key1=KeyCode.K;
        v.actions[(int)Idas3ControlBindings.ActionId.Pause].key2=KeyCode.P;
        v.actions[(int)Idas3ControlBindings.ActionId.Camera].key1=KeyCode.J;
        v.actions[(int)Idas3ControlBindings.ActionId.Camera].key2=KeyCode.None;
        Check(Idas3ControlBindings.Validate(v,out _),"Fixture custom binding is invalid");
        foreach(var key in new[]{KeyCode.K,KeyCode.P,KeyCode.Escape,KeyCode.Return,KeyCode.J}){
            custom.Poll(k=>k==key,default,18);var f=Route(custom);
            Check(((f.flags&Start)!=0)==(key==KeyCode.K),"Configured Start admitted a fallback");
            Check(Key(f,27)==(key==KeyCode.P||key==KeyCode.Escape),"Independent Pause mapping lost");
            Check(Key(f,'C')==(key==KeyCode.J),"Resolved custom View did not reach canonical C");
        }
        custom.Poll(_=>false,startPad,19);
        Check((Route(custom,startPad).flags&Start)==0&&Key(Route(custom,startPad),27),"Explicit Start override lost separate physical Pause");
        custom.Poll(k=>k==KeyCode.K,default,20);
        Check(Key(Route(custom,status:16),27),"Configured Start legacy Pause changed");
        foreach(int modal in new[]{0,1,2}){
            var f=Route(custom,pause:modal==0,online:modal==1,music:modal==2);Neutralize(ref f);
            Check((f.flags&Start)==0&&(f.flags&2)!=0&&!Key(f,27),"Final modal ownership leaked cabinet or Pause");
        }
        custom.BlockStartUntilRelease();custom.Poll(k=>k==KeyCode.K,default,21);
        Check((Route(custom).flags&Start)==0&&!Key(Route(custom),27),"Blocked Start became Pause instead of waiting for release");
        custom.Poll(_=>false,default,22);custom.Poll(k=>k==KeyCode.K,default,23);
        Check((Route(custom).flags&Start)!=0,"Source Start did not rearm");
        var unfocused=Route(custom,flags:0);Neutralize(ref unfocused);
        Check((unfocused.flags&Start)==0,"Focus loss retained Start");
        custom.BlockStartUntilRelease();custom.Poll(k=>k==KeyCode.K,default,24);
        Check((Route(custom).flags&Start)==0,"Refocus held Start synthesized edge");
        custom.Poll(_=>false,default,25);custom.Poll(k=>k==KeyCode.K,default,26);
        Check((Route(custom).flags&Start)!=0,"Refocus release failed to restore Start");

        // Save validation forbids Escape as Start and duplicate source bindings.
        // The defensive routing boundary is still explicit if a caller injects it.
        var conflict=New(root,"invalid-conflict");Values(conflict).actions[(int)Idas3ControlBindings.ActionId.Start].key1=KeyCode.Escape;
        Check(!Idas3ControlBindings.Validate(Values(conflict),out _),"Reserved Escape binding unexpectedly allowed");
        conflict.Poll(k=>k==KeyCode.Escape,default,27);var cf=Route(conflict);
        Check((cf.flags&Start)!=0&&!Key(cf,27),"Injected Start=Escape conflict silently took both owners");
        var rebound=New(root,"rebound");var rv=Values(rebound);
        rv.actions[(int)Idas3ControlBindings.ActionId.Pause].pad=Idas3ControlBindings.PadInput.None;
        rv.actions[(int)Idas3ControlBindings.ActionId.Accelerate].pad=Idas3ControlBindings.PadInput.Start;
        Check(Idas3ControlBindings.Validate(rv,out _),"Fixture rebound pad invalid");
        rebound.Poll(_=>false,startPad,28);var rf=Route(rebound,startPad);
        Check((rf.flags&Start)==0&&rf.rightTrigger==255&&!Key(rf,27),"Rebound driving action gained cabinet/Pause ownership");
        defaults.ControllerDeviceChanged();defaults.Poll(_=>false,startPad,29);
        Check((Route(defaults,startPad).flags&Start)==0&&!Key(Route(defaults,startPad),27),"Reconnect converted held Start to cabinet/Pause");
        defaults.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true},30);defaults.Poll(_=>false,startPad,31);
        Check(Key(Route(defaults,startPad),27)&&(Route(defaults,startPad).flags&Start)==0,"Reconnected Pause did not recover after release");
        var occupied=New(root,"occupied-click");
        Values(occupied).actions[(int)Idas3ControlBindings.ActionId.Headlights].pad=Idas3ControlBindings.PadInput.LeftThumb;
        var click=new Idas3ControlBindings.PadState{connected=true,buttons=0x40};
        occupied.Poll(_=>false,click,31);
        Check((Route(occupied,click).flags&Start)==0&&Key(Route(occupied,click),'H'),"Race Start stole a bound stick click");
        defaults.Poll(_=>false,click,31);defaults.BlockStartUntilRelease();
        Check(!defaults.RaceCabinetStartHeld,"Held race Start bypassed release guard");
        defaults.Poll(_=>false,default,31);defaults.Poll(_=>false,click,31);
        Check(defaults.RaceCabinetStartHeld&&!defaults.CabinetStartHeld,"Race Start did not rearm independently of menu Start");
        var wheel=New(root,"generic");wheel.SelectControllerProfile("private-fq-wheel","Private Forced Quit wheel",true);
        var w=Values(wheel);w.actions[(int)Idas3ControlBindings.ActionId.Start]=new Idas3ControlBindings.Binding{
            controlPath="start",controlButton=true,controlDirection=1,controlMin=0,controlMax=1};
        w.actions[(int)Idas3ControlBindings.ActionId.Pause]=new Idas3ControlBindings.Binding{
            key1=KeyCode.Escape,controlPath="pause",controlButton=true,controlDirection=1,controlMin=0,controlMax=1};
        var s=new Idas3ControllerControl{path="start",button=true,minimum=0,maximum=1,value=0,fallbackButtons=0x10};
        var p=new Idas3ControllerControl{path="pause",button=true,minimum=0,maximum=1,value=0,fallbackButtons=0x20};
        Check(Idas3ControlBindings.Validate(w,out _),"Fixture generic controls invalid");
        wheel.Poll(_=>false,default,32,new[]{s,p});
        for(int mask=0;mask<4;++mask){
            s.value=mask&1;p.value=(mask>>1)&1;wheel.Poll(_=>false,startPad,33+mask,new[]{s,p});
            var f=Route(wheel,startPad,generic:true);
            Check(((f.flags&Start)!=0)==((mask&1)!=0),"Generic configured Start mapping");
            Check(Key(f,27)==((mask&2)!=0),"Generic independent Pause/source alias mapping");
        }
        // Valid profiles cannot share Start/Pause source identity; separate
        // devices may legitimately have independently held Start and Pause.
        var duplicate=w.Clone();duplicate.actions[(int)Idas3ControlBindings.ActionId.Pause].controlPath="start";
        Check(!Idas3ControlBindings.Validate(duplicate,out _),"Duplicate generic Start/Pause identity accepted");
        s.value=p.value=0;wheel.Poll(_=>false,default,38,new[]{s,p});
        var profiles=(Dictionary<string,Idas3ControlBindings.ControllerProfile>)typeof(Idas3ControlBindings).GetField("savedProfiles",Private).GetValue(wheel);
        var aux=Idas3ControlBindings.Defaults().actions;for(int n=0;n<aux.Length;++n)aux[n]=new Idas3ControlBindings.Binding();
        aux[(int)Idas3ControlBindings.ActionId.Start]=new Idas3ControlBindings.Binding{controlPath="start",controlButton=true,controlDirection=1,controlMin=0,controlMax=1};
        aux[(int)Idas3ControlBindings.ActionId.Pause]=new Idas3ControlBindings.Binding{controlPath="pause",controlButton=true,controlDirection=1,controlMin=0,controlMax=1};
        profiles.Add("private-fq-rig",new Idas3ControlBindings.ControllerProfile{key="private-fq-rig",label="Private rig",generic=true,actions=aux});
        var rs=new Idas3ControllerControl{path="start",button=true,minimum=0,maximum=1,value=0};
        var rp=new Idas3ControllerControl{path="pause",button=true,minimum=0,maximum=1,value=0};
        var rig=new Idas3ControllerDevices.RigSample{key="fq-rig-device",profile="private-fq-rig",controls=new[]{rs,rp}};
        wheel.Poll(_=>false,default,39,new[]{s,p},new[]{rig});
        for(int mask=0;mask<4;++mask){
            rs.value=mask&1;rp.value=(mask>>1)&1;wheel.Poll(_=>false,default,40+mask,new[]{s,p},new[]{rig});
            var f=Route(wheel,generic:true);
            Check(((f.flags&Start)!=0)==((mask&1)!=0),"Independent rig Start mapping");
            Check(Key(f,27)==((mask&2)!=0),"Independent rig Pause mapping");
        }
        Check(!Directory.Exists(Path.Combine(root,"forced-quit-defaults")),"Private input fixture wrote controls");
        File.WriteAllText(Path.Combine(root,"forced-quit-input-checks.txt"),"PASS "+checks+" actual managed cabinet/Pause/View ownership checks; source counter timing is tested independently.\n");
        return checks;
    }
}
#endif
