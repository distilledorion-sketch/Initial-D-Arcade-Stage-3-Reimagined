using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using UnityEngine;

public static class Idas3HudCustomizationChecks
{
    sealed class Platform:Idas3GameOptions.IPlatform
    {
        public bool failApply;
        public int Width=>1280;public int Height=>720;public int DisplayMode=>0;public double Now=>0;
        public Idas3GameOptions.ResolutionChoice[] Resolutions=>new[]{new Idas3GameOptions.ResolutionChoice(1280,720)};
        public void Apply(Idas3GameOptions.Values previous,Idas3GameOptions.Values next,bool displayChanged){if(failApply)throw new IOException("Test apply failure");}
    }
    static T State<T>(Idas3HudCustomization menu,string name)=>(T)typeof(Idas3HudCustomization).GetProperty(name,BindingFlags.Instance|BindingFlags.NonPublic).GetValue(menu);
    public static void Run()
    {
        int checks=0;void Check(bool result,string description){++checks;if(!result)throw new Exception(description);}
        string root=Path.GetFullPath("Verification/hud-customization/settings-"+Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(root);
        string file=Path.Combine(root,"game-options.json");
        const string legacy="{\"version\":1,\"defaultCamera\":2,\"hudSpeedometerSize\":3}";
        File.WriteAllText(file,legacy);
        var platform=new Platform();var options=new Idas3GameOptions(platform);options.Initialize(root);
        Check(options.Current.hudMeterStyle==0&&options.Current.hudShiftLights&&options.Current.hudPedalIndicators&&options.Current.hudNameplateStyle==0&&options.Current.hudOrnamentId==0,"Legacy settings must keep original HUD and default Arcade options");
        Check(File.ReadAllText(file)==legacy,"Loading legacy settings rewrote the file");
        int count=Idas3ArcadeMeterCatalog.Count;
        Check(count==88,"The picker must contain Original and all 87 imported meters");
        Check(Idas3ArcadeMeterCatalog.StyleAt(0)==0&&Idas3ArcadeMeterCatalog.StyleAt(1)==1&&Idas3ArcadeMeterCatalog.SourceId(1)==31,"Original and the saved Stuttgart selection must retain their original IDs");
        var styles=new HashSet<int>();int previousSource=-1;
        for(int i=0;i<count;++i){
            int style=Idas3ArcadeMeterCatalog.StyleAt(i),source=Idas3ArcadeMeterCatalog.SourceId(style);
            Check(styles.Add(style)&&Idas3ArcadeMeterCatalog.IndexOfStyle(style)==i,"Catalog IDs must be unique and round-trip through their list indices");
            Check(!string.IsNullOrWhiteSpace(Idas3ArcadeMeterCatalog.Name(style)),"A meter has no display name");
            if(i>=2){Check(style==source+2&&source!=31&&source>previousSource,"Imported meter IDs must follow source IDs, preserving catalog gaps");previousSource=source;}
            var roundTrip=Idas3GameOptions.Normalize(JsonUtility.FromJson<Idas3GameOptions.Values>(JsonUtility.ToJson(new Idas3GameOptions.Values{hudMeterStyle=style})));
            Check(roundTrip.hudMeterStyle==style,"A valid meter ID was lost during JSON serialization or normalization: "+style);
        }
        foreach(int invalid in new[]{-1,18,33,35,61,99}){
            var normalized=Idas3GameOptions.Normalize(new Idas3GameOptions.Values{hudMeterStyle=invalid,hudNameplateStyle=invalid});
            Check(normalized.hudMeterStyle==0&&normalized.hudNameplateStyle==0,"Unknown HUD styles must fall back to original/off");
        }
        int ornamentCount=Idas3OrnamentCatalog.Count;
        Check(ornamentCount==281&&Idas3OrnamentCatalog.IdAt(0)==0,"The ornament picker must contain Off and all 280 recovered models");
        var ornamentIds=new HashSet<int>();
        for(int i=0;i<ornamentCount;++i){
            int id=Idas3OrnamentCatalog.IdAt(i);
            Check(ornamentIds.Add(id)&&Idas3OrnamentCatalog.IndexOf(id)==i&&Idas3OrnamentCatalog.IsValid(id),"Ornament IDs must be unique, valid and round-trip through their list indices");
            Check(!string.IsNullOrWhiteSpace(Idas3OrnamentCatalog.Name(id)),"An ornament has no display name");
            var roundTrip=Idas3GameOptions.Normalize(JsonUtility.FromJson<Idas3GameOptions.Values>(JsonUtility.ToJson(new Idas3GameOptions.Values{hudOrnamentId=id})));
            Check(roundTrip.hudOrnamentId==id,"An ornament ID was lost during JSON serialization or normalization: "+id);
        }
        foreach(int invalid in new[]{-1,int.MinValue,int.MaxValue})
            Check(Idas3GameOptions.Normalize(new Idas3GameOptions.Values{hudOrnamentId=invalid}).hudOrnamentId==0,"Unknown ornaments must fall back to Off");
        int firstOrnament=Idas3OrnamentCatalog.IdAt(1),lastOrnament=Idas3OrnamentCatalog.IdAt(ornamentCount-1);
        options.Draft.hudOrnamentId=firstOrnament;Check(options.HasUnsavedChanges,"Ornament changes not tracked");
        options.BeginEdit();Check(options.Draft.hudOrnamentId==0&&!options.HasUnsavedChanges,"Discarding an ornament change failed");
        options.Draft.hudMeterStyle=1;Check(options.HasUnsavedChanges,"Meter changes not tracked");
        options.BeginEdit();Check(options.Draft.hudMeterStyle==0&&!options.HasUnsavedChanges,"Discarding a meter change failed");
        options.Draft.hudShiftLights=false;Check(options.HasUnsavedChanges,"Shift lights not tracked");options.BeginEdit();
        options.Draft.hudPedalIndicators=false;Check(options.HasUnsavedChanges,"Pedal inputs not tracked");options.BeginEdit();
        options.Draft.hudNameplateStyle=1;Check(options.HasUnsavedChanges,"Driver plate not tracked");options.BeginEdit();

        var original=options.Current.Clone();
        options.Draft.masterVolume=.37f;options.Draft.width=1920;options.Draft.hudTimerSize=4;
        options.Draft.SetHudOffset(2,new Vector2(.12f,-.08f));
        var pending=options.Draft.Clone();
        var appearance=pending.Clone();appearance.hudMeterStyle=1;appearance.hudOrnamentId=firstOrnament;appearance.hudShiftLights=false;appearance.hudPedalIndicators=false;appearance.hudNameplateStyle=1;
        Check(options.ApplyHudCustomization(appearance),"HUD customization save failed");
        Idas3GameOptions.CopyHudCustomization(appearance,original);
        Check(Idas3GameOptions.Equivalent(options.Current,original)&&!options.DisplayConfirmationPending,"HUD save applied unrelated settings or layout edits");
        Idas3GameOptions.CopyHudCustomization(appearance,pending);
        Check(Idas3GameOptions.Equivalent(options.Draft,pending),"HUD save lost the main settings draft");
        var reload=new Idas3GameOptions(new Platform());reload.Initialize(root);
        Check(Idas3GameOptions.Equivalent(reload.Current,options.Current),"Appearance did not persist across restart");
        Check(reload.Current.hudSpeedometerSize==3,"Appearance save changed existing meter size");

        string saved=File.ReadAllText(file);platform.failApply=true;appearance.hudMeterStyle=0;
        Check(!options.ApplyHudCustomization(appearance),"Failed apply unexpectedly succeeded");platform.failApply=false;
        Check(Idas3GameOptions.Equivalent(options.Current,original)&&Idas3GameOptions.Equivalent(options.Draft,pending),"Failed HUD save changed current settings or pending edits");
        Check(File.ReadAllText(file)==saved,"Failed HUD save changed the file");

        var host=new GameObject("HUD customization checks");
        try{
            var menu=host.AddComponent<Idas3PauseMenu>();menu.Initialize(options);menu.SetOpen(true);menu.SelectTab(7);menu.Activate();
            var customization=host.GetComponent<Idas3HudCustomization>();
            Check(customization!=null&&customization.IsOpen,"HUD Customize action did not open");
            menu.Activate();
            Check(State<bool>(customization,"PickerOpen")&&State<int>(customization,"PickerIndex")==1,"Confirm on the meter row must open the picker at the saved Stuttgart selection");
            for(int direction=1;direction>=-1;direction-=2){
                for(int step=1;step<=count;++step){
                    if(direction>0)menu.Navigate(1);else menu.NavigateHorizontal(-1);
                    int expected=(1+direction*step+count)%count;
                    int selected=State<int>(customization,"PickerIndex");
                    int first=Mathf.FloorToInt(State<float>(customization,"PickerScroll"));
                    Check(selected==expected&&State<Idas3GameOptions.Values>(customization,"Draft").hudMeterStyle==Idas3ArcadeMeterCatalog.StyleAt(expected),"Picker navigation skipped a meter or did not wrap in the requested direction");
                    Check(first<=selected&&selected<first+9,"Keyboard/gamepad picker selection scrolled out of view");
                }
            }
            for(int i=0;i<7;++i)menu.Navigate(1); // Select list index 8.
            typeof(Idas3HudCustomization).GetField("pickerScroll",BindingFlags.Instance|BindingFlags.NonPublic).SetValue(customization,.5f); // A scrollbar drag can leave a fractional row.
            menu.Navigate(1);
            Check(State<int>(customization,"PickerIndex")==9&&State<float>(customization,"PickerScroll")>=1,"Navigation after a fractional scrollbar drag left the selected row clipped");
            for(int i=0;i<8;++i)menu.Navigate(-1); // Return to Stuttgart.
            menu.NavigateHorizontal(-1); // Stuttgart to Original in private working copy.
            Check(options.Current.hudMeterStyle==1&&options.Draft.hudMeterStyle==1,"Preview changed the active HUD or outer draft");
            menu.Back();Check(customization.IsOpen&&!State<bool>(customization,"PickerOpen")&&State<Idas3GameOptions.Values>(customization,"Draft").hudMeterStyle==0,"Back from the picker must retain the private selection and return to customization");
            menu.Back();Check(!customization.IsOpen&&File.ReadAllText(file)==saved,"Back did not cancel customization");
            menu.Activate();menu.NavigateHorizontal(-1);menu.Navigate(1);menu.Navigate(1);menu.Navigate(1);menu.Navigate(1);menu.Activate(); // Skip meter-only options, pass Ornament, Reset and Layout, then Apply.
            Check(!customization.IsOpen&&options.Current.hudMeterStyle==0,"Keyboard/gamepad Apply route failed for Original");
            menu.Activate();menu.NavigateHorizontal(1); // Original to Arcade.
            menu.Navigate(1);menu.Activate();menu.Navigate(1);menu.Activate();menu.Navigate(1);menu.Activate();
            menu.Navigate(1);menu.Navigate(1);menu.Navigate(1);menu.Navigate(1);menu.Activate();
            Check(options.Current.hudMeterStyle==1&&options.Current.hudShiftLights&&options.Current.hudPedalIndicators&&options.Current.hudNameplateStyle==0,"Keyboard/gamepad Arcade option routes failed");
            var wheelMode=typeof(Idas3PauseMenu).GetMethod("SetWheelNavigation",BindingFlags.Instance|BindingFlags.NonPublic);
            wheelMode.Invoke(menu,new object[]{true});menu.Activate();
            menu.NavigateHorizontal(1);menu.Activate(); // Steering selects shift lights; accelerator toggles.
            for(int i=0;i<6;++i)menu.NavigateHorizontal(1);menu.Activate();
            Check(!customization.IsOpen&&options.Current.hudMeterStyle==1&&!options.Current.hudShiftLights&&options.Current.hudPedalIndicators,"Wheel-only steering/accelerator cannot change an option and reach Apply");
            var beforeWheelCancel=options.Current.Clone();menu.Activate();menu.Activate(); // Open the meter picker.
            menu.NavigateHorizontal(-1);menu.Activate(); // Steering selects Original; accelerator closes the picker.
            for(int i=0;i<5;++i)menu.NavigateHorizontal(1);menu.Activate(); // Skip disabled options and pass Ornament, Reset, Layout and Apply to reach Cancel.
            Check(!customization.IsOpen&&Idas3GameOptions.Equivalent(options.Current,beforeWheelCancel),"Wheel-only steering/accelerator cannot reach Cancel without saving");
            menu.Activate();menu.Activate();menu.NavigateHorizontal(-1);menu.NavigateHorizontal(-1);menu.Activate(); // Wrap from Stuttgart through Original to the last imported meter.
            int lastStyle=Idas3ArcadeMeterCatalog.StyleAt(count-1);
            Check(State<Idas3GameOptions.Values>(customization,"Draft").hudMeterStyle==lastStyle,"Wheel picker failed to wrap through the entire catalog");
            for(int i=0;i<7;++i)menu.NavigateHorizontal(1);menu.Activate();
            Check(!customization.IsOpen&&options.Current.hudMeterStyle==lastStyle,"Wheel-only Apply failed for an imported meter");
            reload=new Idas3GameOptions(new Platform());reload.Initialize(root);
            Check(reload.Current.hudMeterStyle==lastStyle&&reload.Current.hudSpeedometerSize==3,"An imported meter selection or the existing layout was lost on restart");
            wheelMode.Invoke(menu,new object[]{false});
            menu.Activate();
            for(int i=0;i<count-1;++i)menu.NavigateHorizontal(-1); // Last imported meter to Original.
            menu.Navigate(1);
            Check(State<int>(customization,"SelectedRow")==4,"Ornaments must remain selectable with the original HUD");
            menu.Activate();
            Check(State<bool>(customization,"OrnamentPicker")&&State<int>(customization,"PickerIndex")==1,"The ornament picker must open at the saved selection");
            for(int direction=1;direction>=-1;direction-=2){
                for(int step=1;step<=ornamentCount;++step){
                    if(direction>0)menu.Navigate(1);else menu.NavigateHorizontal(-1);
                    int expected=(1+direction*step+ornamentCount)%ornamentCount;
                    int selected=State<int>(customization,"PickerIndex");
                    int first=Mathf.FloorToInt(State<float>(customization,"PickerScroll"));
                    Check(selected==expected&&State<Idas3GameOptions.Values>(customization,"Draft").hudOrnamentId==Idas3OrnamentCatalog.IdAt(expected),"Ornament navigation skipped a model or failed to wrap");
                    Check(first<=selected&&selected<first+9,"Ornament picker selection scrolled out of view");
                }
            }
            menu.NavigateHorizontal(-1); // Private Off preview.
            Check(options.Current.hudOrnamentId==firstOrnament&&options.Draft.hudOrnamentId==firstOrnament,"Preview changed the saved ornament or outer draft");
            menu.Back();Check(customization.IsOpen&&!State<bool>(customization,"PickerOpen")&&State<int>(customization,"SelectedRow")==4,"Back from ornaments must return to the ornament row");
            menu.Back();Check(!customization.IsOpen&&options.Current.hudMeterStyle==lastStyle&&options.Current.hudOrnamentId==firstOrnament,"Cancelling ornaments changed live settings");
            menu.Activate();
            for(int i=0;i<count-1;++i)menu.NavigateHorizontal(-1);
            menu.Navigate(1);menu.Activate();menu.Navigate(-1);menu.Navigate(-1);menu.Activate(); // First through Off to last ornament.
            menu.Navigate(1);menu.Navigate(1);menu.Navigate(1);menu.Activate();
            Check(!customization.IsOpen&&options.Current.hudMeterStyle==0&&options.Current.hudOrnamentId==lastOrnament,"Applying an ornament with the original HUD failed");
            reload=new Idas3GameOptions(new Platform());reload.Initialize(root);
            Check(reload.Current.hudOrnamentId==lastOrnament&&reload.Current.hudMeterStyle==0&&reload.Current.hudSpeedometerSize==3,"An ornament selection or existing HUD layout was lost on restart");
            wheelMode.Invoke(menu,new object[]{true});menu.Activate();
            menu.NavigateHorizontal(1);menu.Activate();menu.NavigateHorizontal(1);menu.Activate(); // Original skips disabled rows; last ornament wraps to Off.
            menu.NavigateHorizontal(1);menu.NavigateHorizontal(1);menu.NavigateHorizontal(1);menu.Activate();
            Check(!customization.IsOpen&&options.Current.hudOrnamentId==0&&options.Current.hudMeterStyle==0,"Wheel-only ornament selection and Apply failed");
            wheelMode.Invoke(menu,new object[]{false});menu.Activate();
            menu.Navigate(1);menu.NavigateHorizontal(1); // Select an ornament in the private draft.
            menu.Navigate(1);menu.Activate();
            Check(State<Idas3GameOptions.Values>(customization,"Draft").hudOrnamentId==0,"Reset Defaults did not clear the ornament");
            menu.Navigate(1);menu.Navigate(1);menu.Activate();
            Check(!customization.IsOpen&&options.Current.hudOrnamentId==0,"Default ornament appearance did not save");
            menu.Activate();menu.SetOpen(false);Check(!customization.IsOpen,"Closing the pause menu left customization open");
        }finally{UnityEngine.Object.DestroyImmediate(host);}
        File.WriteAllText(Path.Combine(root,"checks.txt"),"PASS "+checks+" HUD customization migration, persistence, transaction, meter and ornament menu checks\n");
        Debug.Log("HUD customization checks passed: "+checks+" ("+root+")");
    }
}
