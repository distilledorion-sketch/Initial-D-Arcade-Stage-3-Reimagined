using System;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.XInput;

// Hardware discovery is independent of the binding mapper. Windows can assign
// a reconnected controller any of four XInput slots; HID pads need Unity's
// device layouts instead of an Xbox-only API.
internal sealed class Idas3GamepadInput
{
    internal delegate uint ReadXInput(uint slot, out Idas3Native.PadState state);
    private int activeSlot = -1;
    private double nextScan;
    private Gamepad activeHid;
    internal int ActiveXInputSlot => activeSlot;

    internal bool TryRead(out Idas3ControlBindings.PadState state)
    {
        bool xinput = TryReadXInput(Time.realtimeSinceStartupAsDouble, Idas3Native.ReadGamepad, out var xbox);
        var current = Gamepad.current;
        if (Usable(current) && !(current is XInputController) && HasInput(ReadUnityPad(current)))
            activeHid = current;
        if (!Usable(activeHid)) activeHid = null;
        if (xinput && HasInput(xbox)) activeHid = null;
        if (activeHid != null) { state = ReadUnityPad(activeHid); return true; }
        if (xinput) { state = xbox; return true; }
        // A recognized device may be present before its first button event.
        if (!Usable(current))
            foreach (var device in Gamepad.all) if (Usable(device)) { current = device; break; }
        if (Usable(current)) { state = ReadUnityPad(current); return true; }
        state = default; return false;
    }

    internal bool TryReadXInput(double now, ReadXInput poll, out Idas3ControlBindings.PadState state)
    {
        state = default;
        if (activeSlot >= 0)
        {
            if (poll((uint)activeSlot, out var selected) == 0) state = FromXInput(selected);
            else { activeSlot = -1; nextScan = 0; }
        }
        // Empty XInput ports are relatively expensive to poll. Refresh the
        // inventory twice a second, but sample the selected pad every frame.
        if (now >= nextScan)
        {
            nextScan = now + .5;
            for (uint slot = 0; slot < 4; ++slot)
            {
                if (slot == activeSlot || poll(slot, out var candidate) != 0) continue;
                var value = FromXInput(candidate);
                if (!state.connected || (!HasInput(state) && HasInput(value)))
                { activeSlot = (int)slot; state = value; }
            }
        }
        return state.connected;
    }

    private static Idas3ControlBindings.PadState FromXInput(Idas3Native.PadState value) =>
        new Idas3ControlBindings.PadState {
            connected = true, buttons = value.gamepad.buttons,
            leftTrigger = value.gamepad.leftTrigger, rightTrigger = value.gamepad.rightTrigger,
            thumbLX = value.gamepad.thumbLX, thumbLY = value.gamepad.thumbLY,
            thumbRX = value.gamepad.thumbRX, thumbRY = value.gamepad.thumbRY
        };
    private static bool Usable(Gamepad pad) => pad != null && pad.added && pad.enabled;
    private static bool HasInput(Idas3ControlBindings.PadState pad) => pad.connected &&
        (pad.buttons != 0 || pad.leftTrigger > 30 || pad.rightTrigger > 30 ||
         Math.Abs((int)pad.thumbLX) > 8000 || Math.Abs((int)pad.thumbLY) > 8000 ||
         Math.Abs((int)pad.thumbRX) > 8000 || Math.Abs((int)pad.thumbRY) > 8000);
    private static short Stick(float value) => (short)Mathf.Clamp(Mathf.RoundToInt(value * (value < 0 ? 32768 : 32767)), -32768, 32767);
    private static byte Trigger(float value) => (byte)Mathf.Clamp(Mathf.RoundToInt(value * 255), 0, 255);
    internal static Idas3ControlBindings.PadState ReadUnityPad(Gamepad pad)
    {
        // Read unprocessed stick values: original native steering applies its
        // own dead zone/response, so Unity must not condition it a second time.
        var left = pad.leftStick.ReadUnprocessedValue(); var right = pad.rightStick.ReadUnprocessedValue();
        ushort buttons = 0;
        if (pad.dpad.up.isPressed) buttons |= 1;
        if (pad.dpad.down.isPressed) buttons |= 2;
        if (pad.dpad.left.isPressed) buttons |= 4;
        if (pad.dpad.right.isPressed) buttons |= 8;
        if (pad.startButton.isPressed) buttons |= 0x10;
        if (pad.selectButton.isPressed) buttons |= 0x20;
        if (pad.leftStickButton.isPressed) buttons |= 0x40;
        if (pad.rightStickButton.isPressed) buttons |= 0x80;
        if (pad.leftShoulder.isPressed) buttons |= 0x100;
        if (pad.rightShoulder.isPressed) buttons |= 0x200;
        if (pad.buttonSouth.isPressed) buttons |= 0x1000;
        if (pad.buttonEast.isPressed) buttons |= 0x2000;
        if (pad.buttonWest.isPressed) buttons |= 0x4000;
        if (pad.buttonNorth.isPressed) buttons |= 0x8000;
        return new Idas3ControlBindings.PadState {
            connected = true, buttons = buttons, leftTrigger = Trigger(pad.leftTrigger.ReadUnprocessedValue()),
            rightTrigger = Trigger(pad.rightTrigger.ReadUnprocessedValue()),
            thumbLX = Stick(left.x), thumbLY = Stick(left.y), thumbRX = Stick(right.x), thumbRY = Stick(right.y)
        };
    }
}
