using System;
using System.IO;
using UnityEngine;

// Mouse steering virtual wheel, its driving packet, mouse-button bindings,
// the left-click latch and the steering aid's HUD placement.
public static class Idas3MouseSteeringChecks
{
    public static void Run(){
        string root=Path.GetFullPath("Verification/mouse-steering/bindings-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(root);
        int checks=0;void Check(bool ok,string why){++checks;if(!ok)throw new Exception(why);}
        bool Near(float a,float b)=>Mathf.Abs(a-b)<.0001f;

        Check(Near(Idas3MouseSteering.CountsToFullLock(0),4000)&&Near(Idas3MouseSteering.CountsToFullLock(1),200),"Sensitivity endpoints changed");
        Check(Idas3MouseSteering.CountsToFullLock(.5f)<Idas3MouseSteering.CountsToFullLock(.4f),"Higher sensitivity did not steer faster");
        var wheel=new Idas3MouseSteering();
        wheel.Advance(450,1);Check(Near(wheel.Position,1),"Full lock was not clamped");
        wheel.Advance(-100,1);Check(Near(wheel.Position,.5f),"Delta X did not move the virtual wheel");
        wheel.Advance(0,1);Check(Near(wheel.Position,.5f),"A still mouse recentred the wheel");
        wheel.Advance(float.NaN,1);wheel.Advance(float.PositiveInfinity,1);Check(Near(wheel.Position,.5f),"Nonfinite delta moved the wheel");
        wheel.Advance(-10000,0);Check(Near(wheel.Position,-1),"Opposite full lock was not clamped");
        wheel.Reset();Check(wheel.Position==0,"Reset did not centre the wheel");
        Check(Idas3MouseSteering.Axis(1)==32767&&Idas3MouseSteering.Axis(-1)==-32767&&Idas3MouseSteering.Axis(float.NaN)==0,"Virtual axis range changed");

        var map=new Idas3ControlBindings();map.Initialize(root);
        Idas3Native.FrameInput Drive(KeyCode key=KeyCode.None,Idas3ControlBindings.PadState pad=default){
            map.Poll(k=>k==key,pad,0);var f=new Idas3Native.FrameInput{flags=1};map.ApplyDriving(ref f);return f;
        }
        map.MouseSteering=.5f;var frame=Drive();
        Check(frame.padConnected==1&&frame.thumbLX==Idas3MouseSteering.Axis(.5f)&&frame.thumbLY==0,"Mouse steering without a controller did not reach native");
        Check(((frame.flags>>11)&7)==3,"Mouse steering was not labelled analog");
        var stick=new Idas3ControlBindings.PadState{connected=true,thumbLY=9000};
        frame=Drive(KeyCode.None,stick);Check(frame.thumbLX==Idas3MouseSteering.Axis(.5f)&&frame.thumbLY==0,"Mouse steering lost to a centred stick");
        stick.thumbLX=-20000;frame=Drive(KeyCode.None,stick);Check(frame.thumbLX==-20000&&map.OtherSteeringHeld,"A turned stick did not take over from the mouse");
        Drive(KeyCode.A);Check(map.OtherSteeringHeld,"Keyboard steering did not take over from the mouse");
        Drive();Check(!map.OtherSteeringHeld,"Idle controls blocked mouse steering");
        map.MouseSteering=float.NaN;frame=Drive();Check(frame.padConnected==0&&frame.thumbLX==0&&((frame.flags>>11)&7)==0,"Mouse steering leaked into another profile");

        map.BeginEdit();
        Check(map.TrySetDraftKey(Idas3ControlBindings.ActionId.Accelerate,Idas3ControlBindings.Slot.Extra,KeyCode.Mouse0),"Left click could not be bound");
        Check(!map.TrySetDraftKey(Idas3ControlBindings.ActionId.Brake,Idas3ControlBindings.Slot.Extra,KeyCode.Mouse2),"Middle click was accepted");
        foreach(var menuAction in new[]{Idas3ControlBindings.ActionId.Pause,Idas3ControlBindings.ActionId.Online,Idas3ControlBindings.ActionId.Start})
            Check(!map.TrySetDraftKey(menuAction,Idas3ControlBindings.Slot.Extra,KeyCode.Mouse1)&&map.LastError=="Mouse buttons can't open menus.","A mouse button was bound to a menu action");
        Check(map.ApplyDraft(),"Mouse binding did not save");
        Check(Idas3ControlBindings.KeyName(KeyCode.Mouse0)=="Left Click"&&Idas3ControlBindings.KeyName(KeyCode.Mouse1)=="Right Click","Mouse button names changed");
        var reloaded=new Idas3ControlBindings();reloaded.Initialize(root);
        Check(reloaded.LastError==null&&reloaded.BindingName(Idas3ControlBindings.ActionId.Accelerate,Idas3ControlBindings.Slot.Extra,false)=="Left Click","Saved mouse binding did not load");
        frame=Drive(KeyCode.Mouse0);Check(((frame.key2>>23)&1)==1,"Left click did not accelerate");

        // Capture accepts a click once the opening input is released.
        map.BeginEdit();map.BeginCapture(Idas3ControlBindings.ActionId.Brake,Idas3ControlBindings.Slot.Extra,0);
        map.Poll(k=>k==KeyCode.Mouse0,default,0);map.Poll(k=>false,default,0);map.Poll(k=>k==KeyCode.Mouse1,default,0);
        Check(!map.IsCapturing&&map.BindingName(Idas3ControlBindings.ActionId.Brake,Idas3ControlBindings.Slot.Extra)=="Right Click","Right click capture failed");
        map.CancelEdit(false);

        // The latch only applies to a bound left click held while driving.
        Check(map.LatchLeftClick(true,true),"Bound left click held while driving was not latched");
        Check(map.LatchLeftClick(true,false),"Latch released before the click did");
        Check(!map.LatchLeftClick(false,false)&&!map.LatchLeftClick(true,false),"Latch outlived the release or caught a menu click");
        var unbound=new Idas3ControlBindings();unbound.Initialize(Path.Combine(root,"unbound"));
        Check(!unbound.LatchLeftClick(true,true),"An unbound left click was latched");

        // The aid turns through half its lock-to-lock range each way.
        var values=new Idas3GameOptions.Values();
        Check(values.mouseSteeringAidRange==540,"Steering aid range default changed");
        Check(Near(Idas3MouseSteeringAid.RotationDegrees(1,540),270)&&Near(Idas3MouseSteeringAid.RotationDegrees(-.5f,540),-135)&&
            Near(Idas3MouseSteeringAid.RotationDegrees(2,180),90)&&Idas3MouseSteeringAid.RotationDegrees(float.NaN,900)==0,"Steering aid rotation ignored its range");
        // The aid is a HUD group: offset moves it and size scales it.
        var rect=Idas3MouseSteeringAid.ScreenRect(values,1280,720);
        Check(Near(rect.center.x,640)&&rect.yMax<=720&&rect.width>0&&Near(rect.width,rect.height),"Aid default placement changed");
        values.SetHudSizePercent(Idas3MouseSteeringAid.HudGroup,150);values.SetHudOffset(Idas3MouseSteeringAid.HudGroup,new Vector2(.25f,-.1f));
        var moved=Idas3MouseSteeringAid.ScreenRect(values,1280,720);
        Check(Near(moved.width,rect.width*1.5f)&&Near(moved.center.x,640+320),"Aid ignored its HUD size or offset");
        File.WriteAllText(Path.Combine(root,"PASS.txt"),checks+" mouse steering checks passed");Debug.Log("PASS "+checks+" mouse steering checks");
    }
}
