using System;
using System.IO;
using UnityEngine;

// Explicit editor entrypoint; never runs on normal game startup.
public static class Idas3ControllerMenuChecks
{
    private static int checks;
    private static void Check(bool value,string reason){++checks;if(!value)throw new Exception(reason);}
    private sealed class Platform:Idas3GameOptions.IPlatform {
        public int Width=>1280;public int Height=>720;public int DisplayMode=>0;public double Now=>0;
        public Idas3GameOptions.ResolutionChoice[] Resolutions=>new[]{new Idas3GameOptions.ResolutionChoice(1280,720)};
        public void Apply(Idas3GameOptions.Values a,Idas3GameOptions.Values b,bool display){}
    }
    public static void Run(){
        checks=0;string root=Path.GetFullPath("Verification/wheel-menu-navigation-20260918/unit-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(root);
        var go=new GameObject("Private menu checks");
        try{
            var options=new Idas3GameOptions(new Platform());options.Initialize(root);
            var bindings=new Idas3ControlBindings();bindings.Initialize(root);
            var menu=go.AddComponent<Idas3PauseMenu>();menu.Initialize(options);menu.InitializeBindings(bindings);menu.OpenAttractOptions();
            Check(menu.CategoryFocused,"Attract category focus");
            for(int tab=0;tab<8;++tab){
                Check(menu.SelectedTab==tab,"Category traversal");menu.Activate();Check(!menu.CategoryFocused,"Enter category");
                menu.Back();Check(menu.IsOpen&&menu.CategoryFocused,"Back preserves settings screen");menu.Navigate(1);
            }
            Check(menu.SelectedTab==0,"Category wrap");menu.Activate();float volume=options.Draft.masterVolume;
            menu.NavigateHorizontal(-1);menu.Back();Check(options.Draft.masterVolume<volume,"Back from fields preserves draft");
            menu.Navigate(1);menu.Activate();menu.Back();menu.Navigate(-1);menu.Activate();Check(options.Draft.masterVolume<volume,"Switch category preserves draft");
            for(int i=0;i<6;++i)menu.Navigate(1);menu.Activate();Check(!options.HasUnsavedChanges&&options.Current.masterVolume<volume,"Apply via menu navigation");
            menu.Back();menu.Back();Check(!menu.IsOpen,"Exit settings");
            menu.SetWheelNavigation(true);menu.OpenAttractOptions();menu.Activate();
            float music=options.Draft.musicVolume;menu.NavigateHorizontal(1);menu.Activate();Check(menu.WheelEditing,"Pedal enters wheel value edit");
            menu.NavigateHorizontal(-1);Check(options.Draft.musicVolume<music,"Wheel edits focused value");
            menu.Back();Check(!menu.WheelEditing&&!menu.CategoryFocused,"Brake leaves edit before category");
            for(int i=0;i<5;++i)menu.NavigateHorizontal(1);menu.Activate();Check(!options.HasUnsavedChanges&&options.Current.musicVolume<music,"Wheel reaches Apply without paddles");
            menu.Back();menu.NavigateHorizontal(1);Check(menu.SelectedTab==1,"Wheel switches category");menu.Back();Check(!menu.IsOpen,"Wheel exits settings");menu.SetWheelNavigation(false);
            menu.OpenAttractOptions();menu.SelectTab(3);menu.SelectBindingColumn(0);menu.Activate();
            Check(menu.BindingChoiceVisible&&!bindings.IsCapturing,"Binding actions accessible before capture");menu.Back();Check(!menu.BindingChoiceVisible,"Controller cancels binding chooser");
            menu.Activate();menu.NavigateHorizontal(1);menu.Activate();Check(!menu.BindingChoiceVisible&&!bindings.IsCapturing,"Controller clears slot without entering capture");
            menu.Activate();menu.Activate();Check(bindings.IsCapturing,"Controller starts selected binding capture");
            bindings.Poll(k=>false,default,Time.realtimeSinceStartupAsDouble+16);Check(!bindings.IsCapturing,"Untouched capture times out without keyboard");menu.SetOpen(false);
            var songs=go.AddComponent<Idas3RaceMusicMenu>();songs.Initialize(new[]{new Idas3RaceMusicMenu.Entry{id=1,title="First",stage=1},new Idas3RaceMusicMenu.Entry{id=2,title="Second",stage=2}},1);songs.SetOpen(true);
            songs.NavigateDevice(1,0,true);Check(songs.HighlightedTrackId==2&&songs.StageFilter==0,"Wheel selects songs rather than only changing stage filter");
            songs.NavigateDevice(0,1,true);Check(songs.StageFilter==1,"Optional wheel paddle changes stage");songs.Back();
            bindings.Poll(k=>false,new Idas3ControlBindings.PadState{connected=true,buttons=0x20},0);Check(bindings.OnlineHeld,"Default Select opens Online");
            bindings.BeginEdit();bindings.TrySetDraftPad(Idas3ControlBindings.ActionId.Online,Idas3ControlBindings.PadInput.None);Check(bindings.ApplyDraft(),"Legacy empty Online binding");
            bindings.Poll(k=>false,new Idas3ControlBindings.PadState{connected=true,buttons=0x20},1);Check(bindings.OnlineHeld,"Existing unbound profile Select fallback");
            bindings.Poll(k=>k==KeyCode.W||k==KeyCode.S||k==KeyCode.A||k==KeyCode.Q,default,2);
            var frame=new Idas3Native.FrameInput{thumbLX=22000,thumbLY=22000};bindings.ApplyMenu(ref frame,true);Check(frame.thumbLX==0&&frame.thumbLY==0,"Bound pedal axes do not also navigate as raw sticks");
            Check((frame.key0&(1u<<13))==0&&(frame.key0&(1u<<8))!=0&&(frame.padButtons&0x2000)!=0&&(frame.key1&(1u<<5))!=0&&(frame.key1&(1u<<8))!=0,"Mapped brake reaches native and managed Back, taking priority over accelerator");
            bindings.BeginCapture(Idas3ControlBindings.ActionId.Camera,Idas3ControlBindings.Slot.Controller,3);frame=default;bindings.ApplyMenu(ref frame,true);Check(frame.key0==0&&frame.key1==0,"Capture blocks menu aliases");bindings.CancelCapture();
            bindings.SelectControllerProfile("private-wheel","Private wheel",true);
            var pedal=new Idas3ControllerControl{path="pedal",label="Pedal",minimum=-1,maximum=1,value=-1};
            var wheel=new Idas3ControllerControl{path="wheel",label="Wheel",minimum=-1,maximum=1};
            bindings.BeginEdit();Check(bindings.TrySetDraftControl(Idas3ControlBindings.ActionId.Accelerate,pedal,1,-1),"Bind wheel pedal");
            Check(bindings.TrySetDraftControl(Idas3ControlBindings.ActionId.SteerRight,wheel,1,0),"Bind wheel steering");Check(bindings.ApplyDraft(),"Save private wheel bindings");
            bindings.Poll(k=>false,default,4,new[]{pedal,wheel});
            pedal.value=1;wheel.value=1;bindings.Poll(k=>false,default,5,new[]{pedal,wheel});frame=default;bindings.ApplyMenu(ref frame,true);
            Check((frame.key0&(1u<<13))!=0&&(frame.key1&(1u<<7))!=0,"Physical wheel control snapshots navigate and confirm");
            var focus=new Idas3MenuFocus();Action build=()=>{focus.Begin();focus.Control("first",new Rect(0,0,30,30),true,true);focus.Control("disabled",new Rect(0,35,30,30),false,true);focus.Control("second",new Rect(0,70,30,30),true,true);focus.End();};build();
            focus.Poll(0,0,true,false,false,0);Check(!focus.Control("first",default,true,true),"Held open confirm is blocked");
            focus.Poll(0,0,false,false,false,1);focus.Poll(0,1,false,false,false,2);Check(focus.Selected=="second","Navigation skips disabled controls");
            focus.Poll(0,0,true,false,false,3);Check(focus.Control("second",default,true,true),"Confirm activates focused control");Check(!focus.Control("second",default,true,true),"Confirm runs once");
            focus.Poll(0,0,false,false,false,4);focus.Poll(0,0,true,false,false,5);focus.Begin();focus.Control("replacement",default,true,true);focus.End();Check(!focus.Control("replacement",default,true,true),"Stale confirmation cannot hit replacement room/action");
            focus.Poll(0,0,false,false,true,6);focus.Poll(0,0,true,false,false,7);Check(!focus.Control("replacement",default,true,true),"Focus return requires neutral release");
            File.WriteAllText(Path.Combine(root,"result.txt"),"PASS "+checks+" checks\n");File.WriteAllText("Verification/wheel-menu-navigation-20260918/unit-result.txt","PASS "+checks+" checks; isolated preferences: "+root+"\n");Debug.Log("PASS controller menu checks "+checks);
        }finally{UnityEngine.Object.DestroyImmediate(go);}
    }
    public static void RunPointerChecks(){
        checks=0;Idas3Native.FrameInput frame=default;
        var focus=new Idas3MenuFocus();Action build=()=>{focus.Begin();focus.Control("first",new Rect(0,0,30,30),true,true);focus.Control("second",new Rect(0,70,30,30),true,true);focus.End();};
            frame=new Idas3Native.FrameInput{padConnected=1};Idas3MenuPointer.ApplyConfirm(ref frame,false,true,false);
            Check((frame.key0&(1u<<13))==0,"Managed mouse click must not confirm controller highlight");
            frame=default;Idas3MenuPointer.ApplyConfirm(ref frame,false,true,false);Check(frame.key0==0,"Managed mouse click without pad stays a pointer");
            Idas3MenuPointer.ApplyConfirm(ref frame,true,false,false);Check((frame.key0&(1u<<13))!=0,"Keypad Enter remains confirm");
            frame=default;Idas3MenuPointer.ApplyConfirm(ref frame,false,true,true);Check((frame.key0&(1u<<13))!=0,"Original native menus retain click confirm");
            var pointer=new Idas3MenuPointer();Check(pointer.BlockNavigation(true,true),"Pointer takes priority over held controller");
            Check(pointer.BlockNavigation(false,true),"Held controller cannot resume after click");Check(pointer.BlockNavigation(false,false),"Neutral releases pointer gate");
            Check(!pointer.BlockNavigation(false,true),"Fresh controller input resumes after neutral");
            focus.Reset();build();focus.Poll(0,0,false,false,false,10);focus.Poll(0,1,false,false,false,11);focus.Poll(0,0,true,false,false,12);
            focus.Pointer("first");Check(focus.Selected=="first"&&!focus.Control("second",default,true,true),"Click takes focus and cancels stale activation");
            focus.Poll(0,0,true,false,false,13);Check(!focus.Control("first",default,true,true),"Held confirm cannot activate mouse selection again");
            focus.Poll(0,0,false,false,false,14);focus.Poll(0,0,true,false,false,15);Check(focus.Control("first",default,true,true),"Fresh confirm activates clicked selection");
            focus.Poll(0,0,false,false,false,16);focus.Poll(0,0,true,false,false,17);focus.Pointer();Check(!focus.Control("first",default,true,true),"Blank pointer click cancels queued confirmation");
        Directory.CreateDirectory("Verification/mouse-menu-20260921");
        File.WriteAllText("Verification/mouse-menu-20260921/unit-result.txt","PASS "+checks+" pointer checks\n");
        Debug.Log("PASS pointer menu checks "+checks);
    }
}
