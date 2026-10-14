// A device-local control snapshot. Paths never include a transient Unity
// device ID, so saved bindings can resolve again after reconnecting.
public sealed class Idas3ControllerControl
{
    public string path, label;
    public float value, minimum = -1, maximum = 1;
    public bool button;
    // Bits emitted by this same physical control through the raw pad fallback.
    // Kept on the sampled control so bindings can remove only its own alias.
    public ushort fallbackButtons;
}
