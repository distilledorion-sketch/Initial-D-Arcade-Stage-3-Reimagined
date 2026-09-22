using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using UnityEngine;

// This translates host controls into the existing native input ABI. Source
// steering response, dead zones, pedal curves and physics remain native.
public sealed class Idas3ControlBindings
{
    public enum ActionId { Accelerate, Brake, SteerLeft, SteerRight, ShiftUp, ShiftDown, Camera, Pause, Online, Headlights }
    public enum Slot { Primary, Secondary, Extra, Controller }
    public enum PadInput
    {
        None, A, B, X, Y, LeftShoulder, RightShoulder, Back, Start, LeftThumb, RightThumb,
        DPadUp, DPadDown, DPadLeft, DPadRight, LeftTrigger, RightTrigger,
        LeftStickLeft, LeftStickRight, LeftStickUp, LeftStickDown,
        RightStickLeft, RightStickRight, RightStickUp, RightStickDown
    }
    [Serializable] public sealed class Binding
    {
        public KeyCode key1, key2, key3;
        public PadInput pad;
        public string controlPath, controlLabel;
        public float controlRest, controlMin, controlMax;
        public int controlDirection;
        public bool controlButton;
        public Binding Clone() => (Binding)MemberwiseClone();
    }
    [Serializable] public sealed class ControllerProfile
    {
        public string key, label;
        public bool generic;
        public Binding[] actions;
        public ControllerProfile Clone() => new ControllerProfile { key=key,label=label,generic=generic,actions=CloneActions(actions) };
    }
    [Serializable] public sealed class Values
    {
        public int version = 3;
        public Binding[] actions;
        public ControllerProfile[] controllerProfiles;
        public Values Clone()
        {
            var result = new Values { version = version, actions = actions == null ? null : new Binding[actions.Length] };
            if (actions != null) for (int i = 0; i < actions.Length; ++i) result.actions[i] = actions[i]?.Clone();
            if (controllerProfiles != null) { result.controllerProfiles=new ControllerProfile[controllerProfiles.Length];for(int i=0;i<controllerProfiles.Length;++i)result.controllerProfiles[i]=controllerProfiles[i]?.Clone(); }
            return result;
        }
    }
    public struct PadState
    {
        public bool connected;
        public ushort buttons;
        public byte leftTrigger, rightTrigger;
        public short thumbLX, thumbLY, thumbRX, thumbRY;
    }

    private static readonly string[] ActionNames = { "Accelerate", "Brake", "Steer left", "Steer right", "Shift up", "Shift down", "Change camera", "Pause", "Online menu", "Toggle headlights" };
    private static readonly KeyCode[] PollKeys = CreatePollKeys();
    private static readonly HashSet<KeyCode> ValidKeys = new HashSet<KeyCode>(PollKeys);
    private readonly bool[] heldKeys = new bool[512], keyboardActions = new bool[10];
    private Values current, draft;
    private PadState pad;
    private string file;
    private const string LegacyProfile = "default";
    private Dictionary<string,ControllerProfile> savedProfiles = new Dictionary<string,ControllerProfile>(StringComparer.Ordinal), draftProfiles;
    private readonly Dictionary<string,Idas3ControllerControl> controls = new Dictionary<string,Idas3ControllerControl>(StringComparer.Ordinal);
    private readonly Dictionary<string,float> restValues = new Dictionary<string,float>(StringComparer.Ordinal);
    private string activeProfileKey=LegacyProfile;
    private bool genericProfile, awaitingProfileSample;
    private bool releaseBlocked, captureArmed;
    private ActionId captureAction;
    private Slot captureSlot;
    private double captureDeadline;
    public Values Current => current?.Clone();
    public Values Draft => draft;
    public string FilePath => file;
    public string LastError { get; private set; }
    public bool HasUnsavedChanges { get { if(current==null)return false;StoreDraftProfile();return !Equivalent(current,draft)||!ProfilesEquivalent(savedProfiles,draftProfiles); } }
    public string ActiveControllerProfileLabel => savedProfiles.TryGetValue(activeProfileKey,out var value)?value.label:"Default controller";
    public string ActiveControllerProfileKey => activeProfileKey;
    public string LastNotice { get; private set; }
    public bool IsCapturing { get; private set; }
    public string CapturePrompt { get; private set; } = "";
    public string CaptureError { get; private set; }
    public bool SuppressInput => IsCapturing || releaseBlocked;
    public bool PauseHeld => !SuppressInput && (Held(KeyCode.Escape) || ActionHeld(ActionId.Pause) || (pad.buttons&0x10)!=0);
    public bool OnlineHeld => !SuppressInput && (Held(KeyCode.F1) || ActionHeld(ActionId.Online) ||
        (current.actions[8].pad==PadInput.None&&string.IsNullOrEmpty(current.actions[8].controlPath)&&(pad.buttons&0x20)!=0));
    public bool ViewChangeHeld => !SuppressInput && ActionHeld(ActionId.Camera);
    public event System.Action Changed;

    public static Values Defaults()
    {
        var value = new Values { actions = new Binding[10] };
        value.actions[0] = new Binding { key1 = KeyCode.W, key2 = KeyCode.UpArrow, pad = PadInput.RightTrigger };
        value.actions[1] = new Binding { key1 = KeyCode.S, key2 = KeyCode.DownArrow, key3 = KeyCode.Space, pad = PadInput.LeftTrigger };
        value.actions[2] = new Binding { key1 = KeyCode.A, key2 = KeyCode.LeftArrow, pad = PadInput.LeftStickLeft };
        value.actions[3] = new Binding { key1 = KeyCode.D, key2 = KeyCode.RightArrow, pad = PadInput.LeftStickRight };
        value.actions[4] = new Binding { key1 = KeyCode.E, pad = PadInput.B };
        value.actions[5] = new Binding { key1 = KeyCode.Q, pad = PadInput.X };
        value.actions[6] = new Binding { key1 = KeyCode.C, pad = PadInput.Y };
        value.actions[7] = new Binding { key1 = KeyCode.Escape, pad = PadInput.Start };
        value.actions[8] = new Binding { key1 = KeyCode.F1, pad = PadInput.Back };
        value.actions[9] = new Binding { key1 = KeyCode.H, pad = PadInput.RightThumb };
        return value;
    }
    public void Initialize(string saveRoot)
    {
        if (string.IsNullOrWhiteSpace(saveRoot)) throw new ArgumentException("A controls save directory is required.", nameof(saveRoot));
        file = Path.Combine(Path.GetFullPath(saveRoot), "controls.json");
        current = Defaults(); LastError = null;
        if (File.Exists(file))
        {
            try { current = Read(file); }
            catch (Exception error)
            {
                LastError = "Could not load controls; using defaults. " + error.Message;
                if (File.Exists(file + ".previous"))
                    try { current = Read(file + ".previous"); LastError = "Recovered the previous controls after the current file could not be loaded."; }
                    catch (Exception) { /* Preserve both files for recovery; defaults remain usable. */ }
            }
        }
        savedProfiles.Clear();activeProfileKey=LegacyProfile;genericProfile=false;
        savedProfiles.Add(LegacyProfile,new ControllerProfile{key=LegacyProfile,label="Default controller",actions=CloneActions(current.actions)});
        if(current.controllerProfiles!=null)foreach(var profile in current.controllerProfiles)savedProfiles.Add(profile.key,profile.Clone());
        current.version=3;current.controllerProfiles=null;
        draftProfiles=CloneProfiles(savedProfiles);draft = current.Clone(); IsCapturing = captureArmed = releaseBlocked = false;
        controls.Clear();restValues.Clear();awaitingProfileSample=false;LastNotice=null;
        Array.Clear(heldKeys, 0, heldKeys.Length); Array.Clear(keyboardActions, 0, keyboardActions.Length); pad = default;
        CapturePrompt = ""; CaptureError = null; Changed?.Invoke();
    }
    private static Values Read(string path)
    {
        if (new FileInfo(path).Length > 262144) throw new InvalidDataException("Controls file is too large.");
        var value = JsonUtility.FromJson<Values>(File.ReadAllText(path));
        MigrateHeadlights(value);
        if (!Validate(value, out string error)) throw new InvalidDataException(error);
        return value;
    }
    private static void MigrateHeadlights(Values value){
        if(value==null||(value.version!=1&&value.version!=2))return;
        Binding[] Upgrade(Binding[] old,bool controllerOnly){
            if(old==null||old.Length!=9)return old;
            var result=new Binding[10];Array.Copy(old,result,9);
            var added=new Binding{key1=controllerOnly?KeyCode.None:KeyCode.H,pad=PadInput.RightThumb};
            foreach(var existing in old){
                if(existing==null)continue;
                if(existing.key1==KeyCode.H||existing.key2==KeyCode.H||existing.key3==KeyCode.H)added.key1=KeyCode.None;
                if(existing.pad==PadInput.RightThumb||existing.controlPath=="rightStickPress")added.pad=PadInput.None;
            }
            result[9]=added;return result;
        }
        value.actions=Upgrade(value.actions,false);
        if(value.controllerProfiles!=null)foreach(var profile in value.controllerProfiles){
            if(profile==null)continue;profile.actions=Upgrade(profile.actions,true);
            if(profile.generic&&profile.actions!=null&&profile.actions.Length==10)ClearController(profile.actions[9]);
        }
        value.version=3;
    }
    public void BeginEdit() { EnsureInitialized(); CancelCapture(); draftProfiles=CloneProfiles(savedProfiles);draft = current.Clone(); LastError = null; CaptureError = null;LastNotice=null; }
    public void CancelEdit(bool waitForRelease = true) { EnsureInitialized(); CancelCapture(); draftProfiles=CloneProfiles(savedProfiles);draft = current.Clone(); LastError = null;LastNotice=null; releaseBlocked = waitForRelease; }
    public void ResetDraft() { EnsureInitialized(); CancelCapture(); draft = Defaults();if(genericProfile)foreach(var b in draft.actions)ClearController(b); LastError = null; CaptureError = null;LastNotice=null; releaseBlocked=false; }
    public void SelectControllerProfile(string key,string label,bool useGenericDefaults=false)
    {
        EnsureInitialized();
        if(string.IsNullOrWhiteSpace(key))key=LegacyProfile;
        if(key.Length>512||key.IndexOf('\0')>=0)throw new ArgumentException("Invalid controller profile key.",nameof(key));
        label=string.IsNullOrWhiteSpace(label)?"Controller":label.Length>256?label.Substring(0,256):label;
        if(key==activeProfileKey){savedProfiles[key].label=label;draftProfiles[key].label=label;return;}
        StoreDraftProfile();CancelCapture();
        if(!savedProfiles.ContainsKey(key))
        {
            if(savedProfiles.Count>=65)throw new InvalidOperationException("Too many saved controller profiles.");
            var value=new ControllerProfile{key=key,label=label,generic=useGenericDefaults,actions=CloneActions(savedProfiles[LegacyProfile].actions)};
            if(useGenericDefaults)foreach(var binding in value.actions)ClearController(binding);
            savedProfiles.Add(key,value);draftProfiles.Add(key,value.Clone());
        }
        activeProfileKey=key;genericProfile=savedProfiles[key].generic;
        savedProfiles[key].label=label;draftProfiles[key].label=label;
        current=Compose(current.actions,savedProfiles[key].actions);draft=Compose(draft.actions,draftProfiles[key].actions);
        ControllerDeviceChanged();LastError=null;LastNotice=null;Changed?.Invoke();
    }
    // Two identical devices may share a profile. The provider still calls this
    // on a physical-device change so its held button cannot finish old capture.
    public void ControllerDeviceChanged()
    {
        EnsureInitialized();CancelCapture();pad=default;controls.Clear();restValues.Clear();awaitingProfileSample=true;
        Array.Clear(keyboardActions,0,keyboardActions.Length);BlockUntilRelease();
    }
    private static Binding[] CloneActions(Binding[] value)
    { if(value==null)return null;var result=new Binding[value.Length];for(int i=0;i<value.Length;++i)result[i]=value[i]?.Clone();return result; }
    private static Dictionary<string,ControllerProfile> CloneProfiles(Dictionary<string,ControllerProfile> value)
    { var result=new Dictionary<string,ControllerProfile>(StringComparer.Ordinal);foreach(var pair in value)result.Add(pair.Key,pair.Value.Clone());return result; }
    private static Values Compose(Binding[] keys,Binding[] controller)
    { var value=new Values{actions=CloneActions(controller)};for(int i=0;i<10;++i){value.actions[i].key1=keys[i].key1;value.actions[i].key2=keys[i].key2;value.actions[i].key3=keys[i].key3;}return value; }
    private void StoreDraftProfile() { if(draftProfiles!=null&&draft!=null)draftProfiles[activeProfileKey].actions=CloneActions(draft.actions); }
    private static bool ProfilesEquivalent(Dictionary<string,ControllerProfile> a,Dictionary<string,ControllerProfile> b)
    { if(a.Count!=b.Count)return false;foreach(var pair in a){if(!b.TryGetValue(pair.Key,out var other))return false;for(int i=0;i<10;++i)if(!SameController(pair.Value.actions[i],other.actions[i]))return false;}return true; }
    private Values PackDraft()
    {
        StoreDraftProfile();var value=Compose(draft.actions,draftProfiles[LegacyProfile].actions);
        var keys=new List<string>(draftProfiles.Keys);keys.Sort(StringComparer.Ordinal);var profiles=new List<ControllerProfile>();
        foreach(var key in keys)if(key!=LegacyProfile){var profile=draftProfiles[key].Clone();foreach(var binding in profile.actions)binding.key1=binding.key2=binding.key3=KeyCode.None;profiles.Add(profile);}
        value.controllerProfiles=profiles.ToArray();return value;
    }
    public bool ApplyDraft()
    {
        // Menu routing owns confirm-button edges. Editing preferences must not
        // wait for an unrelated wheel, pedal, or throttle key to become neutral.
        EnsureInitialized(); CancelCapture(); releaseBlocked=false;
        if (!Validate(draft, out string error)) { LastError = error; return false; }
        var next = PackDraft();
        if(!Validate(next,out error)){LastError=error;return false;}
        string temporary = file + "." + Guid.NewGuid().ToString("N") + ".tmp";
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(file));
            byte[] bytes = new UTF8Encoding(false).GetBytes(JsonUtility.ToJson(next, true));
            if(bytes.Length>262144)throw new InvalidDataException("Controller profiles are too large to save.");
            using (var stream = new FileStream(temporary, FileMode.CreateNew, FileAccess.Write, FileShare.None))
            { stream.Write(bytes, 0, bytes.Length); stream.Flush(true); }
            if (File.Exists(file)) File.Replace(temporary, file, file + ".previous"); else File.Move(temporary, file);
        }
        catch (Exception failure)
        {
            try { if (File.Exists(temporary)) File.Delete(temporary); } catch (Exception) { }
            LastError = "Could not save controls. " + failure.Message; return false;
        }
        savedProfiles=CloneProfiles(draftProfiles);current=Compose(draft.actions,savedProfiles[activeProfileKey].actions);
        draftProfiles=CloneProfiles(savedProfiles);draft = current.Clone(); LastError = null; Changed?.Invoke(); return true;
    }
    public bool ClearDraft(ActionId action, Slot slot)
    {
        if (slot == Slot.Controller) return TrySetDraftPad(action, PadInput.None);
        return TrySetDraftKey(action, slot, KeyCode.None);
    }
    public bool TrySetDraftKey(ActionId action, Slot slot, KeyCode key)
    {
        EnsureInitialized(); CheckAction(action); CheckSlot(slot);
        if (slot == Slot.Controller) { LastError = "Choose a keyboard slot for this key."; return false; }
        var candidate = draft.Clone(); SetKey(candidate.actions[(int)action], slot, key);
        return AcceptDraft(candidate);
    }
    public bool TrySetDraftPad(ActionId action, PadInput input)
    {
        EnsureInitialized(); CheckAction(action);
        var binding=new Binding{pad=input};return SetController(action,binding);
    }
    public bool TrySetDraftControl(ActionId action,Idas3ControllerControl control,int direction,float rest)
    {
        EnsureInitialized();CheckAction(action);
        if(control==null){LastError="Choose a connected controller control.";return false;}
        var binding=new Binding{controlPath=control.path,controlLabel=control.label,controlDirection=direction,
            controlRest=rest,controlMin=control.minimum,controlMax=control.maximum,controlButton=control.button};
        if(!genericProfile&&TryStandardControl(binding,out var standard))return TrySetDraftPad(action,standard);
        return SetController(action,binding);
    }
    private bool SetController(ActionId action,Binding binding)
    {
        var candidate=draft.Clone();var previous=candidate.actions[(int)action].Clone();string notice=null;
        if(ControllerIdentity(binding)!=null)for(int i=0;i<10;++i)
            if(i!=(int)action&&ControllerIdentity(candidate.actions[i])==ControllerIdentity(binding))
            {CopyController(previous,candidate.actions[i]);notice="Swapped "+ActionName(action)+" with "+ActionName((ActionId)i)+".";break;}
        CopyController(binding,candidate.actions[(int)action]);
        if(!AcceptDraft(candidate))return false;LastNotice=notice;return true;
    }
    private static void ClearController(Binding value){CopyController(new Binding(),value);}
    private static void CopyController(Binding source,Binding target)
    { target.pad=source.pad;target.controlPath=source.controlPath;target.controlLabel=source.controlLabel;target.controlDirection=source.controlDirection;target.controlRest=source.controlRest;target.controlMin=source.controlMin;target.controlMax=source.controlMax;target.controlButton=source.controlButton; }
    private static string ControllerIdentity(Binding value)=>!string.IsNullOrEmpty(value.controlPath)?"path:"+value.controlPath+":"+value.controlDirection:value.pad==PadInput.None?null:"pad:"+(int)value.pad;
    private static bool SameController(Binding a,Binding b)=>a!=null&&b!=null&&a.pad==b.pad&&string.Equals(a.controlPath??"",b.controlPath??"",StringComparison.Ordinal)&&a.controlDirection==b.controlDirection&&a.controlRest==b.controlRest&&a.controlMin==b.controlMin&&a.controlMax==b.controlMax&&a.controlButton==b.controlButton;
    private static bool TryStandardControl(Binding binding,out PadInput input)
    {
        input=PadInput.None;switch(binding.controlPath)
        {
            case "buttonSouth":input=PadInput.A;break;case "buttonEast":input=PadInput.B;break;case "buttonWest":input=PadInput.X;break;case "buttonNorth":input=PadInput.Y;break;
            case "start":input=PadInput.Start;break;case "select":input=PadInput.Back;break;
            case "leftShoulder":input=PadInput.LeftShoulder;break;case "rightShoulder":input=PadInput.RightShoulder;break;
            case "leftStickPress":input=PadInput.LeftThumb;break;case "rightStickPress":input=PadInput.RightThumb;break;
            case "dpad/up":input=PadInput.DPadUp;break;case "dpad/down":input=PadInput.DPadDown;break;case "dpad/left":input=PadInput.DPadLeft;break;case "dpad/right":input=PadInput.DPadRight;break;
            case "leftTrigger":if(binding.controlDirection>0)input=PadInput.LeftTrigger;break;case "rightTrigger":if(binding.controlDirection>0)input=PadInput.RightTrigger;break;
            case "leftStick/x":input=binding.controlDirection<0?PadInput.LeftStickLeft:PadInput.LeftStickRight;break;
            case "leftStick/y":input=binding.controlDirection<0?PadInput.LeftStickDown:PadInput.LeftStickUp;break;
            case "rightStick/x":input=binding.controlDirection<0?PadInput.RightStickLeft:PadInput.RightStickRight;break;
            case "rightStick/y":input=binding.controlDirection<0?PadInput.RightStickDown:PadInput.RightStickUp;break;
        }return input!=PadInput.None;
    }
    private bool AcceptDraft(Values candidate)
    {
        if (!Validate(candidate, out string error)) { LastError = error; return false; }
        draft = candidate; LastError = null;LastNotice=null; return true;
    }
    public void BeginCapture(ActionId action, Slot slot, double now)
    {
        EnsureInitialized(); CheckAction(action); CheckSlot(slot);
        if (double.IsNaN(now) || double.IsInfinity(now)) throw new ArgumentException("Capture time must be finite.");
        captureAction = action; captureSlot = slot; captureDeadline = now + 15;
        IsCapturing = true; captureArmed = false; CaptureError = null; LastError = null;LastNotice=null;
        CapturePrompt = "Release buttons and leave axes at rest, then " + (slot == Slot.Controller ? "move an axis, pull a pedal, or press a controller button." : "press a key.");
        BlockUntilRelease();
    }
    public void CancelCapture()
    {
        if (!IsCapturing) return;
        IsCapturing = captureArmed = false; CapturePrompt = ""; CaptureError = null; BlockUntilRelease();
    }
    private void BlockUntilRelease() { releaseBlocked = true; }
    // Called once by the host, from raw hardware. Menu widgets and native input
    // consume the same sample; capture never polls hardware independently.
    public void Poll(Func<KeyCode, bool> keyHeld, PadState rawPad, double now,IReadOnlyList<Idas3ControllerControl> rawControls=null)
    {
        EnsureInitialized(); if (keyHeld == null) throw new ArgumentNullException(nameof(keyHeld));
        foreach (var key in PollKeys) heldKeys[(int)key] = keyHeld(key);
        pad = rawPad.connected ? rawPad : default;
        controls.Clear();if(rawControls!=null)foreach(var control in rawControls)
            if(control!=null&&!string.IsNullOrEmpty(control.path)&&Finite(control.value)&&Finite(control.minimum)&&Finite(control.maximum)&&control.maximum>control.minimum)controls[control.path]=control;
        if(awaitingProfileSample){SnapshotRest(true);awaitingProfileSample=false;}
        for (int i = 0; i < 10; ++i)
        { var binding = current.actions[i]; keyboardActions[i] = Held(binding.key1) || Held(binding.key2) || Held(binding.key3); }
        bool anyHeld = AnyInputHeld();
        if (IsCapturing)
        {
            if (double.IsNaN(now) || double.IsInfinity(now) || now >= captureDeadline)
            { CancelCapture(); CaptureError = "No control selected. Try again."; return; }
            if (!captureArmed)
            {
                // Pedals commonly rest at +1 or -1. Only buttons/keys must be
                // released; axes arm at their measured rest instead of zero.
                bool arm=controls.Count>0? !ButtonsOrKeysHeld():!anyHeld;
                if (arm) { captureArmed = true;SnapshotRest(); CapturePrompt = captureSlot == Slot.Controller ? "Press a button or move one axis/pedal. Escape cancels." : "Press a key. Escape cancels."; }
                return;
            }
            if (Held(KeyCode.Escape)) { CancelCapture(); return; }
            bool attempted = false, accepted = false;
            if (captureSlot == Slot.Controller)
            {
                if(controls.Count>0)
                { var control=CaptureControl(out int direction,out float rest);if(control!=null){attempted=true;accepted=TrySetDraftControl(captureAction,control,direction,rest);} }
                else {var input = CapturePad();if (input != PadInput.None) { attempted = true; accepted = TrySetDraftPad(captureAction, input); }}
            }
            else
            {
                foreach (var key in PollKeys)
                    if ((int)key < (int)KeyCode.Mouse0 && Held(key))
                    { attempted = true; accepted = TrySetDraftKey(captureAction, captureSlot, key); break; }
            }
            if (attempted)
            {
                IsCapturing = !accepted; captureArmed = false; BlockUntilRelease();
                CaptureError = accepted ? null : LastError;
                CapturePrompt = accepted ? (LastNotice??"Binding changed.")+" Apply to save." : "Release the control, then choose another. Escape cancels.";
                if (!accepted) captureDeadline = now + 15;
            }
            return;
        }
        if (releaseBlocked && !anyHeld) releaseBlocked = false;
    }

    internal void ApplyMenu(ref Idas3Native.FrameInput frame, bool genericDevice)
    {
        if(SuppressInput)return;
        // Wheels/HID devices use their saved pedal/steering/paddle bindings
        // in every menu too. Standard pads retain their D-pad, stick and A/B.
        if(genericDevice){
            // A generic stick axis may be bound to a pedal. Navigation must
            // use the configured actions, not also interpret that axis as a
            // raw gamepad stick (especially while holding Back).
            frame.thumbLX=frame.thumbLY=0;
            if(ActionHeld(ActionId.Accelerate)){frame.SetKey(13);frame.padButtons&=~0x2000u;}
            if(ActionHeld(ActionId.Brake)){
                // Native course menus consume B; managed overlays consume B
                // or Backspace. Cancel wins if both pedals are held, including
                // devices whose generic first-button fallback would emit A.
                ClearKey(ref frame,13);frame.SetKey(8);frame.padButtons=(frame.padButtons&~0x1000u)|0x2000u;
            }
            if(ActionHeld(ActionId.SteerLeft))frame.SetKey(37);
            if(ActionHeld(ActionId.SteerRight))frame.SetKey(39);
            if(ActionHeld(ActionId.ShiftUp))frame.SetKey(38);
            if(ActionHeld(ActionId.ShiftDown))frame.SetKey(40);
            if(ActionHeld(ActionId.Camera))frame.SetKey(67);
        }
    }
    internal void ApplyDriving(ref Idas3Native.FrameInput frame)
    {
        // Remove all old action aliases before placing the mapped actions.
        // Unrelated native shortcuts and menu keys retain their existing bits.
        foreach (int key in DrivingKeys) ClearKey(ref frame, key);
        for (int i = 0; i < 10; ++i)
        {
            var binding = current.actions[i];
            ClearBoundShortcut(ref frame, binding.key1); ClearBoundShortcut(ref frame, binding.key2); ClearBoundShortcut(ref frame, binding.key3);
        }
        frame.padConnected = pad.connected||controls.Count>0 ? 1u : 0u;
        frame.padButtons = pad.connected ? (uint)pad.buttons & ~0xE01Fu : 0u; // Pause is routed by the host.
        frame.thumbLY = 0; frame.thumbRX = pad.thumbRX; frame.thumbRY = pad.thumbRY;
        frame.leftTrigger = frame.rightTrigger = 0; frame.thumbLX = 0;
        if (SuppressInput) return;
        int[] output = CanonicalKeys;
        for (int i = 0; i < 7; ++i) if (keyboardActions[i]) frame.SetKey(output[i]);
        if(ActionHeld(ActionId.Headlights))frame.SetKey(72);
        if (!pad.connected&&controls.Count==0) return;
        frame.padConnected = 1;
        frame.rightTrigger = Pedal(current.actions[0]); frame.leftTrigger = Pedal(current.actions[1]);
        frame.thumbLX = Math.Max(-32768, Math.Min(32767, Axis(current.actions[3], false) - Axis(current.actions[2], true)));
        frame.thumbLY = SteeringOrthogonal();
        if (Digital(current.actions[4])) frame.padButtons |= 0x2000;
        if (Digital(current.actions[5])) frame.padButtons |= 0x4000;
        if (Digital(current.actions[6])) frame.padButtons |= 0x8000;
    }
    private int SteeringOrthogonal()
    {
        // thumbLX is virtual steering. Its radial dead zone must use the other
        // axis of that same stick, never an unrelated physical left-stick Y.
        var left=current.actions[(int)ActionId.SteerLeft];var right=current.actions[(int)ActionId.SteerRight];
        bool customLeft=!string.IsNullOrEmpty(left.controlPath),customRight=!string.IsNullOrEmpty(right.controlPath);
        if(customLeft||customRight)
        {
            if(!customLeft||!customRight||left.controlButton||right.controlButton||
                left.controlPath!=right.controlPath||left.controlDirection==right.controlDirection)return 0;
            string paired=PairedStickPath(left.controlPath);
            if(paired==null||!controls.TryGetValue(left.controlPath,out var source)||source.button||
                !controls.TryGetValue(paired,out var other)||other.button||other.minimum>=0||other.maximum<=0)return 0;
            float amount=Math.Max(-1,Math.Min(1,other.value/(other.value<0?-other.minimum:other.maximum)));
            return (int)Math.Round(amount*(amount<0?32768:32767));
        }
        string axis=StickPath(left.pad);
        if(!pad.connected||axis==null||axis!=StickPath(right.pad)||left.pad==right.pad)return 0;
        switch(axis)
        {
            case "leftStick/x":return pad.thumbLY;case "leftStick/y":return pad.thumbLX;
            case "rightStick/x":return pad.thumbRY;case "rightStick/y":return pad.thumbRX;
            default:return 0;
        }
    }
    private static string StickPath(PadInput input)
    {
        switch(input)
        {
            case PadInput.LeftStickLeft:case PadInput.LeftStickRight:return "leftStick/x";
            case PadInput.LeftStickDown:case PadInput.LeftStickUp:return "leftStick/y";
            case PadInput.RightStickLeft:case PadInput.RightStickRight:return "rightStick/x";
            case PadInput.RightStickDown:case PadInput.RightStickUp:return "rightStick/y";
            default:return null;
        }
    }
    private static string PairedStickPath(string path)
    {
        switch(path)
        {
            case "leftStick/x":return "leftStick/y";case "leftStick/y":return "leftStick/x";
            case "rightStick/x":return "rightStick/y";case "rightStick/y":return "rightStick/x";
            default:return null;
        }
    }
    private static readonly int[] DrivingKeys = { 87, 83, 65, 68, 38, 40, 37, 39, 32, 69, 81, 67, 72 };
    private static readonly int[] CanonicalKeys = { 87, 83, 65, 68, 69, 81, 67 };
    private static void ClearBoundShortcut(ref Idas3Native.FrameInput frame, KeyCode key)
    {
        // R is an optional native restart shortcut; rebinding it to driving
        // must not restart the race at the same time. Backspace behaves likewise.
        if (key == KeyCode.R) ClearKey(ref frame, 82);
        if (key == KeyCode.Backspace) ClearKey(ref frame, 8);
    }
    private static void ClearKey(ref Idas3Native.FrameInput frame, int key)
    {
        uint mask = ~(1u << (key & 31));
        switch (key >> 5)
        {
            case 0: frame.key0 &= mask; break; case 1: frame.key1 &= mask; break;
            case 2: frame.key2 &= mask; break; case 3: frame.key3 &= mask; break;
            case 4: frame.key4 &= mask; break; case 5: frame.key5 &= mask; break;
            case 6: frame.key6 &= mask; break; case 7: frame.key7 &= mask; break;
        }
    }
    private bool ActionHeld(ActionId action) => keyboardActions[(int)action] || Digital(current.actions[(int)action]);
    private bool Held(KeyCode key) => key != KeyCode.None && (int)key >= 0 && (int)key < heldKeys.Length && heldKeys[(int)key];
    private bool AnyInputHeld()
    {
        foreach (var key in PollKeys) if (Held(key)) return true;
        if(controls.Count>0)
        {
            foreach(var pair in controls)
            {
                var control=pair.Value;if(control.button){if(control.value>.5f)return true;continue;}
                if(restValues.TryGetValue(pair.Key,out float rest)&&Math.Abs(control.value-rest)>(control.maximum-control.minimum)*.15f)return true;
            }
            if(genericProfile)return false;
        }
        return pad.connected && (pad.buttons != 0 || pad.leftTrigger > 30 || pad.rightTrigger > 30 ||
            Math.Abs((int)pad.thumbLX) > 8000 || Math.Abs((int)pad.thumbLY) > 8000 || Math.Abs((int)pad.thumbRX) > 8000 || Math.Abs((int)pad.thumbRY) > 8000);
    }
    private bool ButtonsOrKeysHeld()
    { foreach(var key in PollKeys)if(Held(key))return true;foreach(var control in controls.Values)if(control.button&&control.value>.5f)return true;return pad.connected&&pad.buttons!=0; }
    private void SnapshotRest(bool preferBoundRest=false)
    {
        restValues.Clear();foreach(var pair in controls)
        {
            float rest=pair.Value.button?0:pair.Value.value;
            // Standard pads have known neutral triggers/sticks. Sampling a
            // held trigger during reconnect must not make its later release
            // look like permanent movement away from a fabricated rest of 1.
            if(preferBoundRest&&!genericProfile&&StandardAxis(pair.Key))rest=0;
            if(preferBoundRest&&!pair.Value.button)foreach(var binding in current.actions)
                if(binding.controlPath==pair.Key){rest=binding.controlRest;break;}
            restValues[pair.Key]=rest;
        }
    }
    private static bool StandardAxis(string path)=>path=="leftTrigger"||path=="rightTrigger"||
        path=="leftStick/x"||path=="leftStick/y"||path=="rightStick/x"||path=="rightStick/y";
    private Idas3ControllerControl CaptureControl(out int direction,out float rest)
    {
        Idas3ControllerControl best=null;direction=0;rest=0;float score=.20f;
        foreach(var pair in controls)
        {
            var control=pair.Value;if(!restValues.TryGetValue(pair.Key,out float baseline))continue;
            float delta=control.value-baseline;
            float amount=control.button?(control.value>.5f?2:0):Math.Abs(delta)/(control.maximum-control.minimum);
            if(amount>score){best=control;score=amount;direction=control.button?1:Math.Sign(delta);rest=control.button?0:baseline;}
        }return best;
    }
    private float CustomAmount(Binding binding)
    {
        if(string.IsNullOrEmpty(binding.controlPath)||!controls.TryGetValue(binding.controlPath,out var control))return 0;
        if(binding.controlButton)return control.value>.5f?1:0;
        float extent=binding.controlDirection>0?binding.controlMax-binding.controlRest:binding.controlRest-binding.controlMin;
        if(extent<=.0001f)return 0;return Math.Max(0,Math.Min(1,(control.value-binding.controlRest)*binding.controlDirection/extent));
    }
    private uint Pedal(Binding binding)=>string.IsNullOrEmpty(binding.controlPath)?Pedal(binding.pad):(uint)Math.Round(CustomAmount(binding)*255);
    private int Axis(Binding binding,bool negative)=>string.IsNullOrEmpty(binding.controlPath)?Axis(binding.pad,negative):(int)Math.Round(CustomAmount(binding)*(negative?32768:32767));
    private bool Digital(Binding binding)=>string.IsNullOrEmpty(binding.controlPath)?Digital(binding.pad):CustomAmount(binding)>=.5f;
    private static bool Finite(float value)=>!float.IsNaN(value)&&!float.IsInfinity(value);
    private static ushort ButtonMask(PadInput input)
    {
        switch (input)
        {
            case PadInput.A: return 0x1000; case PadInput.B: return 0x2000; case PadInput.X: return 0x4000; case PadInput.Y: return 0x8000;
            case PadInput.LeftShoulder: return 0x100; case PadInput.RightShoulder: return 0x200;
            case PadInput.Back: return 0x20; case PadInput.Start: return 0x10; case PadInput.LeftThumb: return 0x40; case PadInput.RightThumb: return 0x80;
            case PadInput.DPadUp: return 1; case PadInput.DPadDown: return 2; case PadInput.DPadLeft: return 4; case PadInput.DPadRight: return 8;
            default: return 0;
        }
    }
    private int Magnitude(PadInput input)
    {
        if (!pad.connected) return 0;
        ushort mask = ButtonMask(input); if (mask != 0) return (pad.buttons & mask) != 0 ? 32768 : 0;
        switch (input)
        {
            case PadInput.LeftTrigger: return pad.leftTrigger * 32768 / 255;
            case PadInput.RightTrigger: return pad.rightTrigger * 32768 / 255;
            case PadInput.LeftStickLeft: return Math.Max(0, -(int)pad.thumbLX);
            case PadInput.LeftStickRight: return Math.Max(0, (int)pad.thumbLX);
            case PadInput.LeftStickUp: return Math.Max(0, (int)pad.thumbLY);
            case PadInput.LeftStickDown: return Math.Max(0, -(int)pad.thumbLY);
            case PadInput.RightStickLeft: return Math.Max(0, -(int)pad.thumbRX);
            case PadInput.RightStickRight: return Math.Max(0, (int)pad.thumbRX);
            case PadInput.RightStickUp: return Math.Max(0, (int)pad.thumbRY);
            case PadInput.RightStickDown: return Math.Max(0, -(int)pad.thumbRY);
            default: return 0;
        }
    }
    private uint Pedal(PadInput input)
    {
        if (!pad.connected) return 0;
        if (input == PadInput.LeftTrigger) return pad.leftTrigger;
        if (input == PadInput.RightTrigger) return pad.rightTrigger;
        int magnitude = Magnitude(input);
        return (uint)Math.Min(255, (magnitude * 255 + 16383) / 32767);
    }
    private int Axis(PadInput input, bool negative)
    {
        int magnitude = Magnitude(input);
        // Raw stick directions remain bit exact, including -32768. Only a
        // digital/trigger source is stretched to the full destination range.
        if (input >= PadInput.LeftStickLeft) return magnitude;
        if (magnitude == 32768) return negative ? 32768 : 32767;
        return magnitude;
    }
    private bool Digital(PadInput input) => Magnitude(input) >= 16384;
    private PadInput CapturePad()
    {
        if (!pad.connected) return PadInput.None;
        for (int i = (int)PadInput.A; i <= (int)PadInput.DPadRight; ++i)
            if ((pad.buttons & ButtonMask((PadInput)i)) != 0) return (PadInput)i;
        for (int i = (int)PadInput.LeftTrigger; i <= (int)PadInput.RightStickDown; ++i)
            if (Magnitude((PadInput)i) >= 16384) return (PadInput)i;
        return PadInput.None;
    }
    public static bool Validate(Values value, out string error)
    {
        error = null;
        if (value == null || (value.version != 1&&value.version!=2&&value.version!=3) || value.actions == null || value.actions.Length != 10)
        { error = "Unsupported controls format."; return false; }
        var keys = new Dictionary<KeyCode, ActionId>(); var pads = new Dictionary<string, ActionId>(StringComparer.Ordinal);
        for (int i = 0; i < 10; ++i)
        {
            var binding = value.actions[i]; var action = (ActionId)i;
            if (binding == null) { error = "Missing binding for " + ActionName(action) + "."; return false; }
            foreach (var key in new[] { binding.key1, binding.key2, binding.key3 })
            {
                if (key == KeyCode.None) continue;
                if (!ValidKeys.Contains(key) || (int)key >= (int)KeyCode.Mouse0)
                { error = "Choose a keyboard key for " + ActionName(action) + "."; return false; }
                if ((key == KeyCode.Escape && action != ActionId.Pause) || (key == KeyCode.F1 && action != ActionId.Online) ||
                    key == KeyCode.Return || key == KeyCode.KeypadEnter || key == KeyCode.F2 || key == KeyCode.F3 || key == KeyCode.F5 || key == KeyCode.F11)
                { error = KeyName(key) + " is reserved for menu or game commands."; return false; }
                if (keys.TryGetValue(key, out var other))
                { error = KeyName(key) + " is already assigned to " + ActionName(other) + ". Clear that binding first."; return false; }
                keys.Add(key, action);
            }
            if (!Enum.IsDefined(typeof(PadInput), binding.pad)) { error = "Unknown controller input for " + ActionName(action) + "."; return false; }
            if(!string.IsNullOrEmpty(binding.controlPath))
            {
                if(binding.controlPath.Length>512||binding.controlPath.IndexOf('\0')>=0||(binding.controlLabel!=null&&binding.controlLabel.Length>256)||binding.pad!=PadInput.None||
                    (binding.controlDirection!=1&&binding.controlDirection!=-1)||!Finite(binding.controlRest)||!Finite(binding.controlMin)||!Finite(binding.controlMax)||
                    binding.controlMin>=binding.controlMax||binding.controlRest<binding.controlMin||binding.controlRest>binding.controlMax||
                    (binding.controlButton&&binding.controlDirection!=1)||
                    (!binding.controlButton&&(binding.controlDirection>0?binding.controlMax-binding.controlRest:binding.controlRest-binding.controlMin)<.0001f))
                {error="Invalid controller axis or button for "+ActionName(action)+".";return false;}
            }
            string identity=ControllerIdentity(binding);
            if(identity!=null)
            {
                if(pads.TryGetValue(identity,out var other)){error="Controller input is already assigned to "+ActionName(other)+".";return false;}
                pads.Add(identity,action);
            }
        }
        if(value.controllerProfiles!=null)
        {
            if((value.version!=2&&value.version!=3)||value.controllerProfiles.Length>64){error="Unsupported controller profile list.";return false;}
            var profileKeys=new HashSet<string>(StringComparer.Ordinal);
            foreach(var profile in value.controllerProfiles)
            {
                if(profile==null||string.IsNullOrWhiteSpace(profile.key)||profile.key==LegacyProfile||profile.key.Length>512||profile.key.IndexOf('\0')>=0||
                    (profile.label!=null&&profile.label.Length>256)||!profileKeys.Add(profile.key))
                {error="Invalid or duplicate controller profile.";return false;}
                if(!Validate(new Values{actions=profile.actions},out error))return false;
            }
        }
        return true;
    }
    public static bool Equivalent(Values a, Values b)
    {
        if (a == null || b == null || a.version != b.version || a.actions == null || b.actions == null || a.actions.Length != b.actions.Length) return false;
        for (int i = 0; i < a.actions.Length; ++i)
        {
            var x = a.actions[i]; var y = b.actions[i];
            if (x == null || y == null || x.key1 != y.key1 || x.key2 != y.key2 || x.key3 != y.key3 || !SameController(x,y)) return false;
        }
        return true;
    }
    private static void SetKey(Binding binding, Slot slot, KeyCode key)
    { if (slot == Slot.Primary) binding.key1 = key; else if (slot == Slot.Secondary) binding.key2 = key; else binding.key3 = key; }
    private static void CheckAction(ActionId action) { if ((int)action < 0 || (int)action >= 10) throw new ArgumentOutOfRangeException(nameof(action)); }
    private static void CheckSlot(Slot slot) { if ((int)slot < 0 || (int)slot > 3) throw new ArgumentOutOfRangeException(nameof(slot)); }
    private void EnsureInitialized() { if (current == null || file == null) throw new InvalidOperationException("Controls have not been initialized."); }
    private static KeyCode[] CreatePollKeys()
    {
        var result = new SortedSet<KeyCode>();
        foreach (KeyCode key in Enum.GetValues(typeof(KeyCode))) if ((int)key > 0 && (int)key < (int)KeyCode.JoystickButton0) result.Add(key);
        var keys = new KeyCode[result.Count]; result.CopyTo(keys); return keys;
    }
    public static string ActionName(ActionId action) { CheckAction(action); return ActionNames[(int)action]; }
    public string BindingName(ActionId action, Slot slot, bool draft = true)
    {
        EnsureInitialized(); CheckAction(action); CheckSlot(slot); var binding = (draft ? this.draft : current).actions[(int)action];
        return slot == Slot.Controller ? (!string.IsNullOrEmpty(binding.controlPath)?(string.IsNullOrWhiteSpace(binding.controlLabel)?binding.controlPath:binding.controlLabel)+(binding.controlButton?"":binding.controlDirection<0?" −":" +"):PadName(binding.pad)) : KeyName(slot == Slot.Primary ? binding.key1 : slot == Slot.Secondary ? binding.key2 : binding.key3);
    }
    public static string KeyName(KeyCode key)
    {
        switch (key)
        {
            case KeyCode.None: return "Unbound"; case KeyCode.UpArrow: return "Up"; case KeyCode.DownArrow: return "Down";
            case KeyCode.LeftArrow: return "Left"; case KeyCode.RightArrow: return "Right"; case KeyCode.LeftShift: return "Left Shift";
            case KeyCode.RightShift: return "Right Shift"; case KeyCode.LeftControl: return "Left Ctrl"; case KeyCode.RightControl: return "Right Ctrl";
            case KeyCode.LeftAlt: return "Left Alt"; case KeyCode.RightAlt: return "Right Alt"; case KeyCode.Return: return "Enter";
            case KeyCode.KeypadEnter: return "Keypad Enter"; default: return key.ToString();
        }
    }
    public static string PadName(PadInput input)
    {
        switch (input)
        {
            case PadInput.None: return "Unbound"; case PadInput.LeftTrigger: return "LT"; case PadInput.RightTrigger: return "RT";
            case PadInput.LeftShoulder: return "LB"; case PadInput.RightShoulder: return "RB";
            case PadInput.LeftThumb: return "Left stick click"; case PadInput.RightThumb: return "Right stick click";
            case PadInput.LeftStickLeft: return "Left stick left"; case PadInput.LeftStickRight: return "Left stick right";
            case PadInput.LeftStickUp: return "Left stick up"; case PadInput.LeftStickDown: return "Left stick down";
            case PadInput.RightStickLeft: return "Right stick left"; case PadInput.RightStickRight: return "Right stick right";
            case PadInput.RightStickUp: return "Right stick up"; case PadInput.RightStickDown: return "Right stick down";
            case PadInput.DPadUp: return "D-pad up"; case PadInput.DPadDown: return "D-pad down";
            case PadInput.DPadLeft: return "D-pad left"; case PadInput.DPadRight: return "D-pad right";
            default: return input.ToString();
        }
    }
}
