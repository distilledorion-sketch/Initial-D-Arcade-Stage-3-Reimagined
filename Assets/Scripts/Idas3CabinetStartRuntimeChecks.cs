#if UNITY_EDITOR
using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.LowLevel;

// Exercise the real binding/final-ownership methods with isolated values and
// synthetic devices. Native accepted-update edge timing has its own fixture.
internal static class Idas3CabinetStartRuntimeChecks
{
    const uint HeldFlag=1u<<14,Finish=0x10000010u;
    static int checks;
    static readonly BindingFlags Private=BindingFlags.Instance|BindingFlags.NonPublic;
    static void Check(bool ok,string why){++checks;if(!ok)throw new Exception(why);}
    static Idas3ControlBindings New(string root,string name)
    {var b=new Idas3ControlBindings();b.Initialize(Path.Combine(root,"cabinet-start-"+name));return b;}
    static Idas3ControlBindings.Values Values(Idas3ControlBindings b)=>(Idas3ControlBindings.Values)
        typeof(Idas3ControlBindings).GetField("current",Private).GetValue(b);
    static Idas3Native.FrameInput Route(Idas3ControlBindings b,Idas3ControlBindings.PadState pad=default,
        uint flags=1,uint status=Finish,int phase=3,bool pause=false,bool online=false,bool generic=false)
    {
        var frame=new Idas3Native.FrameInput{size=88,flags=flags,padConnected=pad.connected?1u:0u,padButtons=pad.buttons,
            leftTrigger=pad.leftTrigger,rightTrigger=pad.rightTrigger,thumbLX=pad.thumbLX,thumbLY=pad.thumbLY};
        Idas3SceneGame.ApplyControlBindings(ref frame,pad,b,status,phase,pause,online,generic);
        return frame;
    }
    static bool Start(Idas3Native.FrameInput f)=>(f.flags&HeldFlag)!=0;
    static bool Key(Idas3Native.FrameInput f,int key)
        =>(((uint)typeof(Idas3Native.FrameInput).GetField("key"+(key/32)).GetValue(f))&(1u<<(key%32)))!=0;
    static void Neutralize(ref Idas3Native.FrameInput f)
    {
        object[] args={f};typeof(Idas3SceneGame).GetMethod("NeutralizeControls",BindingFlags.Static|BindingFlags.NonPublic).Invoke(null,args);
        f=(Idas3Native.FrameInput)args[0];
    }
    public static int Run(string root)
    {
        Idas3StartBindingChecks.Run(); // Existing configured-Start regressions remain part of this run.
        checks=0;
        Check(Marshal.SizeOf<Idas3Native.FrameInput>()==88,"Cabinet Start changed the stable ABI");
        var defaults=New(root,"defaults");
        foreach(var key in new[]{KeyCode.None,KeyCode.Return,KeyCode.KeypadEnter,KeyCode.Escape,KeyCode.W,KeyCode.E,KeyCode.C}){
            defaults.Poll(k=>k==key,default,0);
            bool expected=key==KeyCode.Return||key==KeyCode.KeypadEnter;
            Check(!defaults.StartHeld,"Cabinet default changed configured-only public Start");
            Check(defaults.CabinetStartHeld==expected,"Wrong default keyboard source "+key);
            foreach(var sample in new[]{new{status=16u,phase=0},new{status=16u,phase=1},new{status=16u,phase=2},new{status=Finish,phase=3},new{status=1u,phase=3}})
                Check(Start(Route(defaults,status:sample.status,phase:sample.phase))==expected,"Start changed across phase "+sample.phase);
        }
        defaults.Poll(k=>k==KeyCode.Return,default,1);
        defaults.BlockStartUntilRelease();defaults.Poll(k=>k==KeyCode.Return,default,2);
        Check(!Start(Route(defaults)),"Covered default Enter retriggered without release");
        defaults.Poll(_=>false,default,3);defaults.Poll(k=>k==KeyCode.Return,default,4);
        Check(Start(Route(defaults)),"Default Enter did not recover after physical release");
        // A source pulse without any submitted frame cannot remain queued.
        defaults.Poll(_=>false,default,5);defaults.Poll(k=>k==KeyCode.Return,default,6);defaults.Poll(_=>false,default,7);
        Check(!Start(Route(defaults)),"Zero-submission press/release queued Start");

        var configured=New(root,"configured-key");
        Values(configured).actions[(int)Idas3ControlBindings.ActionId.Start].key1=KeyCode.K;
        foreach(var key in new[]{KeyCode.K,KeyCode.Return,KeyCode.KeypadEnter,KeyCode.Escape,KeyCode.W}){
            configured.Poll(k=>k==key,default,8);
            Check(Start(Route(configured))==(key==KeyCode.K),"Configured Start failed or admitted fallback "+key);
        }
        configured.Poll(k=>k==KeyCode.K,default,9);
        for(uint gate=0;gate<4;++gate)
            Check(Start(Route(configured,flags:gate))==(gate==1),"Focus/block gate leaked Start "+gate);
        Check(!Start(Route(configured,pause:true))&&!Start(Route(configured,online:true)),"Open overlay admitted cabinet Start");
        var held=Route(configured);Neutralize(ref held);
        Check(!Start(held)&&(held.flags&2u)!=0,"Final modal neutralizer left Start held");
        for(int i=0;i<8;++i)Check(Start(Route(configured)),"Reading held Start consumed it on a host frame");
        configured.BlockStartUntilRelease();configured.Poll(k=>k==KeyCode.K,default,10);
        Check(!Start(Route(configured)),"Configured Start bypassed release guard");
        configured.Poll(_=>false,default,11);configured.Poll(k=>k==KeyCode.K,default,12);
        Check(Start(Route(configured)),"Configured Start stayed blocked after release");
        // SceneGame does not poll hardware while unfocused. On return it
        // polls once, then blocks every still-held Start source before routing.
        configured.BlockStartUntilRelease();
        Check(!Start(Route(configured,flags:0)),"Unfocused packet retained Start");
        configured.Poll(k=>k==KeyCode.K,default,12.1);configured.BlockStartUntilRelease();
        Check(!Start(Route(configured)),"Refocus synthesized a Start edge");
        configured.Poll(_=>false,default,12.2);configured.Poll(k=>k==KeyCode.K,default,12.3);
        Check(Start(Route(configured)),"Refocus guard failed to release");
        var router=new Idas3MenuInputRouter();router.VisibilityChanged(false,10);
        router.Process(new Idas3MenuInputRouter.Sample{Escape=true},new Idas3MenuInputRouter.Context{Focused=true});
        var closing=Route(configured);closing.SetKey('E');closing.padButtons|=0x2000u;
        Idas3SceneGame.ApplyPauseOwnership(ref closing,router,false,false,12);
        Check(!Start(closing)&&(closing.flags&2u)==0&&Key(closing,'E')&&(closing.padButtons&0x2000u)!=0,
            "Closing Start suppression changed unrelated driving or leaked Start");
        router.Process(default,new Idas3MenuInputRouter.Context{Focused=true});closing=Route(configured);
        Idas3SceneGame.ApplyPauseOwnership(ref closing,router,false,false,13);
        Check(Start(closing),"Closing release guard consumed later independent Start");

        Gamepad device=null;
        try{
            device=InputSystem.AddDevice<Gamepad>();
            foreach(ushort buttons in new ushort[]{0,0x10,0x1000,0x2000,0x4000,0x8000}){
                var state=new GamepadState().WithButton(GamepadButton.Start,(buttons&0x10)!=0)
                    .WithButton(GamepadButton.South,(buttons&0x1000)!=0).WithButton(GamepadButton.East,(buttons&0x2000)!=0)
                    .WithButton(GamepadButton.West,(buttons&0x4000)!=0).WithButton(GamepadButton.North,(buttons&0x8000)!=0);
                InputState.Change(device,state);
                var pad=Idas3GamepadInput.ReadUnityPad(device);defaults.Poll(_=>false,pad,13);
                Check(Start(Route(defaults,pad))==(buttons==0x10),"Physical gamepad alias became Start "+buttons);
                configured.Poll(_=>false,pad,14);
                Check(!Start(Route(configured,pad)),"Explicit keyboard Start retained physical fallback");
            }
        }finally{if(device!=null)InputSystem.RemoveDevice(device);}
        var startPad=new Idas3ControlBindings.PadState{connected=true,buttons=0x10};
        defaults.Poll(_=>false,startPad,15);defaults.ControllerDeviceChanged();defaults.Poll(_=>false,startPad,16);
        Check(!Start(Route(defaults,startPad)),"Reconnect held default Start became new input");
        defaults.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true},17);defaults.Poll(_=>false,startPad,18);
        Check(Start(Route(defaults,startPad)),"Default Start failed after reconnect/release");
        var rebound=New(root,"rebound-pad");
        Values(rebound).actions[(int)Idas3ControlBindings.ActionId.Accelerate].pad=Idas3ControlBindings.PadInput.Start;
        rebound.Poll(_=>false,startPad,19);var reboundFrame=Route(rebound,startPad);
        Check(!Start(reboundFrame)&&reboundFrame.rightTrigger==255,"Rebound throttle also triggered Start");
        var custom=New(root,"custom-pad");
        Values(custom).actions[(int)Idas3ControlBindings.ActionId.Start].pad=Idas3ControlBindings.PadInput.A;
        var aPad=new Idas3ControlBindings.PadState{connected=true,buttons=0x1000};
        custom.Poll(_=>false,aPad,20);var customFrame=Route(custom,aPad);
        Check(Start(customFrame)&&(customFrame.padButtons&0x1000u)==0,"Configured A Start retained menu-confirm alias");

        var wheel=New(root,"generic");wheel.SelectControllerProfile("private-wheel","Private wheel",true);
        var w=Values(wheel);w.actions[0]=new Idas3ControlBindings.Binding{controlPath="throttle",controlButton=true,controlDirection=1,controlMin=0,controlMax=1};
        var throttle=new Idas3ControllerControl{path="throttle",button=true,minimum=0,maximum=1,value=0,fallbackButtons=0x10};
        wheel.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true},21,new[]{throttle});
        throttle.value=1;wheel.Poll(_=>false,startPad,22,new[]{throttle});
        var menu=Route(wheel,startPad,status:1,phase:3,generic:true);
        Check(Key(menu,13)&&!Start(menu),"Generic pedal synthesized a cabinet Start through menu Enter");
        var rawFallback=Route(wheel,startPad,generic:true);
        Check(!Start(rawFallback),"Generic fallback Start bit bypassed configured actions");
        w.actions[(int)Idas3ControlBindings.ActionId.Start]=new Idas3ControlBindings.Binding{
            controlPath="start-button",controlButton=true,controlDirection=1,controlMin=0,controlMax=1};
        var button=new Idas3ControllerControl{path="start-button",button=true,minimum=0,maximum=1,value=0,fallbackButtons=0x1000};
        throttle.value=0;wheel.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true},23,new[]{throttle,button});
        button.value=1;wheel.Poll(_=>false,aPad,24,new[]{throttle,button});
        Check(Start(Route(wheel,aPad,generic:true)),"Configured generic Start did not reach dedicated flag");
        wheel.BlockStartUntilRelease();wheel.Poll(_=>false,aPad,25,new[]{throttle,button});
        Check(!Start(Route(wheel,aPad,generic:true)),"Covered generic Start bypassed guard");
        button.value=0;wheel.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true},26,new[]{throttle,button});
        button.value=1;wheel.Poll(_=>false,aPad,27,new[]{throttle,button});
        Check(Start(Route(wheel,aPad,generic:true)),"Generic Start failed after release");
        wheel.ControllerDeviceChanged();wheel.Poll(_=>false,aPad,28,new[]{throttle,button});
        Check(!Start(Route(wheel,aPad,generic:true)),"Reconnected generic Start became a new press");
        button.value=0;wheel.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true},29,new[]{throttle,button});
        button.value=1;wheel.Poll(_=>false,aPad,30,new[]{throttle,button});
        Check(Start(Route(wheel,aPad,generic:true)),"Generic Start did not rearm after reconnect/release");
        var profiles=(Dictionary<string,Idas3ControlBindings.ControllerProfile>)typeof(Idas3ControlBindings).GetField("savedProfiles",Private).GetValue(wheel);
        var aux=Idas3ControlBindings.Defaults().actions;
        for(int i=0;i<aux.Length;++i)aux[i]=new Idas3ControlBindings.Binding();
        aux[(int)Idas3ControlBindings.ActionId.Start]=new Idas3ControlBindings.Binding{controlPath="aux-start",controlButton=true,controlDirection=1,controlMin=0,controlMax=1};
        profiles.Add("private-aux",new Idas3ControlBindings.ControllerProfile{key="private-aux",label="Private auxiliary",generic=true,actions=aux});
        var auxButton=new Idas3ControllerControl{path="aux-start",minimum=0,maximum=1,button=true,value=1};
        var rig=new Idas3ControllerDevices.RigSample{key="aux-physical",profile="private-aux",controls=new[]{auxButton}};
        button.value=0;wheel.Poll(_=>false,default,31,new[]{throttle,button},new[]{rig});
        Check(!Start(Route(wheel,generic:true)),"Fresh held rig Start bypassed reconnect guard");
        auxButton.value=0;wheel.Poll(_=>false,default,32,new[]{throttle,button},new[]{rig});
        auxButton.value=1;wheel.Poll(_=>false,default,33,new[]{throttle,button},new[]{rig});
        Check(Start(Route(wheel,generic:true)),"Independent rig Start failed after release");
        wheel.BlockStartUntilRelease();wheel.Poll(_=>false,default,34,new[]{throttle,button},new[]{rig});
        Check(!Start(Route(wheel,generic:true)),"Held rig Start bypassed modal guard");
        wheel.Poll(_=>false,default,35,new[]{throttle,button});wheel.Poll(_=>false,default,36,new[]{throttle,button},new[]{rig});
        Check(!Start(Route(wheel,generic:true)),"Reconnected held rig Start retriggered");
        auxButton.value=0;wheel.Poll(_=>false,default,37,new[]{throttle,button},new[]{rig});
        auxButton.value=1;wheel.Poll(_=>false,default,38,new[]{throttle,button},new[]{rig});
        Check(Start(Route(wheel,generic:true)),"Rig Start failed after second release");
        Check(!Directory.Exists(Path.Combine(root,"cabinet-start-defaults")),"Input fixture wrote personal controls");
        File.WriteAllText(Path.Combine(root,"cabinet-start-checks.txt"),"PASS "+checks+" cabinet Start routing checks; virtual input only, no native/source-clock or physical timing claim.\n");
        return checks;
    }
}
#endif
