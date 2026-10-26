// Pointer interactions consume navigation until both inputs have been released.
// Pure state shared by the screen router and the standalone replay viewer.
internal sealed class Idas3PointerNavigation
{
    private bool waitForRelease;
    internal bool Block(bool pointerActive, bool navigationHeld)
    {
        if (pointerActive) waitForRelease = true;
        if (!waitForRelease) return false;
        if (!pointerActive && !navigationHeld) waitForRelease = false;
        return true; // Consume the neutral release frame as well.
    }
}
