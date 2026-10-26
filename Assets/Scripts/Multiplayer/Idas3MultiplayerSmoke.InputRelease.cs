using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using UnityEngine;

namespace Idas3.Multiplayer
{
    public sealed partial class Idas3MultiplayerSmoke
    {
        private bool InputReleaseCheck => Array.IndexOf(Environment.GetCommandLineArgs(), "-idas3-multiplayer-input-release-check") >= 0;
        private readonly List<string> inputReleaseScenarios = new List<string>();

        [Serializable] private class InputReleaseReport
        {
            public string scope = "Synthetic prior-menu guard state in real local TCP Unity races; normal binding and native submission paths. Not physical devices or Internet EOS.";
            public string[] scenarios;
            public bool passed;
        }

        private IEnumerator VerifyInputReleaseDuringRace(string scenario)
        {
            Check(session.IsRacing && !menu.IsOpen, "Input-release fixture requires a running race with no overlay.");
            // Seed the precise latch state left by a save click / closing
            // music and chat. Do not replace the final native input packet.
            var router = menu.InputRouter;
            router.ProcessSavePointer(new Idas3MenuInputRouter.SavePointerSample { Available = true, Held = true, Pressed = true },
                new Idas3MenuInputRouter.SavePointerContext { Focused = true, Allowed = true, OwnsPointer = true });
            router.BlockMusicUntilRelease();
            router.AttractVisibilityChanged();
            router.OnlineTextEnded(Time.frameCount);
            router.OnlineClosed();
            yield return Frames(5);
            Check(!menu.BlocksGameInput && !menu.CapturesTextInput, "Closed online menu kept the race's driving controls: " + scenario);
            Check(!router.SavePointerReleaseBlocked && !router.MusicReleaseBlocked, "Obsolete frontend guards survived race entry: " + scenario);
            Check((host.DiagnosticSubmittedInput.flags & 2u) == 0 && HasKey(host.DiagnosticSubmittedInput, 87),
                "Held throttle was neutralized by old menu state: " + scenario);
            inputReleaseScenarios.Add(scenario);
            File.WriteAllText(Path.Combine(root, "input-release.json"), JsonUtility.ToJson(new InputReleaseReport {
                passed = true, scenarios = inputReleaseScenarios.ToArray()
            }, true));
        }
    }
}
