internal sealed partial class Idas3MenuInputRouter
{
    internal enum AttractAction { None, OpenOptions, NativeViewTap }
    internal struct AttractSample
    {
        internal bool ViewHeld, AnyControlHeld, StartHeld;
    }
    internal struct AttractContext
    {
        internal bool Focused, Blocked, Allowed;
        internal int Child;
        internal float DeltaSeconds;
    }
    internal struct AttractCommand
    {
        internal AttractAction Kind;
        internal bool BlocksGameInput;
    }
    private float attractHoldSeconds;
    private int attractTapChild;
    private bool attractReleaseBlocked;
    internal float AttractHoldProgress => System.Math.Min(1f, System.Math.Max(0f, attractHoldSeconds / .65f));
    internal void AttractVisibilityChanged()
    {
        attractHoldSeconds = 0;
        attractReleaseBlocked = true;
    }
    internal void ClearAttractReleaseGuard() { attractReleaseBlocked = false; }

    internal AttractCommand ProcessAttract(AttractSample sample, AttractContext context)
    {
        if (!context.Focused || context.Blocked)
        {
            attractHoldSeconds = 0;
            return new AttractCommand { BlocksGameInput = attractReleaseBlocked };
        }
        if (attractReleaseBlocked)
        {
            if (!sample.AnyControlHeld) attractReleaseBlocked = false;
            attractHoldSeconds = 0;
            return new AttractCommand { BlocksGameInput = true };
        }
        if (!context.Allowed)
        {
            attractHoldSeconds = 0;
            return default;
        }
        if (sample.ViewHeld)
        {
            if (attractHoldSeconds == 0) attractTapChild = context.Child;
            attractHoldSeconds += System.Math.Min(context.DeltaSeconds, .1f);
            return new AttractCommand { BlocksGameInput = true,
                Kind = attractHoldSeconds >= .65f ? AttractAction.OpenOptions : AttractAction.None };
        }
        // Ranking pages retain their original tap action. Releasing a hold on
        // another screen must not replay a stale tap or confirm Start.
        bool tap = attractHoldSeconds > 0 && !sample.StartHeld && attractTapChild == 12 && context.Child == attractTapChild;
        attractHoldSeconds = 0;
        return new AttractCommand { Kind = tap ? AttractAction.NativeViewTap : AttractAction.None };
    }

    internal struct SavePointerSample
    {
        internal bool Available, Held, Pressed, NavigationHeld;
        internal float X, Y;
    }
    internal struct SavePointerContext
    {
        internal bool Allowed, OwnsPointer;
    }
    internal struct SavePointerCommand
    {
        internal bool Hover, Click, BlocksGameInput;
    }
    private bool savePointerReleaseBlocked, savePointerTracked;
    private float previousSaveX, previousSaveY;
    internal bool SavePointerReleaseBlocked => savePointerReleaseBlocked;

    internal SavePointerCommand ProcessSavePointer(SavePointerSample sample, SavePointerContext context)
    {
        var command = new SavePointerCommand();
        if (context.Allowed && context.OwnsPointer && sample.Available)
        {
            // A stationary cursor cannot override a new default No or a
            // controller choice when the native confirmation appears.
            command.Hover = savePointerTracked && (sample.X != previousSaveX || sample.Y != previousSaveY);
            previousSaveX = sample.X; previousSaveY = sample.Y; savePointerTracked = true;
            if (sample.Held)
            {
                savePointerReleaseBlocked = true;
                command.Click = sample.Pressed;
            }
        }
        else savePointerTracked = false;
        command.BlocksGameInput = savePointerReleaseBlocked;
        if (context.Allowed && !sample.Held && !sample.NavigationHeld) savePointerReleaseBlocked = false;
        return command;
    }
}
