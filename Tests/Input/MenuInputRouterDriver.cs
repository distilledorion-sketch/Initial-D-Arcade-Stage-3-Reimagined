using System;
using Router = Idas3MenuInputRouter;

internal static class MenuInputRouterDriver
{
    private static void Check(bool condition, string name)
    {
        if (!condition) throw new Exception(name);
        Console.WriteLine("PASS\t" + name);
    }

    private static Router.Context Paused(double now = 0) => new Router.Context {
        Focused = true, PauseOpen = true, CanOpenPause = true, Now = now
    };

    private static void Main()
    {
        var router = new Router();
        var driving = new Router.Context { Focused = true, CanOpenPause = true };
        var start = new Router.Sample { Escape = true, Buttons = 0x10, CloseHeld = true };
        Check(router.Process(start, driving).Kind == Router.Action.OpenPause, "bound Start opens pause once");
        router.VisibilityChanged(true, 10);
        Check(router.Process(start, Paused()).Kind == Router.Action.None, "held opening Start cannot close pause");
        Check(router.Process(new Router.Sample { Buttons = 0x1010, CloseHeld = true }, Paused()).Kind == Router.Action.None,
            "rebound opening control cannot also activate a menu item");
        router.Process(default, Paused());
        Check(router.Process(new Router.Sample { Buttons = 0x1000, CloseHeld = true }, Paused()).Kind == Router.Action.PauseActivate,
            "fresh confirm activates after opening control release");
        Check(router.Process(new Router.Sample { Buttons = 0x1000, CloseHeld = true }, Paused()).Kind == Router.Action.None,
            "held confirm does not repeat activation");
        router.Process(default, Paused());
        Check(router.Process(new Router.Sample { Buttons = 0x2000, CloseHeld = true }, Paused()).Kind == Router.Action.PauseBack,
            "fresh controller back routes to pause");
        Check(router.Process(new Router.Sample { Back = true }, Paused()).Kind == Router.Action.PauseBack,
            "fresh keyboard Backspace routes to pause");
        router.VisibilityChanged(false, 15);
        Check(router.BlocksGameInput(false, false, 15), "closing action cannot leak to driving in the same frame");
        router.Process(new Router.Sample { Buttons = 0x2000, CloseHeld = true }, driving);
        Check(router.BlocksGameInput(false, false, 16), "following frame retains the visibility guard");
        Check(!router.BlocksGameInput(false, false, 17) && router.SuppressClosingControls,
            "after close guard unrelated driving is allowed while closing controls stay masked");
        Check(router.BlocksGameInput(false, true, 17), "held closing input cannot confirm the original frontend");
        router.Process(default, driving);
        Check(!router.SuppressClosingControls && !router.BlocksGameInput(false, true, 18),
            "release restores original frontend input");

        router = new Router();
        var online = Paused(); online.OnlineBlocksInput = true;
        Check(router.Process(new Router.Sample { Confirm = true, CloseHeld = true, Horizontal = 1 }, online).Kind == Router.Action.OnlineNavigation,
            "online menu owns input when pause and online overlap");
        Check(router.BlocksGameInput(false, false, 20), "online menu input never reaches native driving");
        online.Focused = false;
        Check(router.Process(default, online).Kind == Router.Action.OnlineNavigation,
            "unfocused online menu still receives its blocked poll for release handling");

        router = new Router();
        var unfocused = Paused(); unfocused.Focused = false;
        var confirm = new Router.Sample { Confirm = true, CloseHeld = true };
        Check(router.Process(confirm, unfocused).Kind == Router.Action.None, "unfocused pause cannot activate");
        Check(router.Process(confirm, Paused()).Kind == Router.Action.None, "focus restoration does not replay held confirm");
        router.Process(default, Paused());
        Check(router.Process(confirm, Paused()).Kind == Router.Action.PauseActivate, "fresh confirm works after focus restoration");
        router.ObserveCovered(new Router.Sample { Escape = true, CloseHeld = true });
        Check(router.Process(new Router.Sample { Escape = true, CloseHeld = true }, driving).Kind == Router.Action.None,
            "a press consumed by music or capture cannot reopen pause");

        router = new Router();
        var down = new Router.Sample { Vertical = 1 };
        Check(router.Process(down, Paused(1)).Kind == Router.Action.PauseNavigate, "fresh direction navigates immediately");
        Check(router.Process(down, Paused(1.2)).Kind == Router.Action.None, "held direction waits before repetition");
        Check(router.Process(down, Paused(1.33)).Kind == Router.Action.PauseNavigate, "held direction repeats after the initial delay");
        Check(router.Process(down, Paused(1.38)).Kind == Router.Action.None, "repeat interval prevents excess navigation");
        Check(router.Process(down, Paused(1.44)).Kind == Router.Action.PauseNavigate, "continued hold repeats at the menu rate");
        var changed = router.Process(new Router.Sample { Horizontal = -1 }, Paused(1.45));
        Check(changed.Kind == Router.Action.PauseNavigate && changed.Horizontal == -1 && changed.Vertical == 0,
            "a changed axis navigates immediately in its new direction");
        var diagonal = router.Process(new Router.Sample { Horizontal = 1, Vertical = -1 }, Paused(1.46));
        Check(diagonal.Horizontal == 0 && diagonal.Vertical == -1, "vertical navigation takes priority over a diagonal");
        var pointer = Paused(2); pointer.PointerActive = true; pointer.NavigationHeld = true;
        Check(router.Process(confirm, pointer).Kind == Router.Action.None, "pointer ownership prevents duplicate controller activation");
        pointer.PointerActive = false; pointer.Now = 2.1;
        Check(router.Process(confirm, pointer).Kind == Router.Action.None, "held confirm is not replayed after pointer coverage");
        router.Process(default, Paused(2.2));
        router.ResetNavigation();
        Check(router.Process(down, Paused(3)).Kind == Router.Action.PauseNavigate, "returning to a menu resets stale navigation");

        router = new Router();
        driving.CanOpenPause = false;
        Check(router.Process(start, driving).Kind == Router.Action.None, "race phase restriction prevents opening pause");
        Check(!router.BlocksGameInput(false, false, 25), "ordinary uncovered gameplay is not blocked");
        MusicChecks();
        FrontendChecks();
        FocusGapChecks();
    }

    private static void FocusGapChecks()
    {
        // The host does not sample ordinary keyboard/gamepad input while
        // unfocused. Its zero packet does not prove a physical release.
        foreach (var held in new[] {
            new Router.Sample { Confirm = true, CloseHeld = true },
            new Router.Sample { Back = true, CloseHeld = true },
            new Router.Sample { Escape = true, CloseHeld = true },
            new Router.Sample { Buttons = 0x1000, CloseHeld = true },
            new Router.Sample { Buttons = 0x2000, CloseHeld = true },
            new Router.Sample { Horizontal = 1 },
            new Router.Sample { Vertical = -1 }
        })
        {
            var router = new Router();
            router.Process(held, Paused());
            var away = Paused(); away.Focused = false;
            router.Process(default, away);
            router.Process(default, away);
            Check(router.Process(held, Paused(10)).Kind == Router.Action.None,
                "neutral focus gap cannot replay pause control " + held.Buttons + "/" + held.Horizontal + "/" + held.Vertical + "/" + held.Confirm + "/" + held.Back + "/" + held.Escape);
            Check(router.Process(held, Paused(20)).Kind == Router.Action.None, "held pause control remains blocked after refocus");
            router.Process(default, Paused(21));
            Check(router.Process(held, Paused(22)).Kind != Router.Action.None, "pause control resumes after focused release");
        }

        var closing = new Router();
        var driving = new Router.Context { Focused = true, CanOpenPause = true };
        closing.VisibilityChanged(false, 10);
        var unfocused = driving; unfocused.Focused = false;
        closing.Process(default, unfocused);
        Check(closing.SuppressClosingControls, "unfocused neutral cannot clear closing controls");
        var confirm = new Router.Sample { Confirm = true, CloseHeld = true };
        closing.Process(confirm, driving);
        Check(closing.BlocksGameInput(false, true, 20), "held closing control stays blocked from frontend after refocus");
        Check(!closing.BlocksGameInput(false, false, 20), "focus release guard does not block unrelated driving");
        closing.Process(default, driving);
        Check(!closing.SuppressClosingControls, "focused closing-control release clears guard");

        foreach (var held in new[] {
            new Router.MusicSample { Navigation = confirm, AnyControlHeld = true },
            new Router.MusicSample { Navigation = new Router.Sample { Buttons = 0x2000 }, AnyControlHeld = true },
            new Router.MusicSample { Delete = true, AnyControlHeld = true },
            new Router.MusicSample { Navigation = new Router.Sample { Vertical = 1 }, AnyControlHeld = true },
            new Router.MusicSample { Navigation = new Router.Sample { Buttons = 0x8000 }, AnyControlHeld = true }
        })
        {
            var router = new Router();
            router.ProcessMusic(held, MusicOpen());
            var away = MusicOpen(); away.Focused = false;
            router.ProcessMusic(default, away);
            Check(router.ProcessMusic(held, MusicOpen(10)).Kind == Router.MusicAction.None, "neutral focus gap cannot replay music action");
            Check(router.ProcessMusic(held, MusicOpen(20)).Kind == Router.MusicAction.None, "music held action waits for focused release");
            router.ProcessMusic(default, MusicOpen(21));
            Check(router.ProcessMusic(held, MusicOpen(22)).Kind != Router.MusicAction.None, "music action resumes after focused release");
        }
    }

    private static Router.MusicContext MusicOpen(double now = 0) => new Router.MusicContext {
        Focused = true, MenuOpen = true, BlocksGameInput = true, Now = now
    };

    private static void MusicChecks()
    {
        var router = new Router();
        var selection = new Router.MusicContext { Focused = true, OpponentAllowed = true, DeltaSeconds = .1f };
        var view = new Router.MusicSample { ViewHeld = true, AnyControlHeld = true };
        var command = router.ProcessMusic(view, selection);
        Check(command.Kind == Router.MusicAction.None && command.BlocksGameInput && router.MusicHoldProgress > 0,
            "short view hold is consumed without opening the music picker");
        router.ProcessMusic(default, selection);
        Check(router.MusicHoldProgress == 0, "releasing view cancels partial music hold");
        selection.DeltaSeconds = 5;
        Check(router.ProcessMusic(view, selection).Kind == Router.MusicAction.None,
            "one slow frame cannot instantly open the music picker");
        for (int i = 0; i < 6; ++i) command = router.ProcessMusic(view, selection);
        Check(command.Kind == Router.MusicAction.OpenOpponent && command.BlocksGameInput,
            "sustained view hold opens opponent music and consumes underlying input");
        router.MusicVisibilityChanged();
        Check(router.MusicHoldProgress == 0 && router.MusicReleaseBlocked, "music visibility resets hold and arms release guard");
        var confirm = new Router.MusicSample { Navigation = new Router.Sample { Confirm = true }, AnyControlHeld = true };
        Check(router.ProcessMusic(confirm, MusicOpen()).Kind == Router.MusicAction.None,
            "held opening input cannot select music immediately");
        command = router.ProcessMusic(default, MusicOpen());
        Check(command.BlocksGameInput && !router.MusicReleaseBlocked, "music release frame stays consumed");
        Check(router.ProcessMusic(confirm, MusicOpen()).Kind == Router.MusicAction.Activate, "fresh music confirm selects after release");
        Check(router.ProcessMusic(confirm, MusicOpen()).Kind == Router.MusicAction.None, "held music confirm selects only once");
        router.MusicVisibilityChanged();
        Check(router.ProcessMusic(confirm, selection).BlocksGameInput, "held music-closing confirm cannot start the underlying race");
        router.ProcessMusic(default, selection);
        Check(!router.ProcessMusic(default, selection).BlocksGameInput, "music closing guard releases ordinary gameplay");

        router = new Router();
        var covered = MusicOpen(); covered.Blocked = true;
        Check(router.ProcessMusic(confirm, covered).Kind == Router.MusicAction.None, "binding capture prevents music activation");
        Check(router.ProcessMusic(confirm, MusicOpen()).Kind == Router.MusicAction.None,
            "confirm begun during capture cannot activate when coverage ends");
        selection.DeltaSeconds = .1f;
        router.ProcessMusic(view, selection);
        var unfocused = selection; unfocused.Focused = false;
        router.ProcessMusic(view, unfocused);
        Check(router.MusicHoldProgress == 0, "focus loss cancels music hold progress");

        router = new Router();
        var search = MusicOpen(); search.SearchFocused = true;
        var deletion = new Router.MusicSample { Delete = true, AnyControlHeld = true };
        Check(router.ProcessMusic(deletion, search).Kind == Router.MusicAction.None, "Delete edits search without deleting a song");
        Check(router.ProcessMusic(confirm, search).Kind == Router.MusicAction.None, "Enter in search cannot select a song");
        var mappedEscape = new Router.MusicSample { Navigation = new Router.Sample { Escape = true } };
        Check(router.ProcessMusic(mappedEscape, search).Kind == Router.MusicAction.None,
            "a mapped gameplay Escape does not bypass search ownership");
        mappedEscape.SearchEscapePressed = true;
        Check(router.ProcessMusic(mappedEscape, search).Kind == Router.MusicAction.BlurSearch, "physical Escape only blurs search");
        Check(router.ProcessMusic(new Router.MusicSample { Navigation = new Router.Sample { Buttons = 0x2000 } }, search).Kind == Router.MusicAction.BlurSearch,
            "fresh controller back only blurs search");
        Check(router.ProcessMusic(deletion, MusicOpen()).Kind == Router.MusicAction.Delete, "fresh Delete outside search requests song deletion");
        router.MusicSelectionChanged();
        Check(router.ProcessMusic(deletion, MusicOpen()).Kind == Router.MusicAction.None, "delete completion waits for release before another action");
        router.ClearMusicReleaseGuard();

        router = new Router();
        var pointer = MusicOpen(); pointer.PointerActive = true; pointer.NavigationHeld = true;
        var preview = new Router.MusicSample { PreviewPressed = true, Navigation = new Router.Sample { Buttons = 0x8000 } };
        Check(router.ProcessMusic(preview, pointer).Kind == Router.MusicAction.None, "music pointer ownership suppresses preview shortcut");
        preview.PreviewPressed = false;
        pointer.PointerActive = false;
        Check(router.ProcessMusic(preview, pointer).Kind == Router.MusicAction.None, "held preview button is not replayed after pointer coverage");
        router.ProcessMusic(default, MusicOpen());
        Check(router.ProcessMusic(preview, MusicOpen()).Kind == Router.MusicAction.Preview, "fresh controller preview toggles playback");
        Check(router.ProcessMusic(new Router.MusicSample { Navigation = new Router.Sample { Back = true } }, MusicOpen()).Kind == Router.MusicAction.Back,
            "keyboard Backspace goes back in music");
        var direction = new Router.MusicSample { Navigation = new Router.Sample { Horizontal = 1, Vertical = -1 } };
        command = router.ProcessMusic(direction, MusicOpen(1));
        Check(command.Kind == Router.MusicAction.Navigate && command.Horizontal == 1 && command.Vertical == -1,
            "music forwards both axes for device-specific menu navigation");
        Check(router.ProcessMusic(direction, MusicOpen(1.2)).Kind == Router.MusicAction.None, "music navigation waits through initial hold delay");
        Check(router.ProcessMusic(direction, MusicOpen(1.31)).Kind == Router.MusicAction.Navigate, "music navigation repeats after initial delay");

        router = new Router();
        selection.OpponentAllowed = false; selection.LobbyAllowed = true;
        for (int i = 0; i < 7; ++i) command = router.ProcessMusic(view, selection);
        Check(command.Kind == Router.MusicAction.OpenLobby, "lobby hold opens the lobby music context");
        selection.LobbyAllowed = false;
        command = router.ProcessMusic(view, selection);
        Check(command.Kind == Router.MusicAction.None && !command.BlocksGameInput && router.MusicHoldProgress == 0,
            "music opening stops when its context becomes unavailable");
    }

    private static void FrontendChecks()
    {
        var router = new Router();
        var context = new Router.AttractContext { Focused = true, Allowed = true, Child = 12, DeltaSeconds = .1f };
        var view = new Router.AttractSample { ViewHeld = true, AnyControlHeld = true };
        var command = router.ProcessAttract(view, context);
        Check(command.BlocksGameInput && command.Kind == Router.AttractAction.None, "ranking view press waits to distinguish tap from hold");
        command = router.ProcessAttract(default, context);
        Check(!command.BlocksGameInput && command.Kind == Router.AttractAction.NativeViewTap, "ranking tap reaches the original screen once on release");
        Check(router.ProcessAttract(default, context).Kind == Router.AttractAction.None, "released ranking tap cannot repeat");
        for (int i = 0; i < 7; ++i) command = router.ProcessAttract(view, context);
        Check(command.Kind == Router.AttractAction.OpenOptions && command.BlocksGameInput, "holding view opens options without changing ranking page");
        router.AttractVisibilityChanged();
        Check(router.AttractHoldProgress == 0 && router.ProcessAttract(view, context).Kind == Router.AttractAction.None,
            "options visibility consumes the opening hold");
        context.Focused = false;
        Check(router.ProcessAttract(default, context).BlocksGameInput, "unfocused neutral input cannot release the options closing guard");
        context.Focused = true;
        Check(router.ProcessAttract(view, context).BlocksGameInput, "refocusing with held view cannot reopen options");
        Check(router.ProcessAttract(default, context).BlocksGameInput, "options release frame remains consumed");
        Check(!router.ProcessAttract(default, context).BlocksGameInput, "original attract controls resume after physical release");

        router.ProcessAttract(view, context); context.Blocked = true;
        router.ProcessAttract(default, context); context.Blocked = false;
        Check(router.AttractHoldProgress == 0 && router.ProcessAttract(default, context).Kind == Router.AttractAction.None,
            "input capture cancels a pending ranking tap");
        router.ProcessAttract(view, context); context.Child = 0;
        Check(router.ProcessAttract(default, context).Kind == Router.AttractAction.None, "changing attract screens cancels a pending ranking tap");
        context.Child = 12; router.ProcessAttract(view, context);
        Check(router.ProcessAttract(new Router.AttractSample { StartHeld = true }, context).Kind == Router.AttractAction.None,
            "Start takes precedence over a simultaneous ranking tap release");
        context.DeltaSeconds = 5;
        Check(router.ProcessAttract(view, context).Kind == Router.AttractAction.None && router.AttractHoldProgress < .2f,
            "a single slow frame cannot open attract options");
        context.Allowed = false;
        Check(!router.ProcessAttract(view, context).BlocksGameInput && router.AttractHoldProgress == 0,
            "leaving attract resets the pending hold and returns input");
        router.AttractVisibilityChanged(); router.ClearAttractReleaseGuard();
        Check(!router.ProcessAttract(default, context).BlocksGameInput, "admitting a challenger clears the obsolete attract release guard");

        var save = new Router.SavePointerContext { Allowed = true, OwnsPointer = true };
        var mouse = new Router.SavePointerSample { Available = true, X = 20, Y = 30 };
        Check(!router.ProcessSavePointer(mouse, save).Hover, "stationary cursor does not override a new save menu default");
        Check(!router.ProcessSavePointer(mouse, save).Hover, "stationary cursor leaves controller selection intact");
        mouse.X = 25;
        Check(router.ProcessSavePointer(mouse, save).Hover, "moving cursor enables native save menu hover");
        mouse.Held = true; mouse.Pressed = true;
        var pointer = router.ProcessSavePointer(mouse, save);
        Check(pointer.Click && pointer.BlocksGameInput && router.SavePointerReleaseBlocked, "save click owns the native confirmation frame");
        mouse.Pressed = false;
        Check(!router.ProcessSavePointer(mouse, save).Click, "holding the save click never repeats activation");
        save.OwnsPointer = false;
        Check(router.ProcessSavePointer(mouse, save).BlocksGameInput, "click that leaves save selection cannot confirm the following screen");
        save.Allowed = false; mouse.Held = false;
        Check(router.ProcessSavePointer(mouse, save).BlocksGameInput && router.SavePointerReleaseBlocked,
            "unfocused save pointer sample cannot release a held transition");
        save.Allowed = true; mouse.NavigationHeld = true;
        Check(router.ProcessSavePointer(mouse, save).BlocksGameInput, "controller held across save click remains covered");
        mouse.NavigationHeld = false;
        Check(router.ProcessSavePointer(mouse, save).BlocksGameInput && !router.SavePointerReleaseBlocked,
            "save pointer consumes the neutral release frame");
        Check(!router.ProcessSavePointer(mouse, save).BlocksGameInput, "fresh input reaches the next screen after save release");
        save.OwnsPointer = true; mouse.Y = 100;
        Check(!router.ProcessSavePointer(mouse, save).Hover, "returning to save menu starts a fresh pointer position baseline");
        mouse.Y = 110;
        Check(router.ProcessSavePointer(mouse, save).Hover, "pointer hover resumes only on subsequent movement");
        mouse.Available = false; mouse.Held = true; mouse.Pressed = true;
        pointer = router.ProcessSavePointer(mouse, save);
        Check(!pointer.Hover && !pointer.Click && !pointer.BlocksGameInput, "private diagnostics cannot accidentally use the desktop mouse");
    }
}
