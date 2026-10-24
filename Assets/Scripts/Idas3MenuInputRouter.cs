// Owns pause/online navigation edges, hold-repeat and transition release guards.
// Sampling, drawing, and the native simulation remain outside this state machine.
internal sealed partial class Idas3MenuInputRouter
{
    internal enum Action { None, OnlineNavigation, PauseBack, PauseActivate, PauseNavigate, OpenPause }
    internal struct Sample
    {
        internal bool Escape, Back, Confirm, CloseHeld;
        internal uint Buttons;
        internal int Horizontal, Vertical;
    }
    internal struct Context
    {
        internal bool Focused, OnlineBlocksInput, PauseOpen, CanOpenPause, PointerActive, NavigationHeld;
        internal double Now;
    }
    internal struct Command
    {
        internal Action Kind;
        internal int Horizontal, Vertical;
    }

    private Sample previous;
    private bool suppressClosingControls, openingReleaseBlocked, ownsFrame, focusReleaseBlocked;
    private int blockThroughFrame = -1, navigationAxis;
    private double nextNavigation;
    private readonly Idas3PointerNavigation pausePointer = new Idas3PointerNavigation();

    internal bool SuppressClosingControls => suppressClosingControls;

    internal void VisibilityChanged(bool open, int frame)
    {
        blockThroughFrame = frame + 1;
        navigationAxis = 0;
        openingReleaseBlocked = open;
        if (!open) suppressClosingControls = true;
    }

    internal void ResetNavigation()
    {
        previous = default;
        navigationAxis = 0;
    }

    internal void ObserveCovered(Sample sample, bool focused = true)
    {
        // Another modal consumes this sample. It must not become a fresh press
        // when that modal stops covering pause/online navigation.
        previous = sample;
        navigationAxis = 0;
        if (!focused) focusReleaseBlocked = true;
    }

    private static bool NavigationHeld(Sample sample) => sample.Escape || sample.Back || sample.Confirm ||
        sample.CloseHeld || (sample.Buttons & 0x301Fu) != 0 || sample.Horizontal != 0 || sample.Vertical != 0;

    internal Command Process(Sample sample, Context context)
    {
        bool escapePressed = sample.Escape && !previous.Escape;
        bool backPressed = sample.Back && !previous.Back;
        bool confirmPressed = sample.Confirm && !previous.Confirm;
        uint buttons = sample.Buttons & ~previous.Buttons;
        previous = sample;
        ownsFrame = false;
        // The host samples neutral while unfocused. That is not a physical
        // release: rearm menu actions only after a focused neutral sample.
        if (!context.Focused)
        {
            focusReleaseBlocked = true;
            navigationAxis = 0;
        }
        else if (!sample.CloseHeld) suppressClosingControls = false;

        // Online owns navigation before pause; the host forwards focus and
        // capture guards to that menu even while no command may be activated.
        if (context.OnlineBlocksInput)
        {
            ownsFrame = true;
            return new Command { Kind = Action.OnlineNavigation, Horizontal = sample.Horizontal, Vertical = sample.Vertical };
        }
        if (!context.Focused) return default;
        if (focusReleaseBlocked)
        {
            if (!NavigationHeld(sample) && !context.NavigationHeld) focusReleaseBlocked = false;
            return default;
        }
        if (context.PauseOpen)
        {
            ownsFrame = true;
            if (pausePointer.Block(context.PointerActive, context.NavigationHeld)) navigationAxis = 0;
            else if (openingReleaseBlocked)
            {
                if (!sample.CloseHeld) openingReleaseBlocked = false;
            }
            else if (escapePressed || backPressed || (buttons & 0x2000) != 0)
                return new Command { Kind = Action.PauseBack };
            else if (confirmPressed || (buttons & 0x1000) != 0)
                return new Command { Kind = Action.PauseActivate };
            else
            {
                int axis = sample.Vertical != 0 ? sample.Vertical : sample.Horizontal * 2;
                bool navigate = axis != 0 && (axis != navigationAxis || context.Now >= nextNavigation);
                if (navigate) nextNavigation = context.Now + (axis != navigationAxis ? .32 : .10);
                navigationAxis = axis;
                if (navigate)
                    return new Command { Kind = Action.PauseNavigate,
                        Horizontal = sample.Vertical == 0 ? sample.Horizontal : 0, Vertical = sample.Vertical };
            }
        }
        else if (escapePressed && context.CanOpenPause && !suppressClosingControls)
        {
            ownsFrame = true;
            return new Command { Kind = Action.OpenPause };
        }
        return default;
    }

    // Read after executing the command: its visibility callback may close or
    // open an overlay. The same press still belongs to this frame's menu owner.
    internal bool BlocksGameInput(bool pauseBlocksInput, bool returningToFrontend, int frame) =>
        ownsFrame || pauseBlocksInput || (suppressClosingControls && returningToFrontend) || frame <= blockThroughFrame;
}
