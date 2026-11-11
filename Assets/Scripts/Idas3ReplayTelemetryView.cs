using System;
using UnityEngine;

// IMGUI driving reference for the replay viewer: the telemetry panel and
// event ticks drawn over the playback slider.
internal sealed class Idas3ReplayTelemetryView : IDisposable
{
    static readonly Color SteeringColor = new Color(1f, .82f, .25f), PenaltyColor = new Color(1f, .38f, .32f), DriftColor = new Color(.55f, 1f, .45f),
        YawColor = new Color(.85f, .55f, 1f), PendingColor = new Color(.45f, .7f, 1f), ApplyingColor = new Color(1f, .6f, .2f);
    static readonly Color ShiftColor = new Color(.45f, .85f, 1f), SameFrameColor = new Color(1f, .72f, .1f), WallColor = new Color(1f, .3f, .25f), SectionColor = new Color(1f, 1f, 1f, .7f);
    const double PressFlashSeconds = .2;
    Idas3ReplayAnalysis analysis; double duration = 1;
    Texture2D white, wheel, triangle, triangleOutline;
    GUIStyle small, tiny, big, header;
    public bool PanelVisible = true;
    public Idas3ReplayAnalysis Analysis => analysis;

    public void Rebuild(Idas3ReplayAnalysis value, double replayDuration)
    {
        analysis = value; duration = Math.Max(1e-3, replayDuration);
    }
    public void Dispose()
    {
        foreach (var texture in new[] { white, wheel, triangle, triangleOutline }) if (texture != null) UnityEngine.Object.Destroy(texture);
        white = wheel = triangle = triangleOutline = null;
    }

    void EnsureResources()
    {
        if (white == null) { white = new Texture2D(1, 1, TextureFormat.RGBA32, false) { hideFlags = HideFlags.DontSave }; white.SetPixel(0, 0, Color.white); white.Apply(); }
        if (wheel == null) wheel = BuildWheel(160);
        if (triangle == null) { triangle = BuildTriangle(48, false); triangleOutline = BuildTriangle(48, true); }
        if (small == null)
        {
            small = new GUIStyle(GUI.skin.label) { fontSize = 12, wordWrap = false, padding = new RectOffset(0, 0, 0, 0), margin = new RectOffset(0, 0, 0, 0), alignment = TextAnchor.MiddleLeft };
            tiny = new GUIStyle(small) { fontSize = 10 };
            header = new GUIStyle(small) { fontSize = 13, fontStyle = FontStyle.Bold };
            big = new GUIStyle(small) { fontSize = 30, fontStyle = FontStyle.Bold, alignment = TextAnchor.MiddleCenter };
        }
    }

    // Three-spoke wheel, rim marker at twelve o'clock. Alpha is anti-aliased.
    static Texture2D BuildWheel(int size)
    {
        var texture = new Texture2D(size, size, TextureFormat.RGBA32, false) { hideFlags = HideFlags.DontSave, wrapMode = TextureWrapMode.Clamp, filterMode = FilterMode.Bilinear };
        var pixels = new Color32[size * size]; float c = (size - 1) / 2f, outer = size * .47f, inner = size * .39f, hub = size * .12f;
        Vector2[] spokes = { Vector2.left, Vector2.right, Vector2.down };
        for (int y = 0; y < size; y++)
            for (int x = 0; x < size; x++)
            {
                var p = new Vector2(x - c, y - c); float r = p.magnitude, alpha = 0; Color color = new Color(.86f, .86f, .88f);
                float rim = Mathf.Min(r - inner, outer - r);
                if (rim > -1) alpha = Mathf.Clamp01(rim + 1);
                if (r < hub + 1) { alpha = Mathf.Max(alpha, Mathf.Clamp01(hub + 1 - r)); color = new Color(.55f, .55f, .6f); }
                foreach (var s in spokes)
                {
                    float along = Vector2.Dot(p, s), across = Mathf.Abs(p.x * s.y - p.y * s.x);
                    if (along > 0 && along < inner + 1) { float a = Mathf.Clamp01(size * .035f - across + 1); if (a > alpha) { alpha = a; color = new Color(.7f, .7f, .74f); } }
                }
                // Texture rows grow upward: +y is twelve o'clock on screen.
                if (rim > -1 && p.y > 0 && Mathf.Abs(Mathf.Atan2(p.x, p.y)) < .14f) color = new Color(1f, .78f, .15f);
                pixels[y * size + x] = new Color(color.r, color.g, color.b, alpha);
            }
        texture.SetPixels32(pixels); texture.Apply(); return texture;
    }

    // Upward white triangle, filled or as a 2.5 px outline; tinted when drawn.
    static Texture2D BuildTriangle(int size, bool outline)
    {
        var texture = new Texture2D(size, size, TextureFormat.RGBA32, false) { hideFlags = HideFlags.DontSave, wrapMode = TextureWrapMode.Clamp, filterMode = FilterMode.Bilinear };
        var pixels = new Color32[size * size];
        Vector2 top = new Vector2(size / 2f, size - 3), left = new Vector2(3, 4), right = new Vector2(size - 3, 4);
        float Edge(Vector2 a, Vector2 b, Vector2 p) { var d = (b - a).normalized; return (p.x - a.x) * d.y - (p.y - a.y) * d.x; }
        for (int y = 0; y < size; y++)
            for (int x = 0; x < size; x++)
            {
                var p = new Vector2(x + .5f, y + .5f);
                // Signed distance inside all three edges (positive = inside).
                float inside = Mathf.Min(Edge(left, top, p), Mathf.Min(Edge(top, right, p), Edge(right, left, p)));
                float alpha = Mathf.Clamp01(inside + .5f);
                if (outline) alpha = Mathf.Min(alpha, Mathf.Clamp01(3f - inside));
                pixels[y * size + x] = new Color(1, 1, 1, alpha);
            }
        texture.SetPixels32(pixels); texture.Apply(); return texture;
    }

    void Fill(Rect rect, Color color) { var previous = GUI.color; GUI.color = color; GUI.DrawTexture(rect, white); GUI.color = previous; }
    void Text(Rect rect, string text, GUIStyle style, Color color)
    {
        var previous = GUI.color; GUI.color = color; Idas3MenuLocalization.Label(rect, text, style, false); GUI.color = previous;
    }
    void Line(Vector2 from, Vector2 to, float width, Color color)
    {
        var delta = to - from; float length = delta.magnitude; if (length < .01f) return;
        var matrix = GUI.matrix;
        GUI.matrix = matrix * Matrix4x4.TRS(from, Quaternion.Euler(0, 0, Mathf.Atan2(delta.y, delta.x) * Mathf.Rad2Deg), Vector3.one);
        Fill(new Rect(0, -width / 2, length, width), color); GUI.matrix = matrix;
    }
    void Rotated(Rect rect, float clockwiseDegrees, Texture texture, Color color)
    {
        var matrix = GUI.matrix; var pivot = rect.center; var previous = GUI.color; GUI.color = color;
        GUI.matrix = matrix * Matrix4x4.TRS(pivot, Quaternion.Euler(0, 0, clockwiseDegrees), Vector3.one) * Matrix4x4.TRS(-pivot, Quaternion.identity, Vector3.one);
        GUI.DrawTexture(rect, texture); GUI.matrix = matrix; GUI.color = previous;
    }
    // Horizontal bar: centered bars fill from the middle, others from the left.
    void Bar(Rect rect, float value, bool centered, Color color)
    {
        Fill(rect, new Color(0, 0, 0, .45f));
        value = Mathf.Clamp(value, centered ? -1 : 0, 1);
        if (centered) { float half = rect.width / 2; Fill(value >= 0 ? new Rect(rect.x + half, rect.y, half * value, rect.height) : new Rect(rect.x + half + half * value, rect.y, -half * value, rect.height), color); Fill(new Rect(rect.x + half - .5f, rect.y - 2, 1, rect.height + 4), new Color(1, 1, 1, .6f)); }
        else Fill(new Rect(rect.x, rect.y, rect.width * value, rect.height), color);
    }
    void VerticalBar(Rect rect, float value, Color color)
    {
        Fill(rect, new Color(0, 0, 0, .45f)); value = Mathf.Clamp01(value);
        Fill(new Rect(rect.x, rect.yMax - rect.height * value, rect.width, rect.height * value), color);
    }
    static string Side(float value, string unit, string format = "0") =>
        Mathf.Abs(value) < .05f ? "0" + unit : (value > 0 ? "R " : "L ") + Mathf.Abs(value).ToString(format) + unit;
    static string Signed(float value, string format) => (value >= 0 ? "+" : "−") + Mathf.Abs(value).ToString(format);
    public static string Clock6000(uint ticks) { double s = ticks / 6000.0; return string.Format("{0}'{1:00.000}", (int)s / 60, s % 60); }
    static string NotRecorded => Idas3MenuLocalization.T("not recorded");

    // One shift-button triangle: outline when idle, filled while held,
    // cyan just after a press and amber for a same-frame down+up.
    void ShiftButton(Rect rect, bool down, int frame, double seconds)
    {
        var a = analysis; bool held = false; Color? flash = null;
        int shift = a.LastEvent(Idas3ReplayAnalysis.EventKind.Shift, seconds);
        if (shift >= 0 && seconds - a.Events[shift].seconds < PressFlashSeconds)
        {
            var e = a.Events[shift];
            bool mine = a.HasShiftButtons ? (down ? e.down : e.up) : (down ? e.to < e.from : e.to > e.from);
            if (mine) flash = Idas3ReplayAnalysis.SameFrameShift(e) ? SameFrameColor : a.HasShiftButtons ? ShiftColor : new Color(ShiftColor.r, ShiftColor.g, ShiftColor.b, .55f);
        }
        if (a.HasShiftButtons) held = down ? a.DownHeld[frame] : a.UpHeld[frame];
        float angle = down ? 180 : 0;
        if (flash.HasValue) Rotated(rect, angle, triangle, flash.Value);
        else if (held) Rotated(rect, angle, triangle, Color.white);
        else Rotated(rect, angle, triangleOutline, new Color(1, 1, 1, a.HasShiftButtons ? .45f : .25f));
    }

    public const float PanelWidth = 264, PanelHeight = 566;
    public void DrawPanel(Rect area, double seconds)
    {
        if (analysis == null || analysis.Count == 0) return;
        EnsureResources();
        var a = analysis; int i = a.FrameAt(seconds); float x = area.x + 10, w = area.width - 20, y = area.y + 6;
        Fill(area, new Color(0, 0, 0, .62f));
        var dim = new Color(1, 1, 1, .55f);
        Text(new Rect(x, y, w, 18), Idas3MenuLocalization.T("TELEMETRY") + "  (T)", header, Color.white);
        if (!a.HasTelemetry) Text(new Rect(x + 110, y, w - 110, 18), Idas3MenuLocalization.T(a.Detailed ? "older recording" : "legacy recording"), tiny, dim);
        y += 22;

        // Steering wheel, 540 degrees lock to lock.
        var wheelRect = new Rect(x, y, 124, 124);
        Rotated(wheelRect, a.Detailed ? a.WheelDegrees[i] : 0, wheel, a.Detailed ? Color.white : new Color(1, 1, 1, .25f));
        float pedalX = x + 132;
        VerticalBar(new Rect(pedalX, y + 4, 16, 104), a.Throttle[i], new Color(.35f, 1f, .45f));
        VerticalBar(new Rect(pedalX + 22, y + 4, 16, 104), a.Brake[i], new Color(1f, .3f, .25f));
        Text(new Rect(pedalX - 2, y + 110, 20, 14), Idas3MenuLocalization.T("THR"), tiny, dim); Text(new Rect(pedalX + 20, y + 110, 24, 14), Idas3MenuLocalization.T("BRK"), tiny, dim);
        // Gear between the shift-up (above) and shift-down (below) buttons.
        float infoX = pedalX + 46, infoW = area.xMax - 8 - infoX, buttonX = infoX + infoW / 2 - 11;
        ShiftButton(new Rect(buttonX, y, 22, 18), false, i, seconds);
        Text(new Rect(infoX, y + 18, infoW, 36), a.Gear[i] == 0 ? "N" : a.Gear[i].ToString(), big, Color.white);
        ShiftButton(new Rect(buttonX, y + 56, 22, 18), true, i, seconds);
        Text(new Rect(infoX, y + 80, infoW, 16), a.Detailed ? a.Rpm[i].ToString("#,0") + " rpm" : "— rpm", small, Color.white);
        Text(new Rect(infoX, y + 98, infoW, 16), a.SpeedKmh[i].ToString("0") + " km/h", small, Color.white);
        y += 128;
        if (a.Detailed)
        {
            Text(new Rect(x, y, w, 16), Idas3MenuLocalization.Format("Steering {0}  ·  input {1}%", Side(a.WheelDegrees[i], "°"), Signed(-a.SteeringInput[i] * 100, "0")), small, Color.white);
            Bar(new Rect(x, y + 18, w, 6), a.WheelDegrees[i] / Idas3ReplayAnalysis.LockDegrees, true, SteeringColor);
            Text(new Rect(x, y + 25, 60, 12), "L 270°", tiny, dim); Text(new Rect(x + w - 44, y + 25, 44, 12), "R 270°", tiny, dim);
        }
        else Text(new Rect(x, y, w, 16), Idas3MenuLocalization.T("Steering") + ": " + NotRecorded, small, dim);
        y += 40;
        int shift = a.LastEvent(Idas3ReplayAnalysis.EventKind.Shift, seconds);
        if (shift >= 0)
        {
            var e = a.Events[shift]; string rpm = float.IsNaN(e.rpm) ? "" : " @ " + e.rpm.ToString("#,0") + " rpm";
            Text(new Rect(x, y, w, 16), Idas3MenuLocalization.Format("Last shift {0}{1}  ({2} s ago)", Idas3ReplayAnalysis.ShiftLabel(e), rpm, (seconds - e.seconds).ToString("0.0")),
                small, Idas3ReplayAnalysis.SameFrameShift(e) ? SameFrameColor : ShiftColor);
        }
        else Text(new Rect(x, y, w, 16), Idas3MenuLocalization.T("No shifts yet"), small, dim);
        y += 22;

        // Car-relative compass (nose up): the travel arrow shows the drift.
        var compass = new Rect(x, y, 72, 72); Fill(compass, new Color(0, 0, 0, .4f));
        var center = compass.center;
        Line(center + new Vector2(0, 26), center - new Vector2(0, 26), 4, new Color(1, 1, 1, .85f));
        Fill(new Rect(center.x - 4, center.y - 30, 8, 6), new Color(1, 1, 1, .85f));
        float travel = -a.DriftDegrees[i] * Mathf.Deg2Rad;
        Line(center, center + new Vector2(Mathf.Sin(travel), -Mathf.Cos(travel)) * 32, 2, DriftColor);
        float textX = x + 82, textW = area.xMax - 8 - textX;
        Text(new Rect(textX, y + 4, textW, 16), Idas3MenuLocalization.T("Drift") + " " + Side(a.DriftDegrees[i], "°"), small, DriftColor);
        Text(new Rect(textX, y + 24, textW, 16), Idas3MenuLocalization.T("Yaw") + " " + Side(a.YawRateDegrees[i], "°/s"), small, YawColor);
        Text(new Rect(textX, y + 44, textW, 16), Idas3MenuLocalization.T("Heading") + " " + a.HeadingDegrees[i].ToString("0") + "°", small, Color.white);
        y += 76;
        Bar(new Rect(x, y + 2, w, 5), a.YawRateDegrees[i] / Mathf.Max(10, a.MaxAbsYawRate), true, YawColor);
        y += 12;

        // Steering memory: pending (+288) feeds applying (+284) a little per frame.
        if (a.HasRotation)
        {
            Text(new Rect(x, y, 120, 14), Idas3MenuLocalization.T("Pending rotation"), small, PendingColor);
            Text(new Rect(x + w - 60, y, 60, 14), Side(a.PendingRotation[i], "", "0.0"), tiny, Color.white);
            Bar(new Rect(x, y + 16, w, 7), a.PendingRotation[i] / Mathf.Max(1, a.MaxAbsPending), true, PendingColor);
            Text(new Rect(x, y + 27, 120, 14), Idas3MenuLocalization.T("Applying rotation"), small, ApplyingColor);
            Text(new Rect(x + w - 60, y + 27, 60, 14), Side(a.ApplyingRotation[i], "", "0.0"), tiny, Color.white);
            Bar(new Rect(x, y + 43, w, 7), a.ApplyingRotation[i] / Idas3ReplayAnalysis.ApplyingLimit, true, ApplyingColor);
        }
        else
        {
            Text(new Rect(x, y, w, 14), Idas3MenuLocalization.T("Pending rotation") + ": " + NotRecorded, small, dim);
            Text(new Rect(x, y + 27, w, 14), Idas3MenuLocalization.T("Applying rotation") + ": " + NotRecorded, small, dim);
        }
        y += 56;

        Text(new Rect(x, y, w, 16), Idas3MenuLocalization.T("Wheel slip"), small, dim); y += 17;
        for (int wIndex = 0; wIndex < 4; wIndex++)
        {
            var cell = new Rect(x + (wIndex % 2) * (w / 2 + 2), y + (wIndex / 2) * 16, w / 2 - 4, 14);
            Text(new Rect(cell.x, cell.y, 22, 14), Idas3ReplayAnalysis.WheelNames[wIndex], tiny, dim);
            if (a.WheelSlipAvailable)
            {
                float slip = a.WheelSlip[wIndex][i];
                Bar(new Rect(cell.x + 22, cell.y + 3, cell.width - 66, 8), slip * 2, true, slip > 0 ? new Color(1f, .6f, .2f) : new Color(.4f, .7f, 1f));
                Text(new Rect(cell.xMax - 42, cell.y, 42, 14), Signed(slip * 100, "0") + "%", tiny, Color.white);
            }
            else Text(new Rect(cell.x + 22, cell.y, cell.width - 22, 14), NotRecorded, tiny, dim);
        }
        y += 36;

        if (a.HasTelemetry)
        {
            Text(new Rect(x, y, 86, 14), Idas3MenuLocalization.T("Penalty"), small, PenaltyColor);
            Bar(new Rect(x + 88, y + 4, w - 140, 7), a.Penalty[i] / Mathf.Max(.01f, a.MaxPenalty), false, PenaltyColor);
            Text(new Rect(x + w - 48, y, 48, 14), a.Penalty[i].ToString("0.000"), tiny, Color.white);
            Text(new Rect(x, y + 17, 86, 14), Idas3MenuLocalization.T("Speed loss"), small, new Color(1f, .6f, .5f));
            Bar(new Rect(x + 88, y + 21, w - 140, 7), a.SpeedLoss[i] / Mathf.Max(.01f, a.MaxSpeedLoss), false, new Color(1f, .6f, .5f));
            Text(new Rect(x + w - 48, y + 17, 48, 14), a.SpeedLoss[i].ToString("0.000"), tiny, Color.white);
            Text(new Rect(x, y + 34, w - 60, 14), Idas3MenuLocalization.T("Heading error") + " " + Signed(a.HeadingError[i], "0.000"), small, Color.white);
            int wall = a.LastEvent(Idas3ReplayAnalysis.EventKind.Wall, seconds);
            bool hot = a.Wall[i] || (wall >= 0 && seconds - a.Events[wall].seconds < .4);
            var badge = new Rect(x + w - 56, y + 34, 56, 15);
            Fill(badge, hot ? WallColor : new Color(1, 1, 1, .08f));
            Text(badge, Idas3MenuLocalization.T("WALL"), tiny, hot ? Color.white : dim);
        }
        else
        {
            Text(new Rect(x, y, w, 14), Idas3MenuLocalization.T("Penalty") + ": " + NotRecorded, small, dim);
            Text(new Rect(x, y + 17, w, 14), Idas3MenuLocalization.T("Speed loss") + ": " + NotRecorded, small, dim);
            Text(new Rect(x, y + 34, w, 14), Idas3MenuLocalization.T("Wall contact") + ": " + NotRecorded, small, dim);
        }
        y += 54;

        Text(new Rect(x, y, w, 16), Idas3MenuLocalization.T("Sections"), small, dim); y += 17;
        if (a.Sections.Length == 0) Text(new Rect(x, y, w, 14), NotRecorded, tiny, dim);
        for (int s = 0; s < a.Sections.Length && s < 5; s++)
        {
            var section = a.Sections[s]; bool done = section.split6000 / 6000.0 <= seconds + 1e-9;
            string row = string.Format("S{0}  {1}  ({2})", section.index, Clock6000(section.split6000), Clock6000(section.duration6000));
            Text(new Rect(x, y, w - 64, 14), row, tiny, done ? Color.white : dim);
            if (section.hasDelta) Text(new Rect(x + w - 62, y, 62, 14), "Δ " + Signed(section.delta6000 / 6000f, "0.000"), tiny, !done ? dim : section.delta6000 <= 0 ? new Color(.5f, 1f, .5f) : new Color(1f, .5f, .45f));
            y += 15;
        }
    }

    // Event ticks over the playback slider: shifts above the track (amber for
    // same-frame down+up), wall hits below it, sections across it.
    public void DrawMarkers(Rect slider)
    {
        if (analysis == null || Event.current.type != EventType.Repaint) return;
        EnsureResources();
        // Unity's slider thumb never reaches the outer edges; inset by half a thumb.
        float inset = Mathf.Max(0, GUI.skin.horizontalSliderThumb.fixedWidth / 2), left = slider.x + inset, width = Mathf.Max(1, slider.width - inset * 2);
        float mid = slider.center.y;
        foreach (var e in analysis.Events)
        {
            float px = left + (float)(Math.Max(0, Math.Min(1, e.seconds / duration)) * width);
            if (e.kind == Idas3ReplayAnalysis.EventKind.Section) Fill(new Rect(px - .5f, mid - 9, 1, 18), SectionColor);
            else if (e.kind == Idas3ReplayAnalysis.EventKind.Shift) Fill(new Rect(px - .5f, mid - 10, 1.5f, 6), Idas3ReplayAnalysis.SameFrameShift(e) ? SameFrameColor : ShiftColor);
            else Fill(new Rect(px - 1.5f, mid + 4, 3, 6), WallColor);
        }
    }
}
