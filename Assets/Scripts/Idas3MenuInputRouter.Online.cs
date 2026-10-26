internal sealed partial class Idas3MenuInputRouter
{
    internal enum OnlineAction { None, Toggle, Back }
    internal struct OnlineSample
    {
        internal bool Toggle, Cancel, CloseHeld, NavigationHeld, PointerHeld, AnyKey;
    }
    internal struct OnlineContext
    {
        internal bool Open, Blocked, Terminal, TextEditing;
        internal int Frame;
    }

    private OnlineSample previousOnline;
    private bool onlineReleaseBlocked, onlineClosingControls, onlineTextReleaseBlocked;
    private int onlineTextBlockThroughFrame = -1;
    internal bool OnlineToggleHeld => previousOnline.Toggle;
    internal bool OnlineClosingControls => onlineClosingControls;
    internal void OnlineClosed() { onlineClosingControls = true; }
    internal void OnlineTextEnded(int frame)
    {
        onlineTextReleaseBlocked = true;
        onlineTextBlockThroughFrame = frame + 1;
    }
    internal bool OnlineCapturesText(bool editing, int frame) =>
        editing || onlineTextReleaseBlocked || frame <= onlineTextBlockThroughFrame;

    internal OnlineAction ProcessOnline(OnlineSample sample, OnlineContext context)
    {
        bool togglePressed = sample.Toggle && !previousOnline.Toggle;
        bool cancelPressed = sample.Cancel && !previousOnline.Cancel;
        previousOnline = sample;
        if (context.Terminal || context.Blocked)
        {
            // Neither a neutral unfocused sample nor a covered menu can
            // release another screen's input. Re-enter on a fresh press.
            onlineReleaseBlocked = true;
            return OnlineAction.None;
        }
        bool closingHeld = sample.Toggle || sample.Cancel || sample.CloseHeld || sample.PointerHeld;
        if (!closingHeld) onlineClosingControls = false;
        bool navigationHeld = closingHeld || sample.NavigationHeld;
        if (!context.TextEditing && !navigationHeld && !sample.AnyKey && context.Frame > onlineTextBlockThroughFrame)
            onlineTextReleaseBlocked = false;
        if (onlineReleaseBlocked)
        {
            if (!sample.Toggle && !sample.Cancel) onlineReleaseBlocked = false;
            return OnlineAction.None;
        }
        if (OnlineCapturesText(context.TextEditing, context.Frame)) return OnlineAction.None;
        if (togglePressed) return OnlineAction.Toggle;
        return context.Open && cancelPressed ? OnlineAction.Back : OnlineAction.None;
    }
}
