using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.InputSystem.EnhancedTouch;
using Touch = UnityEngine.InputSystem.EnhancedTouch.Touch;
using TouchPhase = UnityEngine.InputSystem.TouchPhase;
using Sensor = UnityEngine.InputSystem.Sensor;
using GravitySensor = UnityEngine.InputSystem.GravitySensor;
using Accelerometer = UnityEngine.InputSystem.Accelerometer;

// No synthetic Gamepad is registered: Xiaomi's built-in uinput device and real
// Bluetooth controllers must not steal focus from the on-screen controls.
// The host merges these actions AFTER user hardware bindings, BEFORE modal,
// pause and multiplayer input ownership checks.
[DefaultExecutionOrder(-1000)]
public sealed class Idas3TouchControls : MonoBehaviour
{
    internal enum Mode { Off, Menu, Driving, Story }
    private enum Button { Steer, Gas, Brake, ShiftUp, ShiftDown, Camera, Lights,
        Up, Down, Left, Right, Confirm, Back, Pause, Online, Fade, Skip, Tilt, Count }
    private static Idas3TouchControls instance;
    private readonly State state = new State();
    private readonly Rect[] boxes = new Rect[(int)Button.Count];
    private Rect lastSafeArea, gauge;
    private Vector2 lastScreen;
    private Mode mode;
    private bool enhancedEnabled, visible, focused = true, suspended, lastTiltLayout;
    private int lastTouchFrame = -10, lastSize;
    private float scale = 1;
    private bool tiltMissing, tiltPrimed;
    private Sensor tiltSensor;
    private Vector3 tiltFiltered;
    private GUIStyle label, caption, small;
    private Texture2D rounded, disc, ring;
    private string pauseLabel = "MENU";
    private static float opacity => Current.opacity;
    internal static bool SuppressMouse => instance != null && instance.visible &&
        (instance.state.HasContacts || Time.frameCount <= instance.lastTouchFrame + 1);
    internal bool PauseHeld => state.Held(Button.Pause);
    internal bool PausePressed => state.Pressed(Button.Pause);
    internal bool OnlineHeld => state.Held(Button.Online);
    internal bool CameraHeld => state.Held(Button.Camera);

    // Edited by the TOUCH settings page (draft + APPLY) and by the in-race
    // TILT shortcut. Stored per device in PlayerPrefs, not in game-options.json.
    internal struct Settings
    {
        public bool tilt, invertTilt;
        public int tiltRange, buttonSize; // degrees for full lock; percent
        public float opacity;
        public static Settings Defaults => new Settings { tiltRange = 30, buttonSize = 100, opacity = .65f };
        public Settings Normalized()
        {
            var value = this;
            value.tiltRange = Mathf.Clamp(Mathf.RoundToInt(tiltRange / 5f) * 5, 15, 45);
            value.buttonSize = Mathf.Clamp(Mathf.RoundToInt(buttonSize / 10f) * 10, 80, 130);
            value.opacity = float.IsNaN(opacity) ? .65f : Mathf.Clamp(Mathf.Round(opacity * 20) / 20, .25f, .9f);
            return value;
        }
        public bool Same(Settings other) { var a = Normalized(); var b = other.Normalized();
            return a.tilt == b.tilt && a.invertTilt == b.invertTilt && a.tiltRange == b.tiltRange && a.buttonSize == b.buttonSize && Mathf.Approximately(a.opacity, b.opacity); }
    }
    private static Settings? settings;
    internal static Settings Current
    {
        get
        {
            if (settings == null)
            {
                var value = Settings.Defaults;
                value.opacity = PlayerPrefs.GetFloat("idas3.touch.opacity", value.opacity);
                // Earlier test builds stored 0 off, 1 on (30 degrees), 2 high (20 degrees).
                int legacy = PlayerPrefs.GetInt("idas3.touch.tilt", 0);
                value.tilt = PlayerPrefs.GetInt("idas3.touch.tiltOn", legacy > 0 ? 1 : 0) != 0;
                value.tiltRange = PlayerPrefs.GetInt("idas3.touch.tiltRange", legacy == 2 ? 20 : value.tiltRange);
                value.invertTilt = PlayerPrefs.GetInt("idas3.touch.tiltInvert", 0) != 0;
                value.buttonSize = PlayerPrefs.GetInt("idas3.touch.size", value.buttonSize);
                settings = value.Normalized();
            }
            return settings.Value;
        }
    }
    internal static void Save(Settings value)
    {
        value = value.Normalized(); settings = value;
        PlayerPrefs.SetFloat("idas3.touch.opacity", value.opacity);
        PlayerPrefs.SetInt("idas3.touch.tiltOn", value.tilt ? 1 : 0);
        PlayerPrefs.SetInt("idas3.touch.tiltRange", value.tiltRange);
        PlayerPrefs.SetInt("idas3.touch.tiltInvert", value.invertTilt ? 1 : 0);
        PlayerPrefs.SetInt("idas3.touch.size", value.buttonSize);
        PlayerPrefs.DeleteKey("idas3.touch.tilt");
        PlayerPrefs.Save();
    }
    internal static string TiltSensorName => GravitySensor.current != null ? "GRAVITY SENSOR" : Accelerometer.current != null ? "ACCELEROMETER" : null;

    internal static Mode SelectMode(uint flags, int racePhase, bool modal)
    {
        if (modal) return Mode.Menu;
        // The native owners (rival dialogue, Legend visit, extra-mode visit)
        // consume held Start for skipping. Never send it to a title/menu, a
        // loading owner, a Legend Yes/No choice, or the driving pause handler.
        if ((flags & (32u | 512u | 4096u)) != 0 && (flags & (64u | 1024u)) == 0) return Mode.Story;
        return (flags & (1u | 32u | 512u | 1024u | 4096u)) == 0 && racePhase < 3 ? Mode.Driving : Mode.Menu;
    }

    private void OnEnable()
    {
        if (Application.platform != RuntimePlatform.Android && !Application.isEditor) { enabled = false; return; }
        instance = this;
        EnhancedTouchSupport.Enable(); enhancedEnabled = true;
    }
    private void OnDisable()
    {
        ResetInput();
        SetTiltSensor(false);
        if (enhancedEnabled) { EnhancedTouchSupport.Disable(); enhancedEnabled = false; }
        if (instance == this) instance = null;
    }
    private void OnDestroy()
    {
        if (rounded != null) Destroy(rounded);
        if (disc != null) Destroy(disc);
        if (ring != null) Destroy(ring);
    }
    private void OnApplicationFocus(bool value) { focused = value; if (!value) ResetInput(); }
    private void OnApplicationPause(bool value) { suspended = value; if (value) { ResetInput(); SetTiltSensor(false); } }
    internal void ResetInput() { state.Clear(); visible = false; }

    internal void Poll(Mode requestedMode, int context, bool allowed, string menuLabel)
    {
        visible = allowed && focused && !suspended && requestedMode != Mode.Off;
        if (!visible) { state.Clear(); return; }
        pauseLabel = menuLabel;
        Rect safe = Screen.safeArea;
        if (safe.width <= 0 || safe.height <= 0) safe = new Rect(0, 0, Screen.width, Screen.height);
        Vector2 screen = new Vector2(Screen.width, Screen.height);
        var options = Current;
        bool tiltLayout = requestedMode == Mode.Driving && options.tilt && TiltSource() != null;
        bool reshaped = mode != requestedMode || lastSafeArea != safe || lastScreen != screen || lastSize != options.buttonSize;
        if (reshaped || tiltLayout != lastTiltLayout)
        {
            // Pedal fingers survive a TILT toggle; only the removed wheel is released.
            if (reshaped) state.Clear(); else state.Release(Button.Steer);
            mode = requestedMode; lastSafeArea = safe; lastScreen = screen; lastTiltLayout = tiltLayout; lastSize = options.buttonSize;
            Layout(boxes, mode, tiltLayout, options.buttonSize / 100f, safe, Screen.height, out scale, out gauge);
        }
        state.BeginFrame(mode, context);
        var touches = Touch.activeTouches;
        for (int i = 0; i < touches.Count; ++i)
        {
            var touch = touches[i];
            Vector2 point = new Vector2(touch.screenPosition.x, Screen.height - touch.screenPosition.y);
            bool ended = touch.phase == TouchPhase.Ended || touch.phase == TouchPhase.Canceled;
            Button hit = Hit(boxes, point);
            state.Contact(touch.touchId, point, touch.phase == TouchPhase.Began, ended, hit, boxes[(int)Button.Steer]);
        }
        state.EndFrame();
        if (state.Pressed(Button.Skip))
            Debug.Log("IDAS3 touch: story skip " + (state.Skipping ? "requested" : "cancelled") + ", context=" + context);
        if (state.TouchedThisFrame) lastTouchFrame = Time.frameCount;
        if (state.Pressed(Button.Tilt)) ToggleTilt();
        UpdateTilt(tiltLayout);
    }

    private static Sensor TiltSource() => (Sensor)GravitySensor.current ?? Accelerometer.current;
    private void ToggleTilt()
    {
        tiltMissing = TiltSource() == null;
        var value = Current; value.tilt = !tiltMissing && !value.tilt; Save(value);
        Debug.Log("IDAS3 touch: tilt steering " + (tiltMissing ? "unavailable (no gravity sensor or accelerometer)" : value.tilt ? "on" : "off"));
    }
    // Android sensors are disabled until requested; keep them off while
    // tilt is off or the app is in the background.
    private void SetTiltSensor(bool wanted)
    {
        if (tiltSensor != null && !tiltSensor.added) tiltSensor = null;
        if (wanted == (tiltSensor != null)) return;
        tiltPrimed = false;
        if (wanted)
        {
            tiltSensor = TiltSource();
            if (tiltSensor == null) return;
            if (!tiltSensor.enabled) UnityEngine.InputSystem.InputSystem.EnableDevice(tiltSensor);
            Debug.Log("IDAS3 touch: tilt steering uses " + tiltSensor.GetType().Name);
        }
        else
        {
            if (tiltSensor.enabled) UnityEngine.InputSystem.InputSystem.DisableDevice(tiltSensor);
            tiltSensor = null;
        }
    }
    private void UpdateTilt(bool active)
    {
        var options = Current;
        SetTiltSensor(options.tilt);
        if (!active || tiltSensor == null) { state.SetTilt(false, 0); return; }
        Vector3 raw = tiltSensor is GravitySensor gravity ? gravity.gravity.ReadValue() : ((Accelerometer)tiltSensor).acceleration.ReadValue();
        if (!UnityEngine.InputSystem.InputSystem.settings.compensateForScreenOrientation) raw = ScreenSpace(raw, Screen.orientation);
        // A freshly enabled sensor reports zero until its first sample.
        if (raw.sqrMagnitude < .25f) { state.SetTilt(false, 0); return; }
        float tau = tiltSensor is GravitySensor ? .045f : .1f;
        tiltFiltered = tiltPrimed ? Vector3.Lerp(tiltFiltered, raw, 1 - Mathf.Exp(-Time.unscaledDeltaTime / tau)) : raw;
        tiltPrimed = true;
        float steering = TiltSteering(TiltDegrees(tiltFiltered), options.tiltRange);
        state.SetTilt(true, options.invertTilt ? -steering : steering);
    }

    // Same rotation as the Input System's CompensateDirection processor:
    // device axes -> screen axes (x right, y up), vector pointing to the ground.
    internal static Vector3 ScreenSpace(Vector3 device, ScreenOrientation orientation)
    {
        switch (orientation)
        {
            case ScreenOrientation.PortraitUpsideDown: return Quaternion.Euler(0, 0, 180) * device;
            case ScreenOrientation.LandscapeLeft: return Quaternion.Euler(0, 0, 90) * device;
            case ScreenOrientation.LandscapeRight: return Quaternion.Euler(0, 0, 270) * device;
            default: return device;
        }
    }
    // Clockwise (right edge down) is positive. Turning the phone like a wheel
    // rotates gravity in the screen plane, independent of how far it leans
    // back; near flat that angle is undefined, so side roll takes over.
    internal static float TiltDegrees(Vector3 gravity)
    {
        float length = gravity.magnitude;
        if (length < .3f) return 0;
        float planar = Mathf.Sqrt(gravity.x * gravity.x + gravity.y * gravity.y) / length;
        float wheel = Mathf.Atan2(gravity.x, -gravity.y) * Mathf.Rad2Deg;
        float roll = Mathf.Asin(Mathf.Clamp(gravity.x / length, -1, 1)) * Mathf.Rad2Deg;
        return Mathf.Lerp(roll, wheel, Mathf.InverseLerp(.25f, .55f, planar));
    }
    // A small angular dead zone hides hand tremor; the native steering dead
    // zone and smoothing options still apply to the resulting stick value.
    internal static float TiltSteering(float degrees, float fullLock)
    {
        const float dead = 1.5f;
        float amount = Mathf.Abs(degrees);
        if (amount <= dead) return 0;
        return Mathf.Sign(degrees) * Mathf.Clamp01((amount - dead) / (fullLock - dead));
    }

    internal void Apply(ref Idas3Native.FrameInput frame, bool allowPause)
    {
        if (visible) state.Apply(ref frame, mode, allowPause);
    }

    private static Button Hit(Rect[] bounds, Vector2 point)
    {
        for (int i = 0; i < bounds.Length; ++i)
            if (bounds[i].width > 0 && bounds[i].Contains(point)) return (Button)i;
        return Button.Count;
    }

    private static void Layout(Rect[] bounds, Mode mode, bool tilt, float size, Rect safe, float screenHeight, out float scale, out Rect gauge)
    {
        Array.Clear(bounds, 0, bounds.Length);
        gauge = Rect.zero;
        // BUTTON SIZE scales targets and their gaps together; 130% still fits
        // a 16:9 safe area. Opacity moved to Options > TOUCH (no FADE button).
        scale = Mathf.Min(safe.width / 1280f, safe.height / 720f) * Mathf.Clamp(size, .8f, 1.3f);
        float s = scale, left = safe.xMin + 28*s, right = safe.xMax - 28*s;
        float top = screenHeight - safe.yMax + 20*s, bottom = screenHeight - safe.yMin - 30*s;
        void Set(Button key, float x, float y, float w, float h) { bounds[(int)key] = new Rect(x,y,w*s,h*s); }
        if (mode != Mode.Story) Set(Button.Pause, right-112*s, top, 112, 52);
        else Set(Button.Skip, right-180*s, top, 180, 62);
        if (mode == Mode.Driving)
        {
            if (tilt)
            {
                // Both thumbs are free: slowing down on the left, speeding up on the right.
                Set(Button.Brake, left, bottom-170*s, 150, 170);
                Set(Button.ShiftDown, left, bottom-262*s, 150, 76);
                Set(Button.Gas, right-150*s, bottom-170*s, 150, 170);
                Set(Button.ShiftUp, right-150*s, bottom-262*s, 150, 76);
                gauge = new Rect(safe.center.x-160*s, bottom-44*s, 320*s, 44*s);
            }
            else
            {
                Set(Button.Steer, left, bottom-218*s, 218, 218);
                Set(Button.Gas, right-100*s, bottom-164*s, 100, 164);
                Set(Button.Brake, right-220*s, bottom-134*s, 100, 134);
                Set(Button.ShiftUp, right-100*s, bottom-254*s, 100, 68);
                Set(Button.ShiftDown, right-220*s, bottom-224*s, 100, 68);
            }
            Set(Button.Camera, right-210*s, top, 82, 52);
            Set(Button.Lights, right-308*s, top, 82, 52);
            Set(Button.Tilt, right-436*s, top, 112, 52);
        }
        else if (mode == Mode.Menu || mode == Mode.Story)
        {
            Set(Button.Left, left, bottom-150*s, 78, 72);
            Set(Button.Right, left+174*s, bottom-150*s, 78, 72);
            Set(Button.Up, left+87*s, bottom-236*s, 78, 72);
            Set(Button.Down, left+87*s, bottom-64*s, 78, 64);
            Set(Button.Confirm, right-116*s, bottom-132*s, 116, 116);
            if (mode == Mode.Menu)
            {
                Set(Button.Back, right-242*s, bottom-98*s, 100, 82);
                Set(Button.Online, right-224*s, top, 96, 52);
            }
        }
    }

    // Contact ownership is fixed at touch-down. Dragging from a pedal onto a
    // neighbouring button cannot accidentally change gears or confirm a menu.
    private sealed class State
    {
        private struct Capture { public Button button; public Vector2 point; public int seen; }
        private readonly Dictionary<int,Capture> contacts = new Dictionary<int,Capture>();
        private readonly List<int> lost = new List<int>();
        private uint held, previous;
        private int stamp, context = int.MinValue;
        private Mode mode;
        private Rect steeringBox;
        public float Steering { get; private set; }
        public bool TiltActive { get; private set; }
        public float Tilt { get; private set; }
        public bool TouchedThisFrame { get; private set; }
        public bool Skipping { get; private set; }
        public bool HasContacts => contacts.Count > 0;
        public bool Held(Button key) => (held & (1u << (int)key)) != 0;
        public bool Pressed(Button key) => Held(key) && (previous & (1u << (int)key)) == 0;
        public void Clear() { contacts.Clear(); held = previous = 0; Steering = 0; Skipping = false; context = int.MinValue; SetTilt(false, 0); }
        public void SetTilt(bool active, float value) { TiltActive = active; Tilt = active ? Mathf.Clamp(value, -1, 1) : 0; }
        public void Release(Button key)
        {
            lost.Clear();
            foreach (var item in contacts) if (item.Value.button == key) lost.Add(item.Key);
            foreach (int id in lost) contacts.Remove(id);
        }
        public void BeginFrame(Mode nextMode, int nextContext)
        {
            if (nextMode != mode || context != nextContext) Clear();
            mode = nextMode; context = nextContext; previous = held; held = 0;
            Steering = 0; TouchedThisFrame = false; ++stamp;
        }
        public void Contact(int id, Vector2 point, bool began, bool ended, Button hit, Rect steering)
        {
            if (ended)
            {
                if (contacts.Remove(id)) TouchedThisFrame = true;
                return;
            }
            if (!contacts.TryGetValue(id, out var capture))
            {
                // A held finger after pause/focus/layout changes must be
                // lifted and pressed again; it is never a new button press.
                if (!began || hit == Button.Count || mode == Mode.Off) return;
                if (hit == Button.Steer)
                    foreach (var other in contacts.Values) if (other.button == Button.Steer) return;
                capture.button = hit;
            }
            capture.point = point; capture.seen = stamp;
            contacts[id] = capture; steeringBox = steering; TouchedThisFrame = true;
        }
        public void EndFrame()
        {
            lost.Clear();
            foreach (var item in contacts)
            {
                var capture = item.Value;
                if (capture.seen != stamp) { lost.Add(item.Key); continue; }
                held |= 1u << (int)capture.button;
                if (capture.button == Button.Steer)
                    Steering = Mathf.Clamp((capture.point.x - steeringBox.center.x) / Mathf.Max(1, steeringBox.width * .36f), -1, 1);
            }
            foreach (int id in lost) { contacts.Remove(id); TouchedThisFrame = true; }
            if (mode == Mode.Story && Pressed(Button.Skip)) Skipping = !Skipping;
        }
        public void Apply(ref Idas3Native.FrameInput frame, Mode mode, bool allowPause)
        {
            if (mode == Mode.Driving)
            {
                if (Held(Button.Steer) || Held(Button.Gas) || Held(Button.Brake)) frame.padConnected = 1;
                if (Held(Button.Steer)) { frame.thumbLX = Mathf.RoundToInt(Steering * (Steering < 0 ? 32768 : 32767)); frame.thumbLY = 0; }
                else if (TiltActive)
                {
                    frame.padConnected = 1;
                    int tilt = Mathf.RoundToInt(Tilt * (Tilt < 0 ? 32768 : 32767));
                    // A larger physical stick or wheel deflection keeps priority.
                    if (Math.Abs(tilt) >= Math.Abs(frame.thumbLX)) { frame.thumbLX = tilt; frame.thumbLY = 0; }
                }
                if (Held(Button.Gas)) frame.rightTrigger = 255;
                if (Held(Button.Brake)) frame.leftTrigger = 255;
                if (Held(Button.ShiftUp)) frame.SetKey(69);
                if (Held(Button.ShiftDown)) frame.SetKey(81);
                if (Held(Button.Camera)) frame.SetKey(67);
                if (Held(Button.Lights)) frame.SetKey(72);
            }
            else if (mode == Mode.Menu || mode == Mode.Story)
            {
                if (Held(Button.Left)) frame.SetKey(37);
                if (Held(Button.Right)) frame.SetKey(39);
                if (Held(Button.Up)) frame.SetKey(38);
                if (Held(Button.Down)) frame.SetKey(40);
                if (Held(Button.Confirm)) frame.SetKey(13);
                if (Held(Button.Back)) { frame.SetKey(8); frame.padConnected = 1; frame.padButtons |= 0x2000; }
                // START (0x10), not A/Enter. The native dialogue advances over
                // several source-clock ticks, so a tap latches until the owner
                // changes or the user taps again. BeginFrame/clear release it
                // before a new choice, pause, loading screen or race.
                if (mode == Mode.Story && Skipping) { frame.padConnected = 1; frame.padButtons |= 0x10u; }
            }
            if (Held(Button.Pause))
            {
                if (allowPause) frame.SetKey(27);
                else { frame.SetKey(8); frame.padConnected = 1; frame.padButtons |= 0x2000; }
            }
        }
    }

    private void OnGUI()
    {
        if (!visible) return;
        int oldDepth = GUI.depth; GUI.depth = -10000;
        try
        {
            var e = Event.current;
            if (e.isMouse && (SuppressMouse || Hit(boxes,e.mousePosition) != Button.Count)) { e.Use(); return; }
            if (e.type != EventType.Repaint) return;
            EnsureArt();
            label.fontSize = Mathf.RoundToInt(22*scale);
            caption.fontSize = Mathf.RoundToInt(16*scale);
            small.fontSize = Mathf.RoundToInt(16*scale);
            for (int i = 0; i < boxes.Length; ++i)
            {
                Rect rect = boxes[i]; if (rect.width <= 0) continue;
                Button button = (Button)i;
                if (button == Button.Steer) { DrawSteering(rect); continue; }
                Color accent = button == Button.Skip ? new Color(1f,.76f,.22f) :
                    button == Button.Gas || button == Button.Confirm || (button == Button.Tilt && Current.tilt) ? new Color(.3f,.9f,.75f) :
                    button == Button.Brake || button == Button.Back ? new Color(1f,.4f,.36f) : new Color(.45f,.76f,1f);
                bool down = state.Held(button) || (button == Button.Skip && state.Skipping) || (button == Button.Tilt && Current.tilt);
                Draw(rect, rounded, new Color(accent.r,accent.g,accent.b,down ? .95f : opacity*.65f));
                Rect inner = new Rect(rect.x+2*scale,rect.y+2*scale,rect.width-4*scale,rect.height-4*scale);
                Draw(inner, rounded, down ? new Color(.06f,.19f,.24f,opacity+.1f) : new Color(.025f,.04f,.055f,opacity));
                GUI.Label(rect, Text(button), button == Button.Tilt ? small : label);
            }
            if (gauge.width > 0) DrawTiltGauge(gauge);
        }
        finally { GUI.depth = oldDepth; }
    }
    private string Text(Button key)
    {
        switch(key)
        {
            case Button.Gas: return "GAS"; case Button.Brake: return "BRAKE";
            case Button.ShiftUp: return "GEAR +"; case Button.ShiftDown: return "GEAR -";
            case Button.Camera: return "VIEW"; case Button.Lights: return "LIGHTS";
            case Button.Left: return "<"; case Button.Right: return ">";
            case Button.Up: return "UP"; case Button.Down: return "DOWN";
            case Button.Confirm: return mode == Mode.Menu && pauseLabel == "OPTIONS" ? "START" : "OK"; case Button.Back: return "BACK";
            case Button.Pause: return pauseLabel; case Button.Online: return "ONLINE";
            case Button.Fade: return "FADE"; case Button.Skip: return state.Skipping ? "SKIPPING..." : "SKIP";
            case Button.Tilt: return tiltMissing ? "TILT\nN/A" : Current.tilt ? "TILT\nON" : "TILT\nOFF";
            default: return "";
        }
    }
    private void DrawTiltGauge(Rect rect)
    {
        var track = new Rect(rect.x, rect.yMax-16*scale, rect.width, 12*scale);
        Draw(track, rounded, new Color(.025f,.04f,.055f,opacity*.75f));
        Draw(new Rect(rect.center.x-1.5f*scale, track.y-4*scale, 3*scale, track.height+8*scale), Texture2D.whiteTexture, new Color(1,1,1,opacity*.6f));
        float size = 26*scale;
        var knob = new Rect(rect.center.x+state.Tilt*(rect.width-size)*.5f-size*.5f, track.center.y-size*.5f, size, size);
        Draw(knob, disc, new Color(.4f,.8f,1f,state.TiltActive ? .95f : opacity*.5f));
        GUI.Label(new Rect(rect.x, rect.y, rect.width, rect.height-20*scale), state.TiltActive ? "TILT STEER" : "TILT: waiting for sensor", caption);
    }
    private void DrawSteering(Rect rect)
    {
        Draw(rect, disc, new Color(.025f,.04f,.055f,opacity*.75f));
        Draw(rect, ring, new Color(.55f,.8f,1f,opacity));
        GUI.Label(new Rect(rect.x,rect.y+rect.height*.12f,rect.width,26*scale), "STEER", caption);
        GUI.Label(new Rect(rect.x+12*scale,rect.center.y-20*scale,30*scale,40*scale), "<", label);
        GUI.Label(new Rect(rect.xMax-42*scale,rect.center.y-20*scale,30*scale,40*scale), ">", label);
        float size=64*scale;
        var knob=new Rect(rect.center.x+state.Steering*rect.width*.30f-size*.5f,rect.center.y-size*.5f,size,size);
        Draw(knob,disc,new Color(.4f,.8f,1f,state.Held(Button.Steer) ? .95f : opacity));
    }
    private static void Draw(Rect rect, Texture2D texture, Color color)
    {
        Color old = GUI.color; GUI.color = color;
        GUI.DrawTexture(rect,texture); GUI.color = old;
    }
    private void EnsureArt()
    {
        if(label==null)
        {
            label=new GUIStyle(GUI.skin.label){alignment=TextAnchor.MiddleCenter,fontStyle=FontStyle.Bold,wordWrap=true};
            label.normal.textColor=Color.white;
            caption=new GUIStyle(label){fontStyle=FontStyle.Normal};
            small=new GUIStyle(label);
        }
        if(rounded!=null)return;
        rounded=MakeTexture(0);disc=MakeTexture(1);ring=MakeTexture(2);
    }
    private static Texture2D MakeTexture(int kind)
    {
        const int size=96;var pixels=new Color[size*size];
        for(int y=0;y<size;++y)for(int x=0;x<size;++x)
        {
            float px=x+.5f-size*.5f,py=y+.5f-size*.5f;
            float distance;
            if(kind==0){float dx=Mathf.Max(Mathf.Abs(px)-35,0),dy=Mathf.Max(Mathf.Abs(py)-35,0);distance=Mathf.Sqrt(dx*dx+dy*dy)-11;}
            else distance=Mathf.Sqrt(px*px+py*py)-45;
            float alpha=Mathf.Clamp01(1-distance);
            if(kind==2)alpha*=Mathf.Clamp01(distance+3);
            pixels[y*size+x]=new Color(1,1,1,alpha);
        }
        var texture=new Texture2D(size,size,TextureFormat.RGBA32,false){name="Touch HUD",wrapMode=TextureWrapMode.Clamp,filterMode=FilterMode.Bilinear};
        texture.SetPixels(pixels);texture.Apply(false,true);return texture;
    }

    // Tests run in the editor but exercise the exact state/ABI mapper shipped
    // to ARM64. No native game, ROM, hardware device or save file is touched.
    public static int RunSelfTests()
    {
        int checks=0;void Check(bool ok,string why){if(!ok)throw new Exception("Touch controls: "+why);++checks;}
        bool Key(Idas3Native.FrameInput f,int key){uint word=key<32?f.key0:key<64?f.key1:key<96?f.key2:f.key3;return(word&(1u<<(key&31)))!=0;}
        var s=new State();var steer=new Rect(0,0,200,200);
        s.BeginFrame(Mode.Driving,1);
        s.Contact(10,new Vector2(200,100),true,false,Button.Steer,steer);
        s.Contact(11,Vector2.zero,true,false,Button.Gas,steer);
        s.Contact(12,Vector2.zero,true,false,Button.Brake,steer);
        s.Contact(13,Vector2.zero,true,false,Button.ShiftUp,steer);s.EndFrame();
        var f=new Idas3Native.FrameInput();s.Apply(ref f,Mode.Driving,true);
        Check(f.padConnected==1&&f.thumbLX==32767,"analog full right");
        Check(f.rightTrigger==255&&f.leftTrigger==255,"independent simultaneous pedals");
        Check(Key(f,69)&&!Key(f,13),"gear up does not confirm");
        s.BeginFrame(Mode.Driving,1);
        s.Contact(10,new Vector2(-100,100),false,false,Button.Gas,steer);
        s.Contact(11,new Vector2(2000,1000),false,false,Button.ShiftDown,steer);
        s.Contact(12,Vector2.zero,false,true,Button.Brake,steer);s.EndFrame();
        f=default;s.Apply(ref f,Mode.Driving,true);
        Check(f.thumbLX==-32768,"clamped full left");
        Check(f.rightTrigger==255&&!Key(f,81),"captured pedal does not slide into gear");
        Check(f.leftTrigger==0&&!Key(f,69),"release and lost touch cleared");
        s.BeginFrame(Mode.Menu,2);
        s.Contact(11,Vector2.zero,false,false,Button.Confirm,steer);s.EndFrame();
        f=default;s.Apply(ref f,Mode.Menu,false);Check(!Key(f,13)&&f.rightTrigger==0,"context change discards held fingers");
        s.BeginFrame(Mode.Menu,2);
        s.Contact(21,Vector2.zero,true,false,Button.Confirm,steer);
        s.Contact(22,Vector2.zero,true,false,Button.Right,steer);s.EndFrame();
        f=default;s.Apply(ref f,Mode.Menu,false);Check(Key(f,13)&&Key(f,39),"menu confirm and navigation");
        s.BeginFrame(Mode.Menu,3);s.Contact(21,Vector2.zero,false,false,Button.Confirm,steer);s.EndFrame();
        Check(!s.Held(Button.Confirm),"new menu page needs a fresh press");
        s.BeginFrame(Mode.Menu,3);s.Contact(30,Vector2.zero,true,false,Button.Back,steer);s.EndFrame();
        f=default;s.Apply(ref f,Mode.Menu,false);Check(Key(f,8)&&(f.padButtons&0x2000)!=0&&!Key(f,27),"back is not app-exit Escape");
        s.Clear();s.BeginFrame(Mode.Driving,4);s.Contact(30,Vector2.zero,false,false,Button.Gas,steer);s.EndFrame();
        Check(!s.HasContacts,"focus reset cannot resume held accelerator");
        s.BeginFrame(Mode.Driving,4);s.Contact(40,Vector2.zero,true,false,Button.Pause,steer);s.EndFrame();
        Check(s.Pressed(Button.Pause),"pause press edge");f=default;s.Apply(ref f,Mode.Driving,true);Check(Key(f,27),"race pause key");
        s.BeginFrame(Mode.Driving,4);s.Contact(40,Vector2.zero,false,false,Button.Pause,steer);s.EndFrame();Check(!s.Pressed(Button.Pause),"holding pause does not repeat edges");
        s.Clear();f=new Idas3Native.FrameInput{padConnected=1,rightTrigger=99,leftTrigger=42,thumbLX=1234};s.Apply(ref f,Mode.Driving,true);
        Check(f.rightTrigger==99&&f.leftTrigger==42&&f.thumbLX==1234,"idle touch preserves hardware controls");
        Check(SelectMode(512,0,false)==Mode.Story,"rival dialogue exposes Skip");
        Check(SelectMode(32,0,false)==Mode.Story,"Legend dialogue exposes Skip");
        Check(SelectMode(4096,0,false)==Mode.Story,"extra-mode scripted owner exposes Skip");
        Check(SelectMode(32|64,0,false)==Mode.Menu,"Legend choice keeps manual confirmation");
        Check(SelectMode(512|1024,0,false)==Mode.Menu,"loading cannot receive Skip");
        Check(SelectMode(512,0,true)==Mode.Menu,"managed modal owns its input");
        Check(SelectMode(1,0,false)==Mode.Menu&&SelectMode(0,1,false)==Mode.Driving,"title and race never expose Skip");
        s.BeginFrame(Mode.Story,512);s.Contact(50,Vector2.zero,true,false,Button.Skip,steer);s.EndFrame();
        f=default;s.Apply(ref f,Mode.Story,false);
        Check(s.Skipping&&f.padConnected==1&&(f.padButtons&0x10)!=0,"Skip maps native Start");
        Check(!Key(f,13)&&!Key(f,27)&&!Key(f,8),"Skip is not Confirm, Escape or Back");
        s.BeginFrame(Mode.Story,512);s.Contact(50,Vector2.zero,false,true,Button.Skip,steer);s.EndFrame();
        f=default;s.Apply(ref f,Mode.Story,false);Check((f.padButtons&0x10)!=0,"tap stays latched across dialogue pages");
        s.BeginFrame(Mode.Story,512);s.Contact(51,Vector2.zero,true,false,Button.Skip,steer);s.EndFrame();
        Check(!s.Skipping,"second tap cancels skip");
        s.BeginFrame(Mode.Story,512);s.EndFrame();
        s.BeginFrame(Mode.Story,512);s.Contact(52,Vector2.zero,true,false,Button.Skip,steer);s.EndFrame();
        s.BeginFrame(Mode.Menu,32|64);s.Contact(52,Vector2.zero,false,false,Button.Confirm,steer);s.EndFrame();
        f=default;s.Apply(ref f,Mode.Menu,false);Check(f.padButtons==0&&!Key(f,13)&&!s.Skipping,"skip cannot select Yes on next screen");
        s.BeginFrame(Mode.Story,512);s.Contact(53,Vector2.zero,true,false,Button.Skip,steer);s.EndFrame();
        s.BeginFrame(Mode.Driving,1);s.Contact(53,Vector2.zero,false,false,Button.Pause,steer);s.EndFrame();
        f=default;s.Apply(ref f,Mode.Driving,true);Check(f.padButtons==0&&!Key(f,27)&&!s.Skipping,"skip cannot pause following race");
        s.BeginFrame(Mode.Story,512);s.Contact(54,Vector2.zero,true,false,Button.Skip,steer);s.EndFrame();s.Clear();
        s.BeginFrame(Mode.Story,512);s.Contact(54,Vector2.zero,false,false,Button.Skip,steer);s.EndFrame();
        Check(!s.Skipping,"focus loss cancels latched Skip");
        // Tilt: screen-space gravity (x right, y up) pointing to the ground.
        Vector3 Rolled(float deg,float lean){float r=Mathf.Cos(lean*Mathf.Deg2Rad);return new Vector3(Mathf.Sin(deg*Mathf.Deg2Rad)*r,-Mathf.Cos(deg*Mathf.Deg2Rad)*r,-Mathf.Sin(lean*Mathf.Deg2Rad));}
        Check(Mathf.Abs(TiltDegrees(Rolled(0,0)))<.01f&&Mathf.Abs(TiltDegrees(Rolled(0,60)))<.01f,"level phone is centred");
        Check(Mathf.Abs(TiltDegrees(Rolled(20,0))-20)<.05f,"clockwise tilt steers right");
        Check(Mathf.Abs(TiltDegrees(Rolled(20,50))-20)<.05f,"leaning back keeps wheel sensitivity");
        Check(Mathf.Abs(TiltDegrees(Rolled(-15,35))+15)<.05f,"anticlockwise tilt steers left");
        Check(Mathf.Abs(TiltDegrees(new Vector3(Mathf.Sin(10*Mathf.Deg2Rad),0,-Mathf.Cos(10*Mathf.Deg2Rad)))-10)<.05f,"flat phone uses side roll");
        Check(TiltDegrees(Vector3.zero)==0,"missing sample is neutral");
        float phi=12*Mathf.Deg2Rad;
        var landscapeLeft=ScreenSpace(new Vector3(-Mathf.Cos(phi),-Mathf.Sin(phi),0),ScreenOrientation.LandscapeLeft);
        Check(Mathf.Abs(landscapeLeft.x-Mathf.Sin(phi))<1e-4f&&Mathf.Abs(landscapeLeft.y+Mathf.Cos(phi))<1e-4f,"landscape-left device axes");
        var landscapeRight=ScreenSpace(new Vector3(Mathf.Cos(phi),Mathf.Sin(phi),0),ScreenOrientation.LandscapeRight);
        Check(Mathf.Abs(TiltDegrees(landscapeLeft)-12)<.05f&&Mathf.Abs(TiltDegrees(landscapeRight)-12)<.05f,"both landscape orientations steer right");
        Check(TiltSteering(1f,30)==0&&TiltSteering(-1.4f,30)==0,"tilt jitter dead zone");
        Check(TiltSteering(30,30)==1&&TiltSteering(-45,30)==-1&&TiltSteering(20,20)==1,"tilt full lock clamps");
        Check(Mathf.Abs(TiltSteering(15.75f,30)-.5f)<1e-4f&&Mathf.Abs(TiltSteering(-15.75f,30)+.5f)<1e-4f,"tilt is linear past the dead zone");
        s.Clear();s.BeginFrame(Mode.Driving,6);s.EndFrame();s.SetTilt(true,.25f);
        f=new Idas3Native.FrameInput{thumbLY=999};s.Apply(ref f,Mode.Driving,true);
        Check(f.padConnected==1&&f.thumbLX==8192&&f.thumbLY==0,"tilt drives the virtual stick");
        f=new Idas3Native.FrameInput{padConnected=1,thumbLX=-30000};s.Apply(ref f,Mode.Driving,true);
        Check(f.thumbLX==-30000,"larger physical stick keeps priority over tilt");
        s.SetTilt(true,-1);f=default;s.Apply(ref f,Mode.Driving,true);Check(f.thumbLX==-32768,"tilt full left");
        f=default;s.Apply(ref f,Mode.Menu,false);Check(f.thumbLX==0&&f.padConnected==0,"menus ignore tilt");
        s.BeginFrame(Mode.Driving,6);s.Contact(60,new Vector2(200,100),true,false,Button.Steer,steer);s.Contact(61,Vector2.zero,true,false,Button.Gas,steer);s.EndFrame();
        f=default;s.Apply(ref f,Mode.Driving,true);Check(f.thumbLX==32767&&f.rightTrigger==255,"touch wheel overrides tilt");
        s.Release(Button.Steer);s.BeginFrame(Mode.Driving,6);
        s.Contact(60,new Vector2(200,100),false,false,Button.Brake,steer);s.Contact(61,Vector2.zero,false,false,Button.Gas,steer);s.EndFrame();
        Check(!s.Held(Button.Steer)&&!s.Held(Button.Brake)&&s.Held(Button.Gas),"TILT toggle releases only the removed wheel");
        s.Clear();Check(!s.TiltActive&&s.Tilt==0,"focus loss clears tilt");
        var normalized=new Settings{tilt=true,tiltRange=99,buttonSize=7,opacity=2}.Normalized();
        Check(normalized.tiltRange==45&&normalized.buttonSize==80&&Mathf.Approximately(normalized.opacity,.9f),"touch settings clamp");
        normalized=new Settings{tiltRange=22,buttonSize=114,opacity=.62f}.Normalized();
        Check(normalized.tiltRange==20&&normalized.buttonSize==110&&Mathf.Approximately(normalized.opacity,.6f),"touch settings snap to menu steps");
        Check(Settings.Defaults.Same(Settings.Defaults)&&!Settings.Defaults.Same(new Settings{tilt=true,tiltRange=30,buttonSize=100,opacity=.65f}),"touch draft change detection");
        Check(Mathf.Abs(TiltSteering(31.5f,45)-.6897f)<1e-3f&&TiltSteering(15,15)==1,"menu tilt range drives full lock");
        foreach(var screen in new[]{new Vector2(2670,1200),new Vector2(1280,720),new Vector2(1920,1080)})
        foreach(Mode mode in new[]{Mode.Menu,Mode.Driving,Mode.Story})
        foreach(bool tilt in new[]{false,true})
        foreach(float buttonSize in new[]{.8f,1f,1.3f})
        {
            var safe=new Rect(70,24,screen.x-110,screen.y-48);var bounds=new Rect[(int)Button.Count];Layout(bounds,mode,tilt,buttonSize,safe,screen.y,out float size,out Rect tiltGauge);
            var guiSafe=new Rect(safe.x,screen.y-safe.yMax,safe.width,safe.height);
            Check(bounds[(int)Button.Fade].width==0,"opacity lives in Options, not a FADE button");
            Check((mode==Mode.Driving&&tilt)==(tiltGauge.width>0),"tilt gauge only in tilt driving");
            if(tiltGauge.width>0)Check(tiltGauge.xMin>=guiSafe.xMin&&tiltGauge.xMax<=guiSafe.xMax&&tiltGauge.yMin>=guiSafe.yMin&&tiltGauge.yMax<=guiSafe.yMax,"tilt gauge in safe area");
            if(mode==Mode.Driving)
            {
                Check((bounds[(int)Button.Steer].width>0)!=tilt&&bounds[(int)Button.Tilt].width>0,"tilt replaces the wheel and keeps its toggle");
                if(tilt)Check(bounds[(int)Button.Brake].center.x<guiSafe.center.x&&bounds[(int)Button.Gas].center.x>guiSafe.center.x,"tilt brake left, gas right");
            }
            for(int i=0;i<bounds.Length;++i)if(bounds[i].width>0)
            {
                Check(guiSafe.Contains(bounds[i].center)&&bounds[i].xMin>=guiSafe.xMin&&bounds[i].xMax<=guiSafe.xMax&&bounds[i].yMin>=guiSafe.yMin&&bounds[i].yMax<=guiSafe.yMax,"safe-area layout");
                Check(Hit(bounds,bounds[i].center)==(Button)i,"hit target matches drawn button");
                Check(!bounds[i].Overlaps(tiltGauge),"tilt gauge does not cover a button");
                for(int j=i+1;j<bounds.Length;++j)if(bounds[j].width>0)Check(!bounds[i].Overlaps(bounds[j]),"controls do not overlap");
            }
        }
        checks+=Idas3GameOptions.RunMobileDisplaySelfTests();
        checks+=Idas3PauseMenu.RunMobileLayoutSelfTests();
        checks+=Idas3.Multiplayer.Idas3TcpTransport.RunLanSelfTests();
        return checks;
    }
}
