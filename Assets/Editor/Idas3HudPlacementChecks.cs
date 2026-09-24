using System;
using System.Globalization;
using System.IO;
using System.Reflection;
using System.Text;
using UnityEngine;

public static class Idas3HudPlacementChecks
{
    sealed class Platform : Idas3GameOptions.IPlatform
    {
        public bool failApply;
        public int Width => 1280;
        public int Height => 720;
        public int DisplayMode => 0;
        public double Now => 0;
        public Idas3GameOptions.ResolutionChoice[] Resolutions => new[] { new Idas3GameOptions.ResolutionChoice(1280,720) };
        public void Apply(Idas3GameOptions.Values previous,Idas3GameOptions.Values next,bool displayChanged)
        {
            if(failApply)throw new IOException("Placement test platform failure");
        }
    }

    static readonly MethodInfo boundsMethod=typeof(Idas3GameOptions).Assembly.GetType("Idas3OrnamentRenderer")
        .GetMethod("ScreenBounds",BindingFlags.Static|BindingFlags.NonPublic);
    static Rect Bounds(float width,float height,Idas3GameOptions.Values values=null) =>
        (Rect)boundsMethod.Invoke(null,new object[] { width,height,values });
    static bool Near(float a,float b) => Mathf.Abs(a-b)<.001f;
    static bool Same(Rect a,Rect b) => Near(a.x,b.x)&&Near(a.y,b.y)&&Near(a.width,b.width)&&Near(a.height,b.height);
    static bool Finite(float value) => !float.IsNaN(value)&&!float.IsInfinity(value);
    static bool Finite(Rect value) => Finite(value.x)&&Finite(value.y)&&Finite(value.width)&&Finite(value.height);
    static bool SameLayout(Idas3GameOptions.Values a,Idas3GameOptions.Values b)
    {
        for(int group=0;group<Idas3GameOptions.Values.HudLayoutGroupCount;++group)
            if(a.HudOffset(group)!=b.HudOffset(group)||a.HudGroupScale(group)!=b.HudGroupScale(group))return false;
        return a.hudMessagesSize==b.hudMessagesSize&&a.minimapSize==b.minimapSize&&a.minimapZoom==b.minimapZoom;
    }

    public static void Run() => Debug.Log(RunChecks());

    public static string RunChecks()
    {
        int checks=0;
        void Check(bool result,string message){++checks;if(!result)throw new Exception(message);}
        string root=Path.GetFullPath("Verification/hud-placement/settings-"+Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(root);
        string file=Path.Combine(root,"game-options.json");

        // A real old-format file has ten positions and no ornament-size field.
        var oldPositions=new Vector2[10];
        oldPositions[2]=new Vector2(.12f,-.08f);oldPositions[3]=oldPositions[6]=oldPositions[7]=new Vector2(.04f,.03f);
        oldPositions[9]=new Vector2(-.1f,.02f);
        var json=new StringBuilder("{\"version\":1,\"hudSpeedometerSize\":3,\"hudPositions\":[");
        for(int i=0;i<oldPositions.Length;++i){
            if(i>0)json.Append(',');
            json.Append("{\"x\":").Append(oldPositions[i].x.ToString("R",CultureInfo.InvariantCulture));
            json.Append(",\"y\":").Append(oldPositions[i].y.ToString("R",CultureInfo.InvariantCulture)).Append('}');
        }
        string legacy=json.Append("]}").ToString();File.WriteAllText(file,legacy);
        var platform=new Platform();var options=new Idas3GameOptions(platform);options.Initialize(root);
        Check(Idas3GameOptions.Values.HudLayoutGroupCount==11&&options.Current.hudPositions.Length==11,"Layout migration did not add the ornament group");
        Check(options.Current.hudOrnamentSize==2&&options.Current.HudGroupScale(10)==1&&options.Current.HudOffset(10)==Vector2.zero,"Legacy ornament placement or size changed");
        Check(options.Current.hudSpeedometerSize==3,"Layout migration changed the existing tachometer size");
        Check(options.Current.HudSizePercent(2)==125&&options.Current.HudSizePercent(10)==100,"Fine-size migration replaced existing legacy sizes");
        Check(options.Current.hudSizePercent.Length==11&&Array.TrueForAll(options.Current.hudSizePercent,value=>value==0),"Loading old settings invented fine-size overrides");
        for(int i=0;i<10;++i)Check(options.Current.HudOffset(i)==oldPositions[i],"Layout migration lost old group "+i);
        Check(File.ReadAllText(file)==legacy,"Loading a legacy layout rewrote the file");

        var defaults=new Idas3GameOptions.Values();
        var copy=defaults.Clone();copy.SetHudOffset(10,new Vector2(.15f,.2f));
        Check(defaults.HudOffset(10)==Vector2.zero&&!ReferenceEquals(defaults.hudPositions,copy.hudPositions),"Cloning shares the layout array");
        Check(!Idas3GameOptions.Equivalent(defaults,copy),"Ornament position is missing from change detection");
        copy=defaults.Clone();copy.hudOrnamentSize=4;
        Check(!Idas3GameOptions.Equivalent(defaults,copy),"Ornament size is missing from change detection");
        copy=defaults.Clone();copy.SetHudOffset(2,new Vector2(.1f,.2f));copy.hudSpeedometerSize=0;
        copy.SetHudOffset(10,new Vector2(-.15f,.25f));copy.hudOrnamentSize=4;
        Check(copy.HudOffset(2)==new Vector2(.1f,.2f)&&copy.HudOffset(10)==new Vector2(-.15f,.25f),"Tachometer and ornament offsets are linked");
        Check(copy.HudGroupScale(2)==.5f&&copy.HudGroupScale(10)==1.5f,"Tachometer and ornament scales are linked");
        var beforeInvalid=copy.Clone();
        foreach(int group in new[] { -1,0,11,int.MaxValue })copy.SetHudOffset(group,Vector2.one);
        Check(Idas3GameOptions.Equivalent(copy,beforeInvalid),"An invalid layout group changed a valid group");
        var missing=defaults.Clone();missing.hudPositions=null;missing.SetHudOffset(10,new Vector2(.2f,.3f));
        Check(missing.hudPositions.Length==11&&missing.HudOffset(10)==new Vector2(.2f,.3f),"Ornament placement cannot recover a missing positions array");

        foreach(int size in new[] { int.MinValue,-1,0,1,2,3,4,5,int.MaxValue }){
            var value=defaults.Clone();value.hudOrnamentSize=size;value.hudSpeedometerSize=size;
            value=Idas3GameOptions.Normalize(value);
            int expected=Mathf.Clamp(size,0,4);
            Check(value.hudOrnamentSize==expected&&value.hudSpeedometerSize==expected,"Meter/ornament size escaped its supported range");
            Check(Near(value.HudGroupScale(10),.5f+.25f*expected)&&Near(value.HudGroupScale(2),.5f+.25f*expected),"HUD size did not map to 50..150 percent");
        }
        foreach(Vector2 point in new[] { new Vector2(float.NaN,float.PositiveInfinity),new Vector2(float.NegativeInfinity,float.NaN),new Vector2(-9,8) }){
            var value=defaults.Clone();value.SetHudOffset(2,point);value.SetHudOffset(10,point);
            Check(Finite(Bounds(1280,720,value)),"A corrupt preview offset produced non-finite screen geometry before normalization");
            value=Idas3GameOptions.Normalize(value);
            foreach(int group in new[] { 2,10 }){
                var position=value.HudOffset(group);
                Check(Finite(position.x)&&Finite(position.y)&&Mathf.Abs(position.x)<=1&&Mathf.Abs(position.y)<=1,"Invalid placement escaped normalization");
            }
            Check(Finite(Bounds(1280,720,value)),"Normalized ornament placement produced non-finite screen geometry");
        }

        // Layout transfer includes sizes and minimap settings, but not unrelated
        // appearance or outer settings. Copies must never share their arrays.
        var layout=options.Current.Clone();layout.SetHudOffset(2,new Vector2(-.2f,-.1f));layout.SetHudOffset(10,new Vector2(.04f,.18f));
        layout.hudTimerSize=0;layout.hudSpeedometerSize=1;layout.hudRecordsSize=3;layout.hudLegendSize=4;
        layout.hudOnlineSize=0;layout.hudMirrorSize=3;layout.hudMessagesSize=4;layout.hudChallengersSize=1;
        layout.hudTimeExtensionSize=0;layout.hudOrnamentSize=3;layout.minimapSize=2;layout.minimapZoom=0;
        layout.hudMeterStyle=1;layout.hudOrnamentId=Idas3OrnamentCatalog.IdAt(1);layout.masterVolume=.16f;
        var target=defaults.Clone();Idas3GameOptions.CopyHudLayout(layout,target);
        Check(SameLayout(layout,target),"Layout transfer lost a position, group size or minimap setting");
        Check(target.hudMeterStyle==defaults.hudMeterStyle&&target.hudOrnamentId==0&&target.masterVolume==1,"Layout transfer applied unrelated appearance or audio");
        target.SetHudOffset(10,Vector2.zero);Check(layout.HudOffset(10)!=Vector2.zero,"Layout transfer retained a shared positions array");
        target=defaults.Clone();Idas3GameOptions.CopyHudCustomization(layout,target);
        Check(SameLayout(defaults,target)&&target.hudMeterStyle==1&&target.hudOrnamentId==layout.hudOrnamentId,"Ordinary appearance copy unexpectedly included layout");

        var activeBefore=options.Current.Clone();
        options.Draft.masterVolume=.37f;options.Draft.width=1920;options.Draft.height=1080;options.Draft.defaultCamera=2;
        options.Draft.hudTimerSize=4;options.Draft.SetHudOffset(2,new Vector2(.19f,-.1f));
        var outerPending=options.Draft.Clone();
        var customize=options.Draft.Clone();var nested=customize.Clone();Idas3GameOptions.CopyHudLayout(layout,nested);
        Check(Idas3GameOptions.Equivalent(options.Current,activeBefore)&&Idas3GameOptions.Equivalent(options.Draft,outerPending),"Private layout preview changed active or outer settings");
        // Cancel discards the nested working copy without copying it back.
        nested=null;
        Check(Idas3GameOptions.Equivalent(customize,outerPending)&&File.ReadAllText(file)==legacy,"Cancelling a nested layout changed its parent or save file");
        nested=customize.Clone();Idas3GameOptions.CopyHudLayout(layout,nested);Idas3GameOptions.CopyHudLayout(nested,customize);
        Check(SameLayout(customize,layout)&&Idas3GameOptions.Equivalent(options.Draft,outerPending),"Layout Done saved outside its customization transaction");
        customize.hudMeterStyle=1;customize.hudOrnamentId=layout.hudOrnamentId;
        Check(options.ApplyHudCustomization(customize,true),"Applying appearance and layout failed");
        var expectedActive=activeBefore.Clone();Idas3GameOptions.CopyHudCustomization(customize,expectedActive);Idas3GameOptions.CopyHudLayout(customize,expectedActive);
        var expectedPending=outerPending.Clone();Idas3GameOptions.CopyHudCustomization(customize,expectedPending);Idas3GameOptions.CopyHudLayout(customize,expectedPending);
        Check(Idas3GameOptions.Equivalent(options.Current,Idas3GameOptions.Normalize(expectedActive))&&!options.DisplayConfirmationPending,"Layout Apply committed unrelated main settings or a display change");
        Check(Idas3GameOptions.Equivalent(options.Draft,Idas3GameOptions.Normalize(expectedPending))&&options.HasUnsavedChanges,"Layout Apply lost unrelated pending main settings");
        var reload=new Idas3GameOptions(new Platform());reload.Initialize(root);
        Check(Idas3GameOptions.Equivalent(reload.Current,options.Current),"Saved ornament/tachometer placement did not persist");
        Check(reload.Current.HudOffset(10)==layout.HudOffset(10)&&reload.Current.hudOrnamentSize==3,"Reload lost the added ornament group");

        string saved=File.ReadAllText(file);var savedActive=options.Current.Clone();var savedDraft=options.Draft.Clone();
        customize.SetHudOffset(10,new Vector2(-.22f,.33f));customize.hudOrnamentSize=0;platform.failApply=true;
        Check(!options.ApplyHudCustomization(customize,true),"A failing platform unexpectedly committed layout");platform.failApply=false;
        Check(Idas3GameOptions.Equivalent(options.Current,savedActive)&&Idas3GameOptions.Equivalent(options.Draft,savedDraft),"Failed layout Apply lost active or pending settings");
        Check(File.ReadAllText(file)==saved,"Failed layout Apply changed the file");
        Check(options.ApplyHudCustomization(customize),"Ordinary appearance transaction failed");
        Check(SameLayout(options.Current,savedActive)&&SameLayout(options.Draft,savedDraft),"Appearance-only Apply silently committed a private layout");

        FineSizingChecks(root,Check);

        foreach(Vector2 resolution in new[] { new Vector2(640,480),new Vector2(1280,720),new Vector2(1920,800) }){
            float width=resolution.x,height=resolution.y,fit=Mathf.Min(width/1280f,height/720f),baseSize=240*fit;
            var expected=new Rect(width*.66f-baseSize*.5f,-baseSize*(.25f/2.7f),baseSize,baseSize);
            Check(Same(Bounds(width,height),expected)&&Same(Bounds(width,height,defaults),expected),"Default ornament rectangle changed at "+resolution);
            var meterOnly=defaults.Clone();meterOnly.SetHudOffset(2,new Vector2(-.3f,-.2f));meterOnly.hudSpeedometerSize=4;
            Check(Same(Bounds(width,height,meterOnly),expected),"Tachometer placement moved the ornament at "+resolution);
            for(int size=0;size<=4;++size){
                var value=defaults.Clone();value.hudOrnamentSize=size;
                var scaled=Bounds(width,height,value);float scale=.5f+.25f*size;
                Check(Near(scaled.width,baseSize*scale)&&Near(scaled.height,baseSize*scale),"Ornament size did not scale with viewport fit");
                Check(Near(scaled.center.x,width*.66f)&&Near(scaled.y,-scaled.height*(.25f/2.7f)),"Scaling moved the original mount anchor");
                value.SetHudOffset(10,new Vector2(.025f,.16f));var moved=Bounds(width,height,value);
                Check(Near((moved.x-scaled.x)/width,.025f)&&Near((moved.y-scaled.y)/height,.16f),"Placement offset was interpreted in pixels instead of normalized screen units");
                foreach(Vector2 offset in new[] { new Vector2(-1,-1),new Vector2(-1,1),new Vector2(1,-1),new Vector2(1,1) }){
                    value.SetHudOffset(10,offset);var clamped=Bounds(width,height,value);
                    Check(Finite(clamped)&&clamped.xMin>=-.001f&&clamped.xMax<=width+.001f&&clamped.yMax<=height+.001f,"Moved ornament escaped the screen at "+resolution);
                    Check(clamped.yMin>=-clamped.height*(.25f/2.7f)-.001f,"Top-edge clamp exposed more than the original chain overhang");
                }
            }
        }
        string report="PASS "+checks+" HUD placement checks: legacy migration, precise 1% sizing, independent groups, minimap preset compensation, size/finite bounds, copy/cancel/apply, persistence, and 4:3/16:9/ultrawide rectangles.";
        File.WriteAllText(Path.Combine(root,"checks.txt"),report+"\n");
        return report+" ("+root+")";
    }

    static void FineSizingChecks(string root,Action<bool,string> Check)
    {
        var defaults=new Idas3GameOptions.Values();
        var fine=defaults.Clone();fine.hudSpeedometerSize=3;fine.hudOrnamentSize=1;
        Check(fine.HudSizePercent(2)==125&&fine.HudSizePercent(10)==75,"Zero overrides did not preserve distinct legacy meter/ornament sizes");
        foreach(int group in new[] { 2,10 }){
            float previous=0;
            for(int percent=50;percent<=150;++percent){
                fine.SetHudSizePercent(group,percent);
                Check(fine.HudSizePercent(group)==percent&&Near(fine.HudGroupScale(group),percent*.01f),"A 1% size change was rounded to a coarse preset");
                if(percent>50)Check(Near(fine.HudGroupScale(group)-previous,.01f),"Consecutive size values did not produce a one-percent scale step");
                previous=fine.HudGroupScale(group);
            }
        }
        Check(fine.hudSpeedometerSize==3&&fine.hudOrnamentSize==1,"Fine resizing rewrote legacy preset geometry");
        fine.SetHudSizePercent(2,113);fine.SetHudSizePercent(10,87);
        Check(fine.HudSizePercent(2)==113&&fine.HudSizePercent(10)==87,"Meter and ornament fine sizes are linked");
        Check(!Idas3GameOptions.Equivalent(fine,defaults),"Fine-size overrides are missing from change detection");
        var almost=fine.Clone();almost.SetHudSizePercent(2,114);
        Check(!Idas3GameOptions.Equivalent(fine,almost),"A one-percent size change was considered unchanged");
        Check(fine.HudSizePercent(2)==113&&!ReferenceEquals(fine.hudSizePercent,almost.hudSizePercent),"Cloning shares fine-size overrides");

        fine.SetHudOffset(3,new Vector2(.11f,.09f));
        fine.SetHudSizePercent(3,91);fine.SetHudSizePercent(6,117);fine.SetHudSizePercent(7,133);
        Check(fine.HudSizePercent(3)==91&&fine.HudSizePercent(6)==117&&fine.HudSizePercent(7)==133,"Shared records/opponent positions incorrectly link their size overrides");
        Check(fine.HudOffset(3)==fine.HudOffset(6)&&fine.HudOffset(3)==fine.HudOffset(7),"Fine-size editing broke shared record/opponent placement");
        fine.ResetHudSize(6);
        Check(fine.HudSizePercent(6)==100&&fine.hudSizePercent[6]==0&&fine.HudSizePercent(3)==91&&fine.HudSizePercent(7)==133,"Resetting one opponent size changed another group");
        fine.ResetHudSize(2);
        Check(fine.HudSizePercent(2)==100&&fine.hudSizePercent[2]==0&&fine.hudSpeedometerSize==2&&fine.HudSizePercent(10)==87,"Reset did not restore the meter's default preset and clear only its override");
        var beforeInvalid=fine.Clone();
        foreach(int group in new[] { -1,0,11,int.MaxValue }){fine.SetHudSizePercent(group,113);fine.ResetHudSize(group);}
        Check(Idas3GameOptions.Equivalent(beforeInvalid,fine),"An invalid fine-size group mutated settings");

        foreach(int preset in new[] { 0,1,2 }){
            var map=defaults.Clone();map.minimapSize=preset;int baked=100+25*preset;
            Check(map.HudSizePercent(5)==baked&&Near(map.HudGroupScale(5),1),"Legacy minimap preset geometry was scaled twice");
            foreach(int percent in new[] { 100,101,113,127,149,150 }){
                map.SetHudSizePercent(5,percent);
                Check(map.HudSizePercent(5)==percent&&Near(map.HudGroupScale(5)*baked,percent),"Fine minimap scale did not compensate for baked native preset size");
            }
            Check(map.minimapSize==preset,"Fine minimap resizing changed the native base preset");
            map.SetHudSizePercent(5,int.MinValue);
            Check(map.HudSizePercent(5)==100,"Minimap fine size was allowed below 100 percent");
            map.SetHudSizePercent(5,int.MaxValue);
            Check(map.HudSizePercent(5)==150,"Minimap fine size was allowed above 150 percent");
            map.ResetHudSize(5);
            Check(map.HudSizePercent(5)==100&&map.minimapSize==0&&map.hudSizePercent[5]==0&&Near(map.HudGroupScale(5),1),"Minimap reset retained a preset or override scale");
        }

        // Corrupt, old and truncated arrays must not lose valid entries or
        // allow unsupported sizes to reach either native or imported HUDs.
        var invalid=defaults.Clone();invalid.hudSizePercent=null;invalid.hudSpeedometerSize=4;
        var normalized=Idas3GameOptions.Normalize(invalid);
        Check(normalized.hudSizePercent.Length==11&&normalized.HudSizePercent(2)==150,"Missing fine-size array lost legacy fallback");
        invalid.hudSizePercent=new[] { 0,0,113 };normalized=Idas3GameOptions.Normalize(invalid);
        Check(normalized.hudSizePercent.Length==11&&normalized.HudSizePercent(2)==113&&normalized.HudSizePercent(10)==100,"Short fine-size array failed to expand without losing valid values");
        invalid.hudSizePercent=new[] { 0,int.MinValue,-1,49,151,99,0,int.MaxValue,117,100,119,137,143 };
        invalid.hudLegendSize=1;normalized=Idas3GameOptions.Normalize(invalid);
        Check(normalized.hudSizePercent.Length==11,"An oversized fine-size array was not trimmed to valid groups");
        Check(normalized.HudSizePercent(1)==50&&normalized.HudSizePercent(2)==50&&normalized.HudSizePercent(3)==50&&normalized.HudSizePercent(4)==150,"Malformed ordinary size overrides were not clamped");
        Check(normalized.HudSizePercent(5)==100&&normalized.hudSizePercent[6]==0&&normalized.HudSizePercent(6)==75&&normalized.HudSizePercent(7)==150,"Minimap limits or zero legacy fallback were lost during normalization");
        Check(normalized.HudSizePercent(8)==117&&normalized.HudSizePercent(9)==100&&normalized.HudSizePercent(10)==119,"Normalization rounded valid precise sizes");
        invalid.hudSizePercent=null;invalid.SetHudSizePercent(10,109);
        Check(invalid.hudSizePercent.Length==11&&invalid.HudSizePercent(10)==109,"Fine-size setter did not recover a missing array");
        foreach(int group in new[] { 1,2,3,4,6,7,8,9,10 }){
            invalid.SetHudSizePercent(group,int.MinValue);Check(invalid.HudSizePercent(group)==50,"Fine-size setter did not enforce minimum");
            invalid.SetHudSizePercent(group,int.MaxValue);Check(invalid.HudSizePercent(group)==150,"Fine-size setter did not enforce maximum");
        }

        fine.SetHudSizePercent(2,113);fine.SetHudSizePercent(10,87);fine.SetHudSizePercent(5,127);
        var transferred=defaults.Clone();Idas3GameOptions.CopyHudLayout(fine,transferred);
        Check(SameLayout(fine,transferred)&&transferred.HudSizePercent(2)==113&&transferred.HudSizePercent(10)==87,"Layout transfer lost precise sizes");
        transferred.SetHudSizePercent(2,114);
        Check(fine.HudSizePercent(2)==113&&!ReferenceEquals(fine.hudSizePercent,transferred.hudSizePercent),"Layout transfer shared the fine-size array");
        transferred=defaults.Clone();Idas3GameOptions.CopyHudCustomization(fine,transferred);
        Check(transferred.HudSizePercent(2)==100&&transferred.HudSizePercent(10)==100,"Appearance-only copy changed fine layout sizes");

        string saveRoot=Path.Combine(root,"fine-size-transactions");
        var platform=new Platform();var options=new Idas3GameOptions(platform);options.Initialize(saveRoot);
        options.BeginEdit();options.Draft.masterVolume=.43f;options.Draft.width=1920;
        options.Draft.SetHudSizePercent(2,109);options.Draft.SetHudSizePercent(10,123);
        var active=options.Current.Clone();var outer=options.Draft.Clone();
        var customization=outer.Clone();var nested=customization.Clone();
        nested.SetHudSizePercent(2,113);nested.SetHudSizePercent(10,87);nested.SetHudSizePercent(5,127);
        nested.SetHudSizePercent(3,91);nested.SetHudSizePercent(6,117);nested.SetHudSizePercent(7,133);
        Check(Idas3GameOptions.Equivalent(options.Current,active)&&Idas3GameOptions.Equivalent(options.Draft,outer)&&Idas3GameOptions.Equivalent(customization,outer),"Fine-size preview leaked into active or parent settings");
        // Cancel the child and open a fresh one: no one-percent change leaks.
        nested=customization.Clone();Check(nested.HudSizePercent(2)==109&&nested.HudSizePercent(10)==123,"Cancelled fine-size edits leaked into reopened child");
        nested.SetHudSizePercent(2,113);nested.SetHudSizePercent(10,87);nested.SetHudSizePercent(5,127);
        nested.SetHudSizePercent(3,91);nested.SetHudSizePercent(6,117);nested.SetHudSizePercent(7,133);
        Idas3GameOptions.CopyHudLayout(nested,customization);
        Check(Idas3GameOptions.Equivalent(options.Draft,outer)&&Idas3GameOptions.Equivalent(options.Current,active),"Fine-size Done committed before outer Apply");
        Check(options.ApplyHudCustomization(customization,true),"Precise-size transaction failed");
        Check(options.Current.HudSizePercent(2)==113&&options.Current.HudSizePercent(10)==87&&options.Current.HudSizePercent(5)==127,"Apply rounded precise meter, ornament or minimap sizes");
        Check(options.Current.HudSizePercent(3)==91&&options.Current.HudSizePercent(6)==117&&options.Current.HudSizePercent(7)==133,"Apply merged independently sized records/opponent panels");
        Check(options.Current.masterVolume==active.masterVolume&&options.Current.width==active.width&&!options.DisplayConfirmationPending,"Fine-size Apply committed pending audio/display edits");
        Check(options.Draft.masterVolume==outer.masterVolume&&options.Draft.width==outer.width&&options.Draft.HudSizePercent(2)==113,"Fine-size Apply discarded unrelated pending edits");
        var reload=new Idas3GameOptions(new Platform());reload.Initialize(saveRoot);
        Check(Idas3GameOptions.Equivalent(reload.Current,options.Current)&&reload.Current.HudSizePercent(10)==87,"Fine-size overrides did not survive save/reload");
        string path=Path.Combine(saveRoot,"game-options.json"),saved=File.ReadAllText(path);
        active=options.Current.Clone();outer=options.Draft.Clone();customization=outer.Clone();customization.ResetHudSize(10);
        Check(options.Current.HudSizePercent(10)==87&&options.Draft.HudSizePercent(10)==87,"Private Reset changed the active ornament size");
        platform.failApply=true;Check(!options.ApplyHudCustomization(customization,true),"Failed precise-size reset unexpectedly applied");platform.failApply=false;
        Check(Idas3GameOptions.Equivalent(options.Current,active)&&Idas3GameOptions.Equivalent(options.Draft,outer)&&File.ReadAllText(path)==saved,"Failed precise-size transaction did not roll back atomically");
        Check(options.ApplyHudCustomization(customization,true),"Precise-size reset transaction failed");
        reload=new Idas3GameOptions(new Platform());reload.Initialize(saveRoot);
        Check(reload.Current.HudSizePercent(10)==100&&reload.Current.hudSizePercent[10]==0&&reload.Current.HudSizePercent(2)==113,"Saved reset failed to clear only the ornament fine-size override");

        foreach(Vector2 resolution in new[] { new Vector2(640,480),new Vector2(1280,720),new Vector2(1920,800) }){
            var values=defaults.Clone();values.SetHudSizePercent(10,113);
            float baseWidth=Bounds(resolution.x,resolution.y,defaults).width;
            var first=Bounds(resolution.x,resolution.y,values);values.SetHudSizePercent(10,114);var next=Bounds(resolution.x,resolution.y,values);
            Check(Near(first.width,baseWidth*1.13f)&&Near(next.width-first.width,baseWidth*.01f),"Rendered ornament size ignored a one-percent adjustment at "+resolution);
            Check(Near(first.center.x,next.center.x),"One-percent resize changed the mount's horizontal anchor");
        }
    }
}
