using System;
using System.IO;
using Steamworks;
using UnityEngine;

namespace Idas3.Multiplayer
{
    // App 480 has its own native-action controller layout. This game's input
    // mapper expects ordinary gamepad output, including after the F1 overlay
    // closes. Bundle a legacy gamepad layout instead of inheriting Spacewar's.
    // Valve retains a manifest override until the Steam client session ends.
    internal sealed class Idas3SteamInputConfig : IDisposable
    {
        private bool initialized;
        private Callback<SteamInputConfigurationLoaded_t> configurationLoaded;
        internal void Initialize()
        {
            var args=Environment.GetCommandLineArgs();
            if(Array.IndexOf(args,"-idas3-online-controller-smoke")>=0 &&
               Array.IndexOf(args,"-idas3-online-controller-steam-baseline")>=0)
            {Debug.Log("IDAS3 Steam Input baseline: bundled mapping intentionally skipped for private diagnostic.");return;}
            string manifest=Path.GetFullPath(Path.Combine(Application.streamingAssetsPath,"SteamInput/steam_input_manifest.vdf"));
            if(!File.Exists(manifest))throw new FileNotFoundException("Bundled Steam gamepad mapping is missing.",manifest);
            configurationLoaded=Callback<SteamInputConfigurationLoaded_t>.Create(value=> {
                Debug.Log("IDAS3 Steam Input config loaded: app="+value.m_unAppID+" device="+value.m_ulDeviceHandle+
                    " revision="+value.m_unMajorRevision+"."+value.m_unMinorRevision+" native="+value.m_bUsesSteamInputAPI+" gamepad="+value.m_bUsesGamepadAPI);
            });
            if(!SteamInput.SetInputActionManifestFilePath(manifest))
                throw new InvalidOperationException("Steam Input did not accept the bundled gamepad mapping.");
            initialized=SteamInput.Init(false);
            if(!initialized)throw new InvalidOperationException("Steam Input could not initialize the gamepad mapping.");
            SteamInput.EnableDeviceCallbacks();
            SteamInput.RunFrame();
            Debug.Log("IDAS3 Steam Input: bundled gamepad manifest accepted. "+manifest);
        }
        public void Dispose()
        {
            configurationLoaded?.Dispose();configurationLoaded=null;
            if(!initialized)return;
            initialized=false;
            SteamInput.Shutdown();
        }
    }
}
