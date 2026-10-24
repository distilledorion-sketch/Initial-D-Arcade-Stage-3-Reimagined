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
        var pointer = Paused(2); pointer.PointerBlocksNavigation = true;
        Check(router.Process(confirm, pointer).Kind == Router.Action.None, "pointer ownership prevents duplicate controller activation");
        Check(router.Process(confirm, Paused(2.1)).Kind == Router.Action.None, "held confirm is not replayed after pointer coverage");
        router.ResetNavigation();
        Check(router.Process(down, Paused(3)).Kind == Router.Action.PauseNavigate, "returning to a menu resets stale navigation");

        router = new Router();
        driving.CanOpenPause = false;
        Check(router.Process(start, driving).Kind == Router.Action.None, "race phase restriction prevents opening pause");
        Check(!router.BlocksGameInput(false, false, 25), "ordinary uncovered gameplay is not blocked");
    }
}
