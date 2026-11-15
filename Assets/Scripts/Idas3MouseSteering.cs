using System;
using UnityEngine;

// Mouse steering response profile: horizontal mouse counts move a virtual
// wheel that stays where it is left. Only delta X is used; the cursor's
// screen position never matters. Native receives the position as a linear
// steering axis (see Idas3GameOptions.Values.NativeControllerResponse).
public sealed class Idas3MouseSteering
{
    // Sensitivity 0..1 maps to 4000 (slowest) .. 200 (fastest) counts per
    // full lock, evenly on a log scale; the default .5 is about 900 counts.
    public static float CountsToFullLock(float sensitivity) =>
        4000f * Mathf.Pow(.05f, Mathf.Clamp01(float.IsNaN(sensitivity) ? .5f : sensitivity));
    public float Position { get; private set; }
    public void Reset() => Position = 0;
    public float Advance(float deltaX, float sensitivity)
    {
        if (!float.IsNaN(deltaX) && !float.IsInfinity(deltaX))
            Position = Mathf.Clamp(Position + deltaX / CountsToFullLock(sensitivity), -1, 1);
        return Position;
    }
    // The full-scale axis value sent as virtual thumbLX.
    public static int Axis(float position) =>
        (int)Math.Round(Mathf.Clamp(float.IsNaN(position) ? 0 : position, -1, 1) * 32767);
}
