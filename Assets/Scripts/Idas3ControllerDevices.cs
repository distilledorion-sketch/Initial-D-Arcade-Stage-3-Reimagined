using System;
using System.Collections.Generic;
using System.IO;
using System.Security.Cryptography;
using System.Text;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.Controls;
using UnityEngine.InputSystem.HID;
using UnityEngine.InputSystem.XInput;

// Device discovery/selection is separate from action bindings and native input
// response. A selected missing device stays selected; it cannot fall through
// to a different controller. Only Automatic may change the input source.
public sealed class Idas3ControllerDevices : IDisposable
{
    public sealed class DeviceChoice
    {
        public string key, label;
        public bool connected;
    }
    [Serializable] private sealed class Preference
    {
        public int version = 1;
        public string key = "automatic", profile = "", label = "", model = "";
        public bool generic;
    }
    [Serializable] private sealed class HidIdentity { public int vendorId, productId, usagePage, usage; }
    private sealed class Device
    {
        public readonly DeviceChoice choice = new DeviceChoice();
        public string profile, model;
        public InputDevice unity;
        public int slot = -1;
        public bool generic, seen, firstValues = true, descriptorControls;
        public uint vendorId,productId;
        public readonly List<Idas3ControllerControl> controls = new List<Idas3ControllerControl>();
        public readonly List<AxisControl> axes = new List<AxisControl>();
        public float[] activityBaseline;
        public Idas3ControlBindings.PadState pad;
    }
    private readonly Func<double> now;
    private readonly Idas3GamepadInput.ReadXInput readXInput;
    private readonly Func<InputDevice,bool> discoveryFilter;
    private readonly List<Device> devices = new List<Device>();
    private readonly Dictionary<int, Device> unityDevices = new Dictionary<int, Device>();
    private readonly Device[] xbox = new Device[4];
    private readonly List<DeviceChoice> choices = new List<DeviceChoice>();
    private static readonly Idas3ControllerControl[] NoControls = Array.Empty<Idas3ControllerControl>();
    private readonly DeviceChoice automaticChoice = new DeviceChoice { key = "automatic", label = "Automatic", connected = true };
    private readonly DeviceChoice keyboardChoice = new DeviceChoice { key = "keyboard", label = "Keyboard only", connected = true };
    private readonly DeviceChoice missingChoice = new DeviceChoice();
    private Preference preference = new Preference();
    private Device active, automaticResume;
    private string file;
    private double nextScan;
    private bool initialized, dirty = true, xinputAvailable = true, restoreSelection;
    public IReadOnlyList<DeviceChoice> Choices => choices;
    public IReadOnlyList<Idas3ControllerControl> Controls => active != null && active.choice.connected ? active.controls : NoControls;
    public string SelectedKey => preference.key;
    public bool UsingFallback => Specific && active != null && active.choice.connected && active.choice.key != preference.key;
    public string ActiveName => active != null && active.choice.connected ? active.choice.label :
        preference.key == "keyboard" ? "Keyboard only" : preference.key == "automatic" ? "No controller connected" : (string.IsNullOrEmpty(preference.label) ? "Selected controller" : preference.label) + " (disconnected)";
    public string ActiveProfileKey => active != null ? active.profile : Specific ? preference.profile : "";
    public bool ActiveIsGeneric => active != null ? active.generic : Specific && preference.generic;
    internal Idas3WheelFeedback.InputIdentity WheelIdentity => new Idas3WheelFeedback.InputIdentity {
        connected=active!=null&&active.choice.connected,key=active?.choice.key??"",
        vendorId=active?.vendorId??0,productId=active?.productId??0
    };
    public string LastError { get; private set; }
    public event Action ActiveDeviceChanged;
    private bool Specific => preference.key != "automatic" && preference.key != "keyboard";

    public Idas3ControllerDevices() : this(() => Time.realtimeSinceStartupAsDouble, Idas3Native.ReadGamepad) { }
    // The production discovery/selection logic also accepts an isolated XInput
    // reader for bounded tests; no platform calls need to be imitated in tests.
    internal Idas3ControllerDevices(Func<double> time, Idas3GamepadInput.ReadXInput reader, Func<InputDevice,bool> filter = null)
    {
        now = time ?? throw new ArgumentNullException(nameof(time));
        readXInput = reader ?? throw new ArgumentNullException(nameof(reader));
        discoveryFilter = filter;
    }

    public void Initialize(string saveRoot)
    {
        if (string.IsNullOrWhiteSpace(saveRoot)) throw new ArgumentException("A controller save directory is required.", nameof(saveRoot));
        Dispose(); devices.Clear(); unityDevices.Clear(); choices.Clear(); active = automaticResume = null;
        file = Path.Combine(Path.GetFullPath(saveRoot), "controller-device.json"); preference = new Preference(); LastError = null;
        if (File.Exists(file))
        {
            try { preference = ReadPreference(file); }
            catch (Exception error)
            {
                LastError = "Could not load the controller selection. " + error.Message;
                if (File.Exists(file + ".previous")) try { preference = ReadPreference(file + ".previous"); LastError = "Recovered the previous controller selection."; } catch (Exception) { }
            }
        }
        for (int i = 0; i < xbox.Length; ++i)
        {
            var device = new Device { slot = i, profile = "xinput:slot:" + i, model = "xinput" };
            device.choice.key = "xinput:" + i; device.choice.label = "Xbox controller — slot " + (i + 1);
            AddStandardControls(device); xbox[i] = device; devices.Add(device);
        }
        initialized = true; dirty = restoreSelection = true; nextScan = 0; xinputAvailable = true;
        InputSystem.onDeviceChange += OnDeviceChange;
        Tick(true);
    }
    public void Dispose()
    {
        if (initialized) InputSystem.onDeviceChange -= OnDeviceChange;
        initialized = false; active = automaticResume = null;
    }
    private void OnDeviceChange(InputDevice device, InputDeviceChange change) { dirty = true; }
    public bool Select(string key)
    {
        if (!initialized) { LastError = "Controller selection is not initialized."; return false; }
        if (string.IsNullOrEmpty(key)) { LastError = "Choose a controller from the device list."; return false; }
        Device selected = Find(key);
        if (key != "automatic" && key != "keyboard" && selected == null && key != preference.key)
        { LastError = "That controller is no longer in the device list."; return false; }
        var candidate = new Preference { key = key, profile = selected?.profile ?? "", label = selected?.choice.label ?? "", model = selected?.model ?? "", generic = selected?.generic ?? false };
        if (Specific && key == preference.key && selected == null) candidate = preference;
        string temporary = file + "." + Guid.NewGuid().ToString("N") + ".tmp";
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(file));
            byte[] bytes = new UTF8Encoding(false).GetBytes(JsonUtility.ToJson(candidate, true));
            using (var stream = new FileStream(temporary, FileMode.CreateNew, FileAccess.Write, FileShare.None))
            { stream.Write(bytes, 0, bytes.Length); stream.Flush(true); }
            if (File.Exists(file)) File.Replace(temporary, file, file + ".previous"); else File.Move(temporary, file);
        }
        catch (Exception error)
        {
            try { if (File.Exists(temporary)) File.Delete(temporary); } catch (Exception) { }
            LastError = "Could not save the controller selection. " + error.Message; return false;
        }
        preference = candidate; LastError = null; dirty = true; restoreSelection = false;
        if (key != "automatic") automaticResume = null;
        // An explicit choice may change source even while automatic switching
        // is locked for a menu/capture. Keyboard-only must clear it immediately.
        SetActive(key == "keyboard" ? null : selected != null && selected.choice.connected ? selected :
            active != null && active.choice.connected ? active : FirstConnected());
        RebuildChoices(); return true;
    }
    public void Tick(bool allowAutoSwitch)
    {
        if (!initialized) return;
        double time = now(); bool scan = dirty || time >= nextScan;
        if (scan) nextScan = time + .5;
        bool rawConnected = false;
        foreach (var device in xbox)
        {
            if (device.choice.connected || scan)
            {
                bool connected = PollXInput(device.slot, out var pad);
                if (connected != device.choice.connected) { dirty = true; device.firstValues = true; }
                device.choice.connected = connected; device.pad = pad;
            }
            rawConnected |= device.choice.connected;
        }
        if (scan || dirty) RefreshUnityDevices(rawConnected);
        Device activity = null;
        foreach (var device in devices)
        {
            if (!device.choice.connected) continue;
            ReadDevice(device);
            if (ObserveActivity(device)) activity = device;
        }
        if (preference.key == "keyboard") SetActive(null);
        else if (Specific)
        {
            var selected = Find(preference.key);
            if (selected != null && selected.choice.connected) SetActive(selected);
            else
            {
                // A saved choice must not strand players after unplugging a
                // controller or changing its connection. Keep its preference
                // and use another device's own bindings until it returns.
                // Retain a live fallback across frames so release/capture
                // state is not reset continuously. Menus lock a live source,
                // but an absent source can still be replaced for recovery.
                var fallback = active != null && active.choice.connected ? active : null;
                SetActive((allowAutoSwitch ? activity : null) ?? fallback ?? FirstConnected());
            }
        }
        else if (active != null && !active.choice.connected) SetActive(null);
        // A menu/capture locks source switching, not reconnection of the same
        // source. Retain its identity when removed so controller-only users
        // can resume without first closing the menu with a keyboard.
        if (preference.key == "automatic" && active == null && automaticResume != null)
        {
            var resumed = Find(automaticResume.choice.key);
            if (resumed != null && resumed.choice.connected && resumed.profile == automaticResume.profile)
                SetActive(resumed);
        }
        if (preference.key == "automatic" && allowAutoSwitch)
            SetActive(activity ?? active ?? FirstConnected());
        if (dirty) { RebuildChoices(); dirty = false; }
    }
    public bool TryRead(out Idas3ControlBindings.PadState state)
    {
        state = active != null && active.choice.connected ? active.pad : default;
        return state.connected;
    }
    private bool PollXInput(int slot, out Idas3ControlBindings.PadState pad)
    {
        pad = default; if (!xinputAvailable) return false;
        try
        {
            if (readXInput((uint)slot, out var raw) != 0) return false;
            pad = new Idas3ControlBindings.PadState { connected = true, buttons = raw.gamepad.buttons,
                leftTrigger = raw.gamepad.leftTrigger, rightTrigger = raw.gamepad.rightTrigger,
                thumbLX = raw.gamepad.thumbLX, thumbLY = raw.gamepad.thumbLY, thumbRX = raw.gamepad.thumbRX, thumbRY = raw.gamepad.thumbRY };
            return true;
        }
        catch (DllNotFoundException) { xinputAvailable = false; return false; }
        catch (EntryPointNotFoundException) { xinputAvailable = false; return false; }
    }
    private void RefreshUnityDevices(bool rawConnected)
    {
        foreach (var entry in unityDevices.Values) entry.seen = false;
        var newlyConnected = new List<Device>();
        foreach (var input in InputSystem.devices)
        {
            if (!input.added || !input.enabled || (discoveryFilter != null && !discoveryFilter(input)) || !GamingDevice(input)) continue;
            // Windows Input System mirrors the same XInput ports. The native
            // API is authoritative for these four pads and preserves every bit.
            if (rawConnected && (input is XInputControllerWindows || string.Equals(input.description.interfaceName, "XInput", StringComparison.OrdinalIgnoreCase))) continue;
            if (!unityDevices.TryGetValue(input.deviceId, out var device))
            {
                device = CreateDevice(input);
                // Some inexpensive HID devices report the same placeholder
                // serial. Keep simultaneous devices independently selectable.
                var duplicate = Find(device.choice.key);
                if (duplicate != null && duplicate.choice.connected && duplicate.unity != null && duplicate.unity.added && duplicate.unity.enabled) device.choice.key += ":device:" + input.deviceId;
                unityDevices.Add(input.deviceId, device); devices.Add(device);
            }
            if (!device.choice.connected) { device.firstValues = true; newlyConnected.Add(device); dirty = true; }
            device.seen = device.choice.connected = true;
        }
        foreach (var device in unityDevices.Values)
            if (!device.seen && device.choice.connected) { device.choice.connected = false; device.pad = default; device.firstValues = true; dirty = true; }
        if (preference.key == "automatic" && automaticResume != null && !automaticResume.choice.connected)
        {
            Device candidate = null; int matches = 0;
            // Serial-less devices can change runtime ID on reconnect. Only
            // a newly connected unique matching profile is eligible; never
            // substitute another matching controller that was already live.
            foreach (var device in newlyConnected)
                if (device.profile == automaticResume.profile && device.model == automaticResume.model)
                { candidate = device; ++matches; }
            if (matches == 1) automaticResume = candidate;
        }
        if (Specific && (Find(preference.key) == null || !Find(preference.key).choice.connected))
        {
            Device candidate = null; int matches = 0;
            // With no hardware serial, an existing second pad of the same
            // model must never take over an unplugged selected controller.
            // At startup a single matching model can recover the saved choice;
            // later only a newly connected matching device is a candidate.
            foreach (var device in restoreSelection ? devices : newlyConnected)
                if (device.choice.connected && device.profile == preference.profile && device.model == preference.model)
                { candidate = device; ++matches; }
            if (matches == 1 && candidate != null)
            { preference.key = candidate.choice.key; preference.label = candidate.choice.label; dirty = true; }
        }
        restoreSelection = false;
    }
    private static bool GamingDevice(InputDevice input)
    {
        if (input is Keyboard || input is Pointer || input is Sensor) return false;
        if (input is Gamepad || input is Joystick) return true;
        if (input is HID hid)
        {
            var d = hid.hidDescriptor;
            return d.usagePage == HID.UsagePage.GenericDesktop && (d.usage == (int)HID.GenericDesktop.Joystick || d.usage == (int)HID.GenericDesktop.Gamepad || d.usage == (int)HID.GenericDesktop.MultiAxisController);
        }
        return false;
    }
    private static Device CreateDevice(InputDevice input)
    {
        var description = input.description; HidIdentity ids = null;
        if (!string.IsNullOrEmpty(description.capabilities)) try { ids = JsonUtility.FromJson<HidIdentity>(description.capabilities); } catch (Exception) { }
        string model = Fingerprint(input.layout + "|" + description.interfaceName + "|" + description.manufacturer + "|" + description.product + "|" + (ids == null ? "" : ids.vendorId + ":" + ids.productId + ":" + ids.usagePage + ":" + ids.usage));
        string profile = "unity:" + model + (string.IsNullOrEmpty(description.serial) ? "" : ":serial:" + Fingerprint(description.serial));
        var device = new Device { unity = input, generic = !(input is Gamepad), profile = profile, model = model,
            vendorId=ids!=null&&ids.vendorId>0?(uint)ids.vendorId:0,productId=ids!=null&&ids.productId>0?(uint)ids.productId:0 };
        device.choice.key = profile + (string.IsNullOrEmpty(description.serial) ? ":device:" + input.deviceId : "");
        string label = string.IsNullOrWhiteSpace(description.product) ? input.displayName : description.product;
        if (string.IsNullOrWhiteSpace(label)) label = input.layout;
        device.choice.label = label + (device.generic ? " (joystick/HID)" : " (gamepad)") + " — device " + input.deviceId;
        var elements = AutoHidInputElements(input);
        device.descriptorControls = elements != null;
        foreach (var control in input.allControls)
        {
            if (!(control is AxisControl axis) || control.noisy || control.synthetic) continue;
            string path = control.path.Substring(input.path.Length + 1);
            if (elements != null && !DescribedHidControl(input, axis, path, elements)) continue;
            bool trigger = input is Gamepad && (path == "leftTrigger" || path == "rightTrigger");
            bool button = axis is ButtonControl && !trigger;
            float minimum = button || trigger ? 0 : -1, maximum = 1;
            if (!button && !trigger && axis.normalize && axis.normalizeZero <= axis.normalizeMin) minimum = 0;
            if (axis.invert && minimum == 0 && !button && !trigger) { minimum = -1; maximum = 0; }
            string name = string.IsNullOrWhiteSpace(control.displayName) ? path : control.displayName;
            device.controls.Add(new Idas3ControllerControl { path = path, label = name, minimum = minimum, maximum = maximum, button = button });
            device.axes.Add(axis);
        }
        device.activityBaseline = new float[device.controls.Count]; return device;
    }
    private static HID.HIDElementDescriptor[] AutoHidInputElements(InputDevice input)
    {
        // Unity's generated HID joystick layout inherits a default trigger
        // and stick even when an axis-only pedal descriptor has no buttons.
        // Explicit layouts may intentionally remap report fields; leave those
        // and specialized gamepad layouts under their own control definitions.
        if (input is Gamepad || !input.layout.StartsWith("HID::", StringComparison.Ordinal) ||
            !string.Equals(input.description.interfaceName, "HID", StringComparison.OrdinalIgnoreCase) ||
            string.IsNullOrEmpty(input.description.capabilities)) return null;
        try { return HID.HIDDeviceDescriptor.FromJson(input.description.capabilities).elements; }
        catch (Exception) { return null; }
    }
    private static bool HidAxisUsage(int usage)
    {
        switch ((HID.GenericDesktop)usage)
        {
            case HID.GenericDesktop.X: case HID.GenericDesktop.Y: case HID.GenericDesktop.Z:
            case HID.GenericDesktop.Rx: case HID.GenericDesktop.Ry: case HID.GenericDesktop.Rz:
            case HID.GenericDesktop.Vx: case HID.GenericDesktop.Vy: case HID.GenericDesktop.Vz:
            case HID.GenericDesktop.Vbrx: case HID.GenericDesktop.Vbry: case HID.GenericDesktop.Vbrz:
            case HID.GenericDesktop.Slider: case HID.GenericDesktop.Dial: case HID.GenericDesktop.Wheel: return true;
            default: return false;
        }
    }
    private static bool DescribedHidControl(InputDevice input, AxisControl control, string path, HID.HIDElementDescriptor[] elements)
    {
        // Input control offsets include the device's global state-buffer
        // offset; descriptor offsets are relative to its input report. Report
        // IDs are already included in the descriptor's reportOffsetInBits.
        long offset = ((long)control.stateBlock.byteOffset - input.stateBlock.byteOffset) * 8 + control.stateBlock.bitOffset;
        long size = control.stateBlock.sizeInBits;
        if (offset < 0 || size <= 0) return false;
        bool button = control is ButtonControl;
        foreach (var element in elements)
        {
            if (element.reportType != HID.HIDReportType.Input || element.isConstant ||
                element.reportOffsetInBits < 0 || element.reportSizeInBits <= 0) continue;
            long start = element.reportOffsetInBits, end = start + element.reportSizeInBits;
            if (element.usagePage == HID.UsagePage.GenericDesktop && element.usage == (int)HID.GenericDesktop.HatSwitch)
            {
                // A real hat is one descriptor field shared by four decoded
                // directional buttons. Its synthetic X/Y are skipped above.
                if (button && control.parent is DpadControl && offset >= start && offset + size <= end) return true;
                continue;
            }
            if (offset != start || size != element.reportSizeInBits) continue;
            if (element.usagePage == HID.UsagePage.Button)
            {
                if (button && (path != "trigger" || element.usage == 1)) return true;
                continue;
            }
            if (element.usagePage != HID.UsagePage.GenericDesktop) continue;
            if (!button && HidAxisUsage(element.usage))
            {
                if (path == "stick/x" && element.usage != (int)HID.GenericDesktop.X) continue;
                if (path == "stick/y" && element.usage != (int)HID.GenericDesktop.Y) continue;
                return true;
            }
            if (button && path != "trigger") switch ((HID.GenericDesktop)element.usage)
            {
                case HID.GenericDesktop.Select: case HID.GenericDesktop.Start:
                case HID.GenericDesktop.DpadUp: case HID.GenericDesktop.DpadDown:
                case HID.GenericDesktop.DpadLeft: case HID.GenericDesktop.DpadRight: return true;
            }
        }
        return false;
    }
    private static void AddStandardControls(Device device)
    {
        string[] names = { "dpad/up", "dpad/down", "dpad/left", "dpad/right", "start", "select", "leftStickPress", "rightStickPress", "leftShoulder", "rightShoulder", "buttonSouth", "buttonEast", "buttonWest", "buttonNorth", "leftTrigger", "rightTrigger", "leftStick/x", "leftStick/y", "rightStick/x", "rightStick/y" };
        string[] labels = { "D-pad up", "D-pad down", "D-pad left", "D-pad right", "Start", "Back", "Left stick press", "Right stick press", "Left shoulder", "Right shoulder", "A / south", "B / east", "X / west", "Y / north", "Left trigger", "Right trigger", "Left stick X", "Left stick Y", "Right stick X", "Right stick Y" };
        for (int i = 0; i < names.Length; ++i) device.controls.Add(new Idas3ControllerControl { path = names[i], label = labels[i], minimum = i < 16 ? 0 : -1, maximum = 1, button = i < 14 });
        device.activityBaseline = new float[names.Length];
    }
    private static readonly ushort[] ButtonMasks = { 1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200, 0x1000, 0x2000, 0x4000, 0x8000 };
    private static float Signed(short value) => value < 0 ? value / 32768f : value / 32767f;
    private static short Stick(float value) => (short)Mathf.Clamp(Mathf.RoundToInt(value * (value < 0 ? 32768 : 32767)), -32768, 32767);
    private static void ReadDevice(Device device)
    {
        if (device.slot >= 0)
        {
            for (int i = 0; i < ButtonMasks.Length; ++i) device.controls[i].value = (device.pad.buttons & ButtonMasks[i]) != 0 ? 1 : 0;
            device.controls[14].value = device.pad.leftTrigger / 255f; device.controls[15].value = device.pad.rightTrigger / 255f;
            device.controls[16].value = Signed(device.pad.thumbLX); device.controls[17].value = Signed(device.pad.thumbLY);
            device.controls[18].value = Signed(device.pad.thumbRX); device.controls[19].value = Signed(device.pad.thumbRY);
            return;
        }
        for (int i = 0; i < device.controls.Count; ++i)
        {
            var control = device.controls[i]; float value = device.axes[i].ReadUnprocessedValue();
            control.value = float.IsNaN(value) || float.IsInfinity(value) ? 0 : control.button ? value >= .5f ? 1 : 0 : Mathf.Clamp(value, control.minimum, control.maximum);
        }
        if (device.unity is Gamepad gamepad) { device.pad = Idas3GamepadInput.ReadUnityPad(gamepad); return; }
        var state = new Idas3ControlBindings.PadState { connected = true };
        // Descriptor-filtered HIDs must not reintroduce an inherited trigger
        // or stick via Joystick's convenience properties below.
        if (!device.descriptorControls && device.unity is Joystick joystick)
        {
            if (joystick.stick != null) { var stick = joystick.stick.ReadUnprocessedValue(); state.thumbLX = Stick(stick.x); state.thumbLY = Stick(stick.y); }
            if (joystick.trigger != null && joystick.trigger.isPressed) state.buttons |= 0x1000;
            if (joystick.hatswitch != null)
            { var hat = joystick.hatswitch.ReadUnprocessedValue(); if (hat.y > .5f) state.buttons |= 1; if (hat.y < -.5f) state.buttons |= 2; if (hat.x < -.5f) state.buttons |= 4; if (hat.x > .5f) state.buttons |= 8; }
        }
        int ordinal = 0;
        for (int i = 0; i < device.controls.Count; ++i)
        {
            var control = device.controls[i];
            if (control.path == "stick/x") state.thumbLX = Stick(control.value);
            if (control.path == "stick/y") state.thumbLY = Stick(control.value);
            if (!control.button) continue;
            string path = control.path;
            if (device.descriptorControls)
            {
                if (device.axes[i].parent is DpadControl) path = "dpad/" + device.axes[i].name;
                else if (path == "dpadUp") path = "dpad/up";
                else if (path == "dpadDown") path = "dpad/down";
                else if (path == "dpadLeft") path = "dpad/left";
                else if (path == "dpadRight") path = "dpad/right";
            }
            if (path == "dpad/up" && control.value > .5f) state.buttons |= 1;
            else if (path == "dpad/down" && control.value > .5f) state.buttons |= 2;
            else if (path == "dpad/left" && control.value > .5f) state.buttons |= 4;
            else if (path == "dpad/right" && control.value > .5f) state.buttons |= 8;
            else if (path == "start" && control.value > .5f) state.buttons |= 0x10;
            else if (path == "select" && control.value > .5f) state.buttons |= 0x20;
            else if (path == "start" || path == "select") continue;
            else if (!path.StartsWith("dpad/", StringComparison.Ordinal))
            { if (control.value > .5f && ordinal < 2) state.buttons |= ordinal == 0 ? (ushort)0x1000 : (ushort)0x2000; ++ordinal; }
        }
        device.pad = state;
    }
    private static bool ObserveActivity(Device device)
    {
        bool active = false;
        for (int i = 0; i < device.controls.Count; ++i)
        {
            var control = device.controls[i]; float prior = device.activityBaseline[i];
            if (!device.firstValues && (control.button ? control.value > .5f && prior <= .5f : Math.Abs(control.value - prior) > .18f)) active = true;
            // Axes accumulate small deliberate movements, but a stationary
            // pedal resting at -1 cannot repeatedly steal automatic selection.
            if (device.firstValues || control.button || Math.Abs(control.value - prior) > .18f) device.activityBaseline[i] = control.value;
        }
        device.firstValues = false; return active;
    }
    private Device Find(string key)
    { Device missing = null; foreach (var d in devices) if (d.choice.key == key) { if (d.choice.connected) return d; missing = d; } return missing; }
    private Device FirstConnected() { foreach (var d in devices) if (d.choice.connected) return d; return null; }
    private void SetActive(Device device)
    {
        if (preference.key == "automatic" && device != null) automaticResume = device;
        if (ReferenceEquals(active, device)) return;
        active = device; ActiveDeviceChanged?.Invoke();
    }
    private void RebuildChoices()
    {
        choices.Clear(); choices.Add(automaticChoice); choices.Add(keyboardChoice);
        bool selectedListed = false;
        foreach (var device in devices)
            if (device.choice.connected)
            { choices.Add(device.choice); selectedListed |= device.choice.key == preference.key; }
        if (Specific && !selectedListed)
        { missingChoice.key = preference.key; missingChoice.label = string.IsNullOrEmpty(preference.label) ? "Selected controller" : preference.label; missingChoice.connected = false; choices.Add(missingChoice); }
    }
    private static string Fingerprint(string value)
    {
        using (var sha = SHA256.Create())
        { var bytes = sha.ComputeHash(Encoding.UTF8.GetBytes(value ?? "")); var text = new StringBuilder(32); for (int i = 0; i < 16; ++i) text.Append(bytes[i].ToString("x2")); return text.ToString(); }
    }
    private static Preference ReadPreference(string path)
    {
        if (new FileInfo(path).Length > 32768) throw new InvalidDataException("Controller selection file is too large.");
        var value = JsonUtility.FromJson<Preference>(File.ReadAllText(path));
        if (value == null || value.version != 1 || string.IsNullOrEmpty(value.key) || value.key.Length > 512 ||
            (value.key != "automatic" && value.key != "keyboard" && !value.key.StartsWith("unity:", StringComparison.Ordinal) && !value.key.StartsWith("xinput:", StringComparison.Ordinal)))
            throw new InvalidDataException("Invalid controller selection.");
        value.profile = value.profile ?? ""; value.label = value.label ?? ""; value.model = value.model ?? "";
        if (value.profile.Length > 512 || value.label.Length > 512 || value.model.Length > 512) throw new InvalidDataException("Controller identity is too long.");
        return value;
    }
}
