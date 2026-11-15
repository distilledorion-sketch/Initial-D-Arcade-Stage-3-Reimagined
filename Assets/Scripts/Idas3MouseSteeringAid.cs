using UnityEngine;

// On-screen wheel for the mouse steering profile. It rotates with the
// virtual wheel position through an adjustable lock-to-lock range, marks the
// centre, and turns its marker red at full lock. HUD layout group 11 moves and sizes it; the art is generated here.
public sealed class Idas3MouseSteeringAid : MonoBehaviour
{
    public const int HudGroup = 11;
    static Texture2D wheel;
    Idas3SceneGame host;

    public void Initialize(Idas3SceneGame owner) { host = owner; }
    internal bool Visible => host != null && host.Ready && host.MouseSteeringActive && host.GameOptions != null && host.GameOptions.Current.mouseSteeringAid;

    // Bottom centre of the 640x480 safe area, scaled by the group's size and
    // moved by its normalized offset like the other HUD groups.
    public static Rect ScreenRect(Idas3GameOptions.Values values, float width, float height)
    {
        float fit = Mathf.Min(width / 640, height / 480), scale = values?.HudGroupScale(HudGroup) ?? 1;
        float size = 72 * fit * scale;
        var center = new Vector2(width * .5f, height - 24 * fit - size * .5f);
        center += Vector2.Scale(values?.HudOffset(HudGroup) ?? Vector2.zero, new Vector2(width, height));
        return new Rect(center.x - size * .5f, center.y - size * .5f, size, size);
    }

    // The range is the wheel's rotation from lock to lock, so full lock
    // turns it by half the range each way.
    public static float RotationDegrees(float position, int rangeDegrees) =>
        (float.IsNaN(position) ? 0 : Mathf.Clamp(position, -1, 1)) * rangeDegrees * .5f;

    public static void Draw(Rect rect, float position, int rangeDegrees)
    {
        if (wheel == null) wheel = CreateWheel(128);
        position = float.IsNaN(position) ? 0 : Mathf.Clamp(position, -1, 1);
        var matrix = GUI.matrix; var color = GUI.color; var center = rect.center;
        // A fixed tick above the wheel shows where centre is.
        GUI.color = new Color(1, 1, 1, .55f);
        GUI.DrawTexture(new Rect(center.x - rect.width * .02f, rect.y - rect.height * .14f, rect.width * .04f, rect.height * .1f), Texture2D.whiteTexture);
        GUIUtility.RotateAroundPivot(RotationDegrees(position, rangeDegrees), center);
        GUI.color = new Color(1, 1, 1, .85f);
        GUI.DrawTexture(rect, wheel);
        GUI.color = Mathf.Abs(position) >= .99f ? new Color(1, .2f, .15f) : new Color(1, .8f, .2f);
        GUI.DrawTexture(new Rect(center.x - rect.width * .05f, rect.y, rect.width * .1f, rect.height * .13f), Texture2D.whiteTexture);
        GUI.matrix = matrix; GUI.color = color;
    }

    void OnGUI()
    {
        if (Event.current.type != EventType.Repaint || !Visible) return;
        GUI.depth = 100; // Behind every menu and overlay.
        var values = host.GameOptions.Current;
        Draw(ScreenRect(values, Screen.width, Screen.height), host.MouseSteeringPosition, values.mouseSteeringAidRange);
    }

    // White ring, hub and three spokes with a one-pixel anti-aliased edge.
    static Texture2D CreateWheel(int size)
    {
        var texture = new Texture2D(size, size, TextureFormat.RGBA32, false) { name = "Mouse steering aid", wrapMode = TextureWrapMode.Clamp, hideFlags = HideFlags.DontSave };
        var pixels = new Color32[size * size];
        float half = size * .5f, outer = half - 2, inner = outer - size * .09f, hub = size * .1f, spoke = size * .035f;
        static float Edge(float distance) => Mathf.Clamp01(.5f - distance);
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
            {
                float px = x + .5f - half, py = y + .5f - half, r = Mathf.Sqrt(px * px + py * py);
                float ring = Mathf.Min(Edge(r - outer), Edge(inner - r));
                float centre = Edge(r - hub);
                // Spokes run left, right and down (texture y is up) from hub to ring.
                float sideways = Edge(Mathf.Abs(py) - spoke);
                float down = py < 0 ? Edge(Mathf.Abs(px) - spoke) : 0;
                float spokes = Mathf.Max(sideways, down) * Edge(r - inner - 1);
                float alpha = Mathf.Max(ring, Mathf.Max(centre, spokes));
                pixels[y * size + x] = new Color32(255, 255, 255, (byte)Mathf.RoundToInt(alpha * 255));
            }
        texture.SetPixels32(pixels); texture.Apply(false, true);
        return texture;
    }
}
