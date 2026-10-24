using UnityEngine;

// Pointer controls own their clicks. A connected controller may resume after
// any input held during the click is released, without replaying that input.
internal sealed class Idas3MenuPointer
{
    readonly Idas3PointerNavigation navigation = new Idas3PointerNavigation();
    internal static bool Active => Input.GetMouseButton(0)||Input.GetMouseButtonUp(0)||
        Input.GetMouseButton(1)||Input.GetMouseButtonUp(1)||Input.mouseScrollDelta.sqrMagnitude>0;
    internal bool BlockNavigation(bool pointerActive,bool navigationHeld)
    {
        return navigation.Block(pointerActive,navigationHeld);
    }
    internal static void ApplyConfirm(ref Idas3Native.FrameInput frame,bool keypadEnter,bool mouseHeld,bool nativeOwnsPointer)
    {
        if(keypadEnter||mouseHeld&&nativeOwnsPointer)frame.SetKey(13);
    }
}
