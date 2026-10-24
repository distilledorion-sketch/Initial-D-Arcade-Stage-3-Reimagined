internal sealed partial class Idas3MenuInputRouter
{
    internal enum MusicAction { None, BlurSearch, Preview, Back, Activate, Delete, Navigate, OpenLobby, OpenOpponent }
    internal struct MusicSample
    {
        internal Sample Navigation;
        internal bool Delete, ViewHeld, AnyControlHeld, SearchEscapePressed, PreviewPressed;
    }
    internal struct MusicContext
    {
        internal bool Focused, Blocked, MenuOpen, BlocksGameInput, SearchFocused, PointerActive, NavigationHeld;
        internal bool LobbyAllowed, OpponentAllowed;
        internal float DeltaSeconds;
        internal double Now;
    }
    internal struct MusicCommand
    {
        internal MusicAction Kind;
        internal bool BlocksGameInput, ShowHoldProgress;
        internal int Horizontal, Vertical;
    }

    private MusicSample previousMusic;
    private float musicHoldSeconds;
    private bool musicReleaseBlocked, musicFocusReleaseBlocked;
    private int musicNavigationAxis;
    private double nextMusicNavigation;
    private readonly Idas3PointerNavigation musicPointer = new Idas3PointerNavigation();

    internal bool MusicReleaseBlocked => musicReleaseBlocked;
    internal float MusicHoldProgress => System.Math.Min(1f, System.Math.Max(0f, musicHoldSeconds / .65f));
    internal void BlockMusicUntilRelease() { musicReleaseBlocked = true; }
    internal void ClearMusicReleaseGuard() { musicReleaseBlocked = false; }
    internal void MusicSelectionChanged()
    {
        musicReleaseBlocked = true;
        musicNavigationAxis = 0;
        previousMusic = default;
    }
    internal void MusicVisibilityChanged()
    {
        MusicSelectionChanged();
        musicHoldSeconds = 0;
    }

    internal MusicCommand ProcessMusic(MusicSample sample, MusicContext context)
    {
        var previous = previousMusic;
        previousMusic = sample;
        var command = new MusicCommand { BlocksGameInput = context.BlocksGameInput || musicReleaseBlocked };
        if (!context.Focused) musicFocusReleaseBlocked = true;
        if (!context.Focused || context.Blocked)
        {
            musicHoldSeconds = 0;
            return command;
        }
        if (musicFocusReleaseBlocked)
        {
            // A neutral packet produced while unfocused must not turn a held
            // confirm, back, preview or direction into a new menu action.
            if (!sample.AnyControlHeld && !NavigationHeld(sample.Navigation) && !sample.Delete &&
                !sample.ViewHeld && !sample.SearchEscapePressed && !sample.PreviewPressed)
                musicFocusReleaseBlocked = false;
            command.BlocksGameInput |= context.MenuOpen || context.LobbyAllowed || context.OpponentAllowed;
            return command;
        }
        if (musicReleaseBlocked)
        {
            if (!sample.AnyControlHeld) musicReleaseBlocked = false;
            command.BlocksGameInput = true; // The release frame is consumed too.
            return command;
        }
        if (context.MenuOpen)
        {
            command.BlocksGameInput = true;
            uint buttons = sample.Navigation.Buttons & ~previous.Navigation.Buttons;
            if (context.SearchFocused)
            {
                // Search text owns all ordinary bindings, including Delete.
                if (sample.SearchEscapePressed || (buttons & 0x2000) != 0) command.Kind = MusicAction.BlurSearch;
                return command;
            }
            if (musicPointer.Block(context.PointerActive, context.NavigationHeld)) return command;
            if (sample.PreviewPressed || (buttons & 0x8000) != 0) command.Kind = MusicAction.Preview;
            else if (sample.Navigation.Escape && !previous.Navigation.Escape ||
                     sample.Navigation.Back && !previous.Navigation.Back || (buttons & 0x2000) != 0) command.Kind = MusicAction.Back;
            else if (sample.Navigation.Confirm && !previous.Navigation.Confirm || (buttons & 0x1000) != 0) command.Kind = MusicAction.Activate;
            else if (sample.Delete && !previous.Delete || (buttons & 0x4000) != 0) command.Kind = MusicAction.Delete;
            else
            {
                int horizontal = sample.Navigation.Horizontal, vertical = sample.Navigation.Vertical;
                int axis = vertical != 0 ? vertical : horizontal * 2;
                if (axis != 0 && (axis != musicNavigationAxis || context.Now >= nextMusicNavigation))
                {
                    command.Kind = MusicAction.Navigate;
                    command.Horizontal = horizontal;
                    command.Vertical = vertical;
                    nextMusicNavigation = context.Now + (axis != musicNavigationAxis ? .30 : .10);
                }
                musicNavigationAxis = axis;
            }
            return command;
        }
        if ((context.OpponentAllowed || context.LobbyAllowed) && sample.ViewHeld)
        {
            musicHoldSeconds += System.Math.Min(context.DeltaSeconds, .1f);
            if (musicHoldSeconds >= .65f) command.Kind = context.LobbyAllowed ? MusicAction.OpenLobby : MusicAction.OpenOpponent;
            command.BlocksGameInput = true;
        }
        else musicHoldSeconds = 0;
        command.ShowHoldProgress = true;
        return command;
    }
}
