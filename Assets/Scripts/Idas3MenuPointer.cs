using UnityEngine;

// Pointer controls own their clicks. A connected controller may resume after
// any input held during the click is released, without replaying that input.
internal sealed class Idas3MenuPointer
{
    bool waitForRelease;
    internal static bool Active => Input.GetMouseButton(0)||Input.GetMouseButtonUp(0)||
        Input.GetMouseButton(1)||Input.GetMouseButtonUp(1)||Input.mouseScrollDelta.sqrMagnitude>0;
    internal bool BlockNavigation(bool pointerActive,bool navigationHeld)
    {
        if(pointerActive)waitForRelease=true;
        if(!waitForRelease)return false;
        if(!pointerActive&&!navigationHeld)waitForRelease=false;
        return true;
    }
    internal static void ApplyConfirm(ref Idas3Native.FrameInput frame,bool keypadEnter,bool mouseHeld,bool nativeOwnsPointer)
    {
        if(keypadEnter||mouseHeld&&nativeOwnsPointer)frame.SetKey(13);
    }
}
