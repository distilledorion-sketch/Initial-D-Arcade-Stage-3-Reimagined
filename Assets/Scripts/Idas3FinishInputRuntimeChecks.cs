#if UNITY_EDITOR
using System;
using System.IO;
using System.Reflection;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.LowLevel;

// Uses the same route invoked by SceneGame.Update,
// real bindings and synthetic Unity input. Never reads a physical device.
internal static class Idas3FinishInputRuntimeChecks
{
    const uint FinishControls=0x10000000u,Original=16u,Results=262144u;
    static int checks;
    static void Check(bool ok,string why){++checks;if(!ok)throw new Exception(why);}
    static bool Key(Idas3Native.FrameInput frame,int key)
    {
        var field=typeof(Idas3Native.FrameInput).GetField("key"+(key/32));
        return (((uint)field.GetValue(frame))&(1u<<(key%32)))!=0;
    }
    static Idas3ControlBindings New(string root,string name)
    {var result=new Idas3ControlBindings();result.Initialize(Path.Combine(root,name));return result;}
    static Idas3ControlBindings.Values Values(Idas3ControlBindings bindings)=>(Idas3ControlBindings.Values)
        typeof(Idas3ControlBindings).GetField("current",BindingFlags.NonPublic|BindingFlags.Instance).GetValue(bindings);
    static Idas3Native.FrameInput Frame(Idas3ControlBindings.PadState pad)=>new Idas3Native.FrameInput{
        size=88,flags=1,padConnected=pad.connected?1u:0u,padButtons=pad.buttons,
        leftTrigger=pad.leftTrigger,rightTrigger=pad.rightTrigger,thumbLX=pad.thumbLX,thumbLY=pad.thumbLY};
    static Idas3Native.FrameInput KeyboardFrame(Func<KeyCode,bool> held)
    {
        // Match the real raw keyboard collector. Arbitrary remapped keys are
        // sampled by bindings, not inserted as extra native virtual keys.
        var frame=Frame(default);var fields=BindingFlags.NonPublic|BindingFlags.Static;
        var keys=(KeyCode[])typeof(Idas3SceneGame).GetField("keys",fields).GetValue(null);
        var codes=(int[])typeof(Idas3SceneGame).GetField("virtualKeys",fields).GetValue(null);
        for(int i=0;i<keys.Length;++i)if(keys[i]!=KeyCode.F1&&held(keys[i]))frame.SetKey(codes[i]);
        return frame;
    }
    static Idas3PrecisePedals.Sample Route(ref Idas3Native.FrameInput frame,Idas3ControlBindings.PadState pad,
        Idas3ControlBindings bindings,uint flags,int phase,bool generic,bool pause=false,bool online=false)=>
        Idas3SceneGame.ApplyControlBindings(ref frame,pad,bindings,flags,phase,pause,online,generic);
    static void SameDriving(Idas3Native.FrameInput race,Idas3PrecisePedals.Sample racePedals,
        Idas3Native.FrameInput finish,Idas3PrecisePedals.Sample finishPedals,string name)
    {
        Check(race.thumbLX==finish.thumbLX&&race.thumbLY==finish.thumbLY,name+" steering changed at finish");
        Check(race.leftTrigger==finish.leftTrigger&&race.rightTrigger==finish.rightTrigger,name+" pedals changed at finish");
        Check(race.padButtons==finish.padButtons&&race.flags==finish.flags&&race.padConnected==finish.padConnected,name+" button/source mapping changed at finish");
        for(int key=0;key<256;++key)Check(Key(race,key)==Key(finish,key),name+" canonical keyboard mapping changed at finish");
        var expected=Idas3PrecisePedals.ForSubmission(race,racePedals,false);
        var actual=Idas3PrecisePedals.ForSubmission(finish,finishPedals,false);
        Check(expected.validMask==actual.validMask&&expected.throttleAdc==actual.throttleAdc&&expected.brakeAdc==actual.brakeAdc,name+" raw ADC changed at finish");
    }
    public static int Run(string root)
    {
        checks=0;
        var wheel=New(root,"finish-wheel");wheel.SelectControllerProfile("synthetic-wheel","Synthetic wheel",true);
        var values=Values(wheel);
        values.actions[0]=new Idas3ControlBindings.Binding{controlPath="gas",controlMin=-1,controlMax=1,controlRest=1,controlDirection=-1};
        values.actions[1]=new Idas3ControlBindings.Binding{controlPath="brake",controlMin=0,controlMax=1,controlRest=0,controlDirection=1};
        values.actions[2]=new Idas3ControlBindings.Binding{controlPath="wheel",controlMin=-1,controlMax=1,controlRest=0,controlDirection=-1};
        values.actions[3]=new Idas3ControlBindings.Binding{controlPath="wheel",controlMin=-1,controlMax=1,controlRest=0,controlDirection=1};
        values.actions[4]=new Idas3ControlBindings.Binding{controlPath="up",controlMin=0,controlMax=1,controlRest=0,controlDirection=1,controlButton=true};
        values.actions[5]=new Idas3ControlBindings.Binding{controlPath="down",controlMin=0,controlMax=1,controlRest=0,controlDirection=1,controlButton=true};
        var gas=new Idas3ControllerControl{path="gas",minimum=-1,maximum=1,value=1};
        var brake=new Idas3ControllerControl{path="brake",minimum=0,maximum=1,value=0};
        var steering=new Idas3ControllerControl{path="wheel",minimum=-1,maximum=1,value=0};
        var up=new Idas3ControllerControl{path="up",minimum=0,maximum=1,value=0,button=true};
        var down=new Idas3ControllerControl{path="down",minimum=0,maximum=1,value=0,button=true};
        var controls=new[]{gas,brake,steering,up,down};var pad=new Idas3ControlBindings.PadState{connected=true};
        wheel.Poll(_=>false,pad,0,controls);
        gas.value=.5f;brake.value=.4f;steering.value=.7f;up.value=1;
        wheel.Poll(_=>false,pad,1,controls);
        var race=Frame(pad);var racePedals=Route(ref race,pad,wheel,Original,2,true);
        Check(race.thumbLX>0&&race.thumbLX<32767&&race.rightTrigger>0&&race.rightTrigger<255,"Synthetic wheel fixture did not produce partial analog controls");
        Check((race.padButtons&0x2000u)!=0&&!Key(race,38),"Synthetic shift-up fixture did not use driving mapping");
        var finish=Frame(pad);var finishPedals=Route(ref finish,pad,wheel,Original|FinishControls,3,true);
        SameDriving(race,racePedals,finish,finishPedals,"Wheel");
        // Old DLLs do not advertise this capability. Result/Continue owners
        // retain their existing wheel navigation and do not inherit driving.
        foreach(uint flags in new[]{Original,Original|Results}){
            var menu=Frame(pad);Route(ref menu,pad,wheel,flags,3,true);
            Check(menu.thumbLX==0&&Key(menu,38)&&Key(menu,39),"Result/legacy menu navigation changed");
            Check((menu.padButtons&0x2000u)==0,"Driving shift alias leaked into result cancellation");
        }
        foreach(uint block in new[]{1u,32u,512u,1024u,4096u}){
            var blocked=Frame(pad);Route(ref blocked,pad,wheel,Original|FinishControls|block,3,true);
            Check(blocked.thumbLX==0&&Key(blocked,38),"Finish capability bypassed a native menu owner");
        }
        for(int mode=0;mode<2;++mode){var blocked=Frame(pad);Route(ref blocked,pad,wheel,Original|FinishControls,3,true,mode==0,mode==1);
            Check(blocked.thumbLX==0&&Key(blocked,38),"Finish capability bypassed a host overlay");}

        var keyboard=New(root,"finish-keyboard");var keyValues=Values(keyboard);
        keyValues.actions[0]=new Idas3ControlBindings.Binding{key1=KeyCode.I};
        keyValues.actions[3]=new Idas3ControlBindings.Binding{key1=KeyCode.L};
        keyValues.actions[4]=new Idas3ControlBindings.Binding{key1=KeyCode.U};
        keyboard.Poll(_=>false,default,0);
        keyboard.Poll(k=>k==KeyCode.I||k==KeyCode.L||k==KeyCode.U,default,1);
        race=KeyboardFrame(k=>k==KeyCode.I||k==KeyCode.L||k==KeyCode.U);
        finish=race;racePedals=Route(ref race,default,keyboard,Original,2,false);
        finishPedals=Route(ref finish,default,keyboard,Original|FinishControls,3,false);
        SameDriving(race,racePedals,finish,finishPedals,"Remapped keyboard");
        Check(Key(finish,'W')&&Key(finish,'D')&&Key(finish,'E')&&!Key(finish,'I')&&!Key(finish,'U'),"Remapped keyboard did not reach canonical driving controls");

        var remapped=New(root,"finish-pad");var remap=Values(remapped);
        remap.actions[0].pad=Idas3ControlBindings.PadInput.LeftTrigger;
        remap.actions[1].pad=Idas3ControlBindings.PadInput.RightTrigger;
        remap.actions[4].pad=Idas3ControlBindings.PadInput.RightShoulder;
        remapped.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true},0);
        pad=new Idas3ControlBindings.PadState{connected=true,leftTrigger=64,rightTrigger=128,thumbLX=12000,buttons=0x200,
            preciseTriggers=true,preciseLeftTrigger=.25f,preciseRightTrigger=.5f};
        remapped.Poll(_=>false,pad,1);race=Frame(pad);finish=Frame(pad);
        racePedals=Route(ref race,pad,remapped,Original,2,false);
        finishPedals=Route(ref finish,pad,remapped,Original|FinishControls,3,false);
        SameDriving(race,racePedals,finish,finishPedals,"Remapped gamepad");
        Check(finish.rightTrigger==64&&finish.leftTrigger==128&&(finish.padButtons&0x2000u)!=0,"Remapped pad fixture lost bindings");

        Gamepad device=null;
        try{
            device=InputSystem.AddDevice<Gamepad>();
            foreach(float invalid in new[]{float.NaN,float.PositiveInfinity,float.NegativeInfinity})for(int side=0;side<2;++side){
                InputState.Change(device.leftTrigger,side==0?invalid:.25f);
                InputState.Change(device.rightTrigger,side==1?invalid:.25f);
                var physical=Idas3GamepadInput.ReadUnityPad(device);
                Check((side==0?physical.leftTrigger:physical.rightTrigger)==0,"Invalid physical trigger was not neutralized before legacy conversion");
                Check((side==0?physical.preciseLeftTrigger:physical.preciseRightTrigger)==0,"Invalid physical trigger entered precise sample");
                Check((side==0?physical.preciseRightTrigger:physical.preciseLeftTrigger)==.25f,"Invalid trigger corrupted the other pedal");
                var physicalFrame=Frame(physical);var sample=Idas3PrecisePedals.Physical(physicalFrame,physical);
                Check((side==0?sample.pedals.brakeAdc:sample.pedals.throttleAdc)==0,"Malformed physical report did not reach neutral ADC");
                for(uint flags=0;flags<4;++flags){physicalFrame.flags=flags;
                    Check((Idas3PrecisePedals.ForSubmission(physicalFrame,sample,false).validMask!=0)==(flags==1),"Malformed report defeated final focus/block routing");}
                bool strict=false;try{Idas3PrecisePedals.Adc(invalid);}catch(ArgumentException){strict=true;}
                Check(strict,"Internal ADC validation was relaxed instead of sanitizing physical input");
            }
        }finally{if(device!=null)InputSystem.RemoveDevice(device);}
        Check(Directory.GetFiles(root,"controls.json",SearchOption.AllDirectories).Length==0,"Tests wrote controller preferences");
        return checks;
    }
}
#endif
