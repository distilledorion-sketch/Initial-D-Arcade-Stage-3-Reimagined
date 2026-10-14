using System;
using System.IO;
using UnityEngine;

// Explicit editor entrypoint. Historical fixtures describe their own layouts;
// they never derive their lengths or contents from current Defaults().
public static class Idas3StartBindingChecks
{
    static int checks;
    static void Check(bool value,string reason){++checks;if(!value)throw new Exception(reason);}
    sealed class Platform:Idas3GameOptions.IPlatform
    {
        public int Width=>1280;public int Height=>720;public int DisplayMode=>0;public double Now=>0;
        public Idas3GameOptions.ResolutionChoice[] Resolutions=>new[]{new Idas3GameOptions.ResolutionChoice(1280,720)};
        public void Apply(Idas3GameOptions.Values a,Idas3GameOptions.Values b,bool display){}
    }
    static Idas3ControlBindings.Binding[] Historical(int version)
    {
        var B=Idas3ControlBindings.PadInput.None;
        var old=new[]{
            new Idas3ControlBindings.Binding{key1=KeyCode.W,pad=Idas3ControlBindings.PadInput.RightTrigger},
            new Idas3ControlBindings.Binding{key1=KeyCode.S,pad=Idas3ControlBindings.PadInput.LeftTrigger},
            new Idas3ControlBindings.Binding{key1=KeyCode.A,pad=Idas3ControlBindings.PadInput.LeftStickLeft},
            new Idas3ControlBindings.Binding{key1=KeyCode.D,pad=Idas3ControlBindings.PadInput.LeftStickRight},
            new Idas3ControlBindings.Binding{key1=KeyCode.E,pad=Idas3ControlBindings.PadInput.B},
            new Idas3ControlBindings.Binding{key1=KeyCode.Q,pad=Idas3ControlBindings.PadInput.X},
            new Idas3ControlBindings.Binding{key1=KeyCode.C,pad=Idas3ControlBindings.PadInput.Y},
            new Idas3ControlBindings.Binding{key1=KeyCode.Escape,pad=Idas3ControlBindings.PadInput.Start},
            new Idas3ControlBindings.Binding{key1=KeyCode.F1,pad=Idas3ControlBindings.PadInput.Back}
        };
        int count=version<=2?9:version==3?10:16;
        var result=new Idas3ControlBindings.Binding[count];Array.Copy(old,result,9);
        if(count>=10)result[9]=new Idas3ControlBindings.Binding{key1=KeyCode.H,pad=Idas3ControlBindings.PadInput.RightThumb};
        for(int i=10;i<count;++i)result[i]=new Idas3ControlBindings.Binding{pad=B};
        return result;
    }
    public static void Run()
    {
        checks=0;
        string root=Path.Combine(Path.GetTempPath(),"idas3-start-"+Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(root);
        try
        {
            Check((int)Idas3ControlBindings.ActionId.Gear6==15,"Legacy action indexes changed");
            Check((int)Idas3ControlBindings.ActionId.Start==16&&Idas3ControlBindings.ActionCount==17,"Start must append after Gear 6");
            var defaults=Idas3ControlBindings.Defaults();
            Check(defaults.actions[16].key1==KeyCode.None&&defaults.actions[16].key2==KeyCode.None&&
                defaults.actions[16].key3==KeyCode.None&&defaults.actions[16].pad==Idas3ControlBindings.PadInput.None,"Start defaults must be unbound");
            Check(defaults.actions[7].key1==KeyCode.Escape&&defaults.actions[7].pad==Idas3ControlBindings.PadInput.Start,"Pause defaults changed");
            foreach(int version in new[]{1,2,3,4})
            {
                string folder=Path.Combine(root,"v"+version);Directory.CreateDirectory(folder);
                var top=Historical(version);top[4].key2=KeyCode.R;
                var first=Historical(version);first[0].controlPath="button4";first[0].controlLabel="Wheel button";
                first[0].controlButton=true;first[0].controlDirection=1;first[0].controlMin=0;first[0].controlMax=1;
                first[0].pad=Idas3ControlBindings.PadInput.None;
                var second=Historical(version);second[6].key1=KeyCode.None;second[6].pad=Idas3ControlBindings.PadInput.LeftShoulder;
                var saved=new Idas3ControlBindings.Values{version=version,actions=top,controllerProfiles=new[]{
                    new Idas3ControlBindings.ControllerProfile{key="wheel-a",label="Wheel A",generic=true,actions=first},
                    new Idas3ControlBindings.ControllerProfile{key="wheel-b",label="Wheel B",generic=false,actions=second}}};
                File.WriteAllText(Path.Combine(folder,"controls.json"),JsonUtility.ToJson(saved));
                var mapper=new Idas3ControlBindings();mapper.Initialize(folder);
                Check(mapper.LastError==null&&mapper.Current.version==5,"v"+version+" migrated to defaults or failed");
                for(int i=0;i<top.Length;++i)Check(JsonUtility.ToJson(top[i])==JsonUtility.ToJson(mapper.Current.actions[i]),"v"+version+" changed top action "+i);
                Check(mapper.Current.actions[16].pad==Idas3ControlBindings.PadInput.None,"v"+version+" invented Start");
                foreach(var profile in new[]{new{key="wheel-a",label="Wheel A",actions=first,generic=true},new{key="wheel-b",label="Wheel B",actions=second,generic=false}})
                {
                    mapper.SelectControllerProfile(profile.key,profile.label,profile.generic);
                    for(int i=0;i<profile.actions.Length;++i)
                        Check(mapper.Current.actions[i].pad==profile.actions[i].pad&&
                            (mapper.Current.actions[i].controlPath??"")== (profile.actions[i].controlPath??""),
                            "v"+version+" changed profile "+profile.key+" action "+i);
                }
                mapper.BeginEdit();Check(mapper.ApplyDraft(),"v"+version+" save failed");
                var savedAgain=JsonUtility.FromJson<Idas3ControlBindings.Values>(File.ReadAllText(Path.Combine(folder,"controls.json")));
                Check(savedAgain.controllerProfiles.Length==2&&savedAgain.controllerProfiles[0].label=="Wheel A"&&savedAgain.controllerProfiles[0].generic&&
                    savedAgain.controllerProfiles[1].label=="Wheel B"&&!savedAgain.controllerProfiles[1].generic,
                    "v"+version+" profile identity or attributes changed on save");
                var reload=new Idas3ControlBindings();reload.Initialize(folder);
                Check(reload.LastError==null&&reload.Current.version==5&&reload.Current.actions[16].pad==Idas3ControlBindings.PadInput.None,"v"+version+" reload failed");
                reload.SelectControllerProfile("wheel-a","Wheel A",true);
                Check(reload.Current.actions[0].controlPath=="button4","v"+version+" profile was lost on reload");
            }
            var controls=new Idas3ControlBindings();controls.Initialize(Path.Combine(root,"new"));controls.BeginEdit();
            Check(!controls.TrySetDraftKey(Idas3ControlBindings.ActionId.Start,Idas3ControlBindings.Slot.Primary,KeyCode.Escape),"Start accepted reserved Escape");
            Check(!controls.TrySetDraftKey(Idas3ControlBindings.ActionId.Start,Idas3ControlBindings.Slot.Primary,KeyCode.Return),"Start accepted reserved Enter");
            Check(controls.TrySetDraftKey(Idas3ControlBindings.ActionId.Start,Idas3ControlBindings.Slot.Primary,KeyCode.K),"Start rejected ordinary keyboard slot");
            Check(controls.ApplyDraft(),"Keyboard Start save failed");
            controls.Poll(key=>key==KeyCode.K,default,0);
            var keyboardFrame=new Idas3Native.FrameInput{flags=1};controls.ApplyMenu(ref keyboardFrame,false);
            Check(controls.StartHeld&&keyboardFrame.padConnected==1&&(keyboardFrame.padButtons&0x10)!=0,"Keyboard Start did not enter existing pad Start route");
            controls.Poll(_=>false,default,.1);
            controls.BeginEdit();Check(controls.ClearDraft(Idas3ControlBindings.ActionId.Start,Idas3ControlBindings.Slot.Primary)&&controls.ApplyDraft(),"Keyboard Start clear failed");
            Check(!controls.TrySetDraftPad(Idas3ControlBindings.ActionId.Start,Idas3ControlBindings.PadInput.LeftTrigger),"Start accepted a trigger");
            Check(!controls.TrySetDraftPad(Idas3ControlBindings.ActionId.Start,Idas3ControlBindings.PadInput.LeftStickLeft),"Start accepted a stick axis");
            var axis=new Idas3ControllerControl{path="pedal",minimum=-1,maximum=1,value=1,button=false};
            Check(!controls.TrySetDraftControl(Idas3ControlBindings.ActionId.Start,axis,1,-1),"Start accepted a pedal");
            var forgedAxis=new Idas3ControllerControl{path="leftTrigger",minimum=0,maximum=1,button=true};
            Check(!controls.TrySetDraftControl(Idas3ControlBindings.ActionId.Start,forgedAxis,1,0),"Start accepted a trigger path marked as button");
            Check(controls.TrySetDraftPad(Idas3ControlBindings.ActionId.Start,Idas3ControlBindings.PadInput.DPadUp),"Start rejected a digital direction");
            Check(controls.ApplyDraft(),"Start save failed");
            controls.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true,buttons=1},1);
            var frame=new Idas3Native.FrameInput{flags=1};controls.ApplyDriving(ref frame);
            Check(((frame.flags>>8)&7)==0,"Start became Gear 7");
            Check(controls.StartHeld,"Bound D-pad Start did not resolve as held");
            controls.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true,buttons=0},2);
            Check(!controls.StartHeld,"Released Start remained held");
            controls.BeginEdit();Check(controls.TrySetDraftPad(Idas3ControlBindings.ActionId.Start,Idas3ControlBindings.PadInput.B),"Start could not claim Shift Up's button");
            Check(controls.Draft.actions[16].pad==Idas3ControlBindings.PadInput.B&&
                controls.Draft.actions[(int)Idas3ControlBindings.ActionId.ShiftUp].pad==Idas3ControlBindings.PadInput.DPadUp,
                "Controller conflict did not swap the previous Start binding");
            controls.CancelEdit(false);
            var malformed=controls.Current;malformed.actions[16].pad=Idas3ControlBindings.PadInput.LeftTrigger;
            Check(!Idas3ControlBindings.Validate(malformed,out _),"File validation accepted analog Start");
            string invalidRoot=Path.Combine(root,"invalid");Directory.CreateDirectory(invalidRoot);
            File.WriteAllText(Path.Combine(invalidRoot,"controls.json"),JsonUtility.ToJson(malformed));
            var invalidLoad=new Idas3ControlBindings();invalidLoad.Initialize(invalidRoot);
            Check(invalidLoad.LastError!=null&&invalidLoad.Current.actions[16].pad==Idas3ControlBindings.PadInput.None,"Invalid Start file was loaded");
            string failureRoot=Path.Combine(root,"failure");
            var failedSave=new Idas3ControlBindings();failedSave.Initialize(failureRoot);failedSave.BeginEdit();
            Check(failedSave.TrySetDraftPad(Idas3ControlBindings.ActionId.Start,Idas3ControlBindings.PadInput.Y)&&failedSave.ApplyDraft(),"Could not establish prior save");
            string original=File.ReadAllText(Path.Combine(failureRoot,"controls.json"));
            Directory.CreateDirectory(Path.Combine(failureRoot,"controls.json.previous"));
            failedSave.BeginEdit();Check(failedSave.TrySetDraftPad(Idas3ControlBindings.ActionId.Start,Idas3ControlBindings.PadInput.A),"Could not prepare failing Start save");
            Check(!failedSave.ApplyDraft()&&failedSave.LastError!=null,"Failed save reported success");
            Check(File.ReadAllText(Path.Combine(failureRoot,"controls.json"))==original&&
                failedSave.Current.actions[16].pad==Idas3ControlBindings.PadInput.Y,"Failed save damaged the last good Start");
            controls.SelectControllerProfile("wheel","Wheel",true);controls.BeginEdit();
            var button=new Idas3ControllerControl{path="button0",label="Button 0",minimum=0,maximum=1,button=true,value=0,fallbackButtons=0x1000};
            Check(controls.TrySetDraftControl(Idas3ControlBindings.ActionId.Start,button,1,0),"Wheel button rejected");
            Check(controls.ApplyDraft(),"Wheel Start save failed");
            controls.BeginEdit();Check(controls.TrySetDraftPad(Idas3ControlBindings.ActionId.ShiftUp,Idas3ControlBindings.PadInput.A)&&controls.ApplyDraft(),
                "Could not prepare a generic raw-A alias regression");
            controls.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true},3,new[]{button});
            button.value=1;controls.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true,buttons=0x1000},4,new[]{button});
            frame=new Idas3Native.FrameInput{flags=1,padConnected=1,padButtons=0x1000};controls.ApplyMenu(ref frame,true);
            Check(controls.StartHeld&&(frame.padButtons&0x10)!=0&&(frame.padButtons&0x1000)==0,"Generic button did not become Start only");
            frame=new Idas3Native.FrameInput{flags=1,padConnected=1,padButtons=0x1000};controls.ApplyDriving(ref frame);
            Check((frame.padButtons&0x2000)==0,"Generic raw-A alias also triggered old Shift Up");
            controls.BlockStartUntilRelease();Check(!controls.StartHeld,"Owner change re-triggered held Start");
            button.value=0;controls.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true},5,new[]{button});
            button.value=1;controls.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true,buttons=0x1000},6,new[]{button});
            Check(controls.StartHeld,"Start did not recover after release");
            controls.BeginEdit();controls.CancelEdit();Check(!controls.StartHeld,"Cancel edit re-triggered held Start");
            button.value=0;controls.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true},7,new[]{button});
            button.value=1;controls.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true,buttons=0x1000},8,new[]{button});
            Check(controls.StartHeld,"Cancel edit permanently blocked Start");
            controls.BeginCapture(Idas3ControlBindings.ActionId.Start,Idas3ControlBindings.Slot.Controller,8.1);
            controls.CancelCapture();controls.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true,buttons=0x1000},8.2,new[]{button});
            Check(!controls.StartHeld,"Cancelled capture replayed held Start");
            button.value=0;controls.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true},8.3,new[]{button});
            button.value=1;controls.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true,buttons=0x1000},8.4,new[]{button});
            Check(controls.StartHeld,"Cancelled capture permanently blocked Start");
            controls.BeginCapture(Idas3ControlBindings.ActionId.Start,Idas3ControlBindings.Slot.Controller,8.5);
            controls.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true,buttons=0x1000},24,new[]{button});
            Check(!controls.IsCapturing&&!controls.StartHeld,"Timed-out capture replayed held Start");
            button.value=0;controls.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true},25,new[]{button});
            button.value=1;controls.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true,buttons=0x1000},26,new[]{button});
            Check(controls.StartHeld,"Timed-out capture permanently blocked Start");
            controls.BeginEdit();Check(controls.ApplyDraft(),"Re-applying Start failed");
            Check(!controls.StartHeld,"Apply replayed a held Start");
            controls.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true,buttons=0x1000},27,new[]{button});
            Check(!controls.StartHeld,"A synthetic neutral period released Start without physical release");
            button.value=0;controls.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true},28,new[]{button});
            button.value=1;controls.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true,buttons=0x1000},29,new[]{button});
            Check(controls.StartHeld,"Apply permanently blocked Start");
            controls.ControllerDeviceChanged();
            controls.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true,buttons=0x1000},30,new[]{button});
            Check(!controls.StartHeld,"Reconnected held Start became a new press");
            button.value=0;controls.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true},31,new[]{button});
            button.value=1;controls.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true,buttons=0x1000},32,new[]{button});
            Check(controls.StartHeld,"Reconnected Start did not recover after release");
            button.value=0;controls.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true},33,new[]{button});
            var gearButton=new Idas3ControllerControl{path="button1",minimum=0,maximum=1,button=true,value=0,fallbackButtons=0x2000};
            controls.BeginEdit();Check(controls.TrySetDraftControl(Idas3ControlBindings.ActionId.Gear1,gearButton,1,0)&&controls.ApplyDraft(),"Gear and Start could not be saved together");
            controls.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true},34,new[]{button,gearButton});
            button.value=gearButton.value=1;
            controls.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true,buttons=0x3000},35,new[]{button,gearButton});
            frame=new Idas3Native.FrameInput{flags=1,padConnected=1,padButtons=0x3000};controls.ApplyDriving(ref frame);
            Check(controls.StartHeld&&((frame.flags>>8)&7)==1,"Held Start swallowed independent Gear 1");
            button.value=gearButton.value=0;
            controls.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true},36,new[]{button,gearButton});
            controls.SelectControllerProfile("aux","Auxiliary",true);controls.BeginEdit();
            var rigButton=new Idas3ControllerControl{path="button2",minimum=0,maximum=1,button=true,value=0};
            Check(controls.TrySetDraftControl(Idas3ControlBindings.ActionId.Start,rigButton,1,0)&&controls.ApplyDraft(),"Auxiliary Start could not be saved");
            controls.SelectControllerProfile("wheel","Wheel",true);
            var rig=new Idas3ControllerDevices.RigSample{key="aux-physical",profile="aux",controls=new[]{rigButton}};
            controls.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true},37,new[]{button,gearButton},new[]{rig});
            rigButton.value=1;controls.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true},38,new[]{button,gearButton},new[]{rig});
            Check(controls.StartHeld,"Saved secondary generic device could not supply Start");
            frame=new Idas3Native.FrameInput{flags=1,padConnected=1};controls.ApplyMenu(ref frame,true);
            Check((frame.padButtons&0x10)!=0,"Rig Start did not reach native menu input");
            rigButton.value=0;button.value=1;
            controls.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true,buttons=0x1000},39,new[]{button,gearButton},new[]{rig});
            controls.BlockStartUntilRelease();Check(!controls.StartHeld,"Active Start release guard failed");
            rigButton.value=1;controls.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true,buttons=0x1000},40,new[]{button,gearButton},new[]{rig});
            Check(controls.StartHeld,"Blocked active Start swallowed independent rig Start");
            string transitionRoot=Path.Combine(root,"transition");
            var transition=new Idas3ControlBindings();transition.Initialize(transitionRoot);
            transition.SelectControllerProfile("hid","HID",true);transition.BeginEdit();
            var source=new Idas3ControllerControl{path="button0",minimum=0,maximum=1,button=true,value=0,fallbackButtons=0x1000};
            Check(transition.TrySetDraftControl(Idas3ControlBindings.ActionId.Start,source,1,0)&&transition.ApplyDraft(),"Transition Start setup failed");
            transition.BeginEdit();Check(transition.TrySetDraftPad(Idas3ControlBindings.ActionId.ShiftUp,Idas3ControlBindings.PadInput.A)&&transition.ApplyDraft(),"Transition raw alias setup failed");
            transition.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true},1,new[]{source});
            source.value=1;transition.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true,buttons=0x1000},2,new[]{source});
            transition.BeginEdit();Check(transition.ClearDraft(Idas3ControlBindings.ActionId.Start,Idas3ControlBindings.Slot.Controller)&&transition.ApplyDraft(),"Clear while held failed");
            frame=new Idas3Native.FrameInput{padConnected=1,padButtons=0x1000};transition.ApplyMenu(ref frame,true);
            Check((frame.padButtons&0x1010)==0,"Applying Clear replayed held button as A or Start");
            frame=new Idas3Native.FrameInput{padConnected=1,padButtons=0x1000};transition.ApplyDriving(ref frame);
            Check((frame.padButtons&0x2000)==0,"Applying Clear replayed held source as Shift Up");
            source.value=0;transition.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true},3,new[]{source});
            source.value=1;transition.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true,buttons=0x1000},4,new[]{source});
            frame=new Idas3Native.FrameInput{padConnected=1,padButtons=0x1000};transition.ApplyMenu(ref frame,true);
            Check((frame.padButtons&0x1000)!=0&&(frame.padButtons&0x10)==0,"Unbound A did not recover after release");
            var guard=new Idas3ControlBindings();guard.Initialize(Path.Combine(root,"capture-guard"));guard.SelectControllerProfile("wheel","Wheel",true);
            var restingPedal=new Idas3ControllerControl{path="pedal",minimum=-1,maximum=1,button=false,value=1};
            var latchedShifter=new Idas3ControllerControl{path="button1",minimum=0,maximum=1,button=true,value=1};
            var newStart=new Idas3ControllerControl{path="button2",minimum=0,maximum=1,button=true,value=0};
            var guardControls=new[]{restingPedal,latchedShifter,newStart};
            guard.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true},1,guardControls);
            guard.BeginCapture(Idas3ControlBindings.ActionId.Start,Idas3ControlBindings.Slot.Controller,2);
            guard.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true},2,guardControls);
            newStart.value=1;guard.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true},2.1,guardControls);
            Check(!guard.IsCapturing&&guard.Draft.actions[16].controlPath=="button2","Latched shifter blocked digital Start capture");
            newStart.value=0;guard.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true},2.2,guardControls);
            Check(!guard.SuppressInput,"Unrelated pedal or latched shifter permanently blocked Start capture release");
            Check(guard.ApplyDraft(),"Guarded Start save failed");
            guard.BeginEdit();guard.CancelEdit();guard.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true},2.3,guardControls);
            Check(!guard.SuppressInput,"Unrelated pedal or latched shifter blocked Start after cancel editing");
            string uiRoot=Path.Combine(root,"ui");
            var uiBindings=new Idas3ControlBindings();uiBindings.Initialize(uiRoot);
            var options=new Idas3GameOptions(new Platform());options.Initialize(uiRoot);
            var go=new GameObject("Start binding menu check");
            try
            {
                var menu=go.AddComponent<Idas3PauseMenu>();menu.Initialize(options);menu.InitializeBindings(uiBindings);
                menu.OpenAttractOptions();menu.SelectTab(3);
                Check(menu.OptionRows==17,"Start row is missing from controls menu");
                double now=Time.realtimeSinceStartupAsDouble;
                var uiButton=new Idas3ControllerControl{path="button7",label="Start button",minimum=0,maximum=1,button=true,value=0};
                menu.BeginBindingCapture(Idas3ControlBindings.ActionId.Start,Idas3ControlBindings.Slot.Controller);
                Check(uiBindings.IsCapturing,"Start row did not enter controller capture");
                uiBindings.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true},now,new[]{uiButton});
                uiButton.value=1;uiBindings.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true},now+.1,new[]{uiButton});
                Check(!uiBindings.IsCapturing&&uiBindings.Draft.actions[16].controlPath=="button7","UI capture did not select digital Start");
                uiButton.value=0;uiBindings.Poll(_=>false,new Idas3ControlBindings.PadState{connected=true},now+.2,new[]{uiButton});
                menu.Navigate(1);menu.Navigate(1);menu.Activate();
                Check(uiBindings.Current.actions[16].controlPath=="button7","UI Apply did not save Start");
                var uiReload=new Idas3ControlBindings();uiReload.Initialize(uiRoot);
                Check(uiReload.Current.actions[16].controlPath=="button7","New instance did not reload UI Start");
                menu.SelectTab(3);for(int i=0;i<16;++i)menu.Navigate(1);
                menu.Activate();Check(menu.BindingChoiceVisible,"Start row chooser is unreachable");
                menu.NavigateHorizontal(1);menu.Activate();
                Check(uiBindings.Draft.actions[16].controlPath==null,"UI Clear did not clear Start draft");
                menu.Navigate(1);menu.Navigate(1);menu.Activate();
                uiReload=new Idas3ControlBindings();uiReload.Initialize(uiRoot);
                Check(string.IsNullOrEmpty(uiReload.Current.actions[16].controlPath),"Cleared Start survived save and reload");
                menu.SetOpen(false);
            }
            finally { UnityEngine.Object.DestroyImmediate(go); }
            Idas3ControllerDevicesSmoke.RunStartDescriptorChecks(root,Check);
            Debug.Log("PASS "+checks+" Start binding and migration checks");
        }
        finally { Directory.Delete(root,true); }
    }
}
