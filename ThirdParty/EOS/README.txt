Epic Online Services desktop SDK
================================

The com.initiald.eos-sdk folder is a minimal local Unity package containing the
unmodified official Epic C# SDK and Windows/Linux x64 native binaries. It does
not install the PlayEveryWare managers, sample content, overlay bootstrapper,
RTC audio helper, or editor automation.

Upstream package: EOS Plugin for Unity v6.2.0, released September 22, 2026
Repository: https://github.com/EOS-Contrib/eos_plugin_for_unity
Commit: 65e44063d5f908002e74a3080343fb0be561dacb
Download: https://github.com/EOS-Contrib/eos_plugin_for_unity/releases/download/v6.2.0/com.playeveryware.eos-6.2.0.tgz
Archive SHA256: aafe5a1cc278f2f65e0373777706d0a1028eea49b9548ef4a7520cc50f647c4b
SDK version returned by the Windows binary: 1.19.2.1-58105819
Upstream package.json description says 1.19.1.2, but release notes, generated
VersionInterface constants, and the native EOS_GetVersion agree on 1.19.2.1.

Windows native binary: Runtime/Windows/x64/EOSSDK-Win64-Shipping.dll
Size: 19,548,600 bytes
SHA256: ab25af350c48856633d671a16e8469277f9acee39186d69fe5b2a26100d738e8
Authenticode signature verified valid; signer Epic Games Inc.

Linux native binary: Runtime/Linux/libEOSSDK-Linux-Shipping.so
Size: 26,700,576 bytes
SHA256: 05c88b7adb10bfe3dc89d133af7318422838ab6909c06dcd3e74bd2af4ac2309

Unity import
------------
Add com.initiald.eos-sdk with local path
file:../ThirdParty/EOS/com.initiald.eos-sdk to Packages/manifest.json.
No further Unity package dependencies are needed for the C# SDK itself.

The generated C# SDK is self-contained under Runtime/EOS_SDK, with assembly
definition com.Epic.OnlineServices. Normal Unity desktop symbols select the
correct native library. Do not define EOS_DISABLE in enabled builds.

This packaging enables each native binary only for its matching x64 player.
Native editor import is disabled so the game's editor loader can own complete
unload between play sessions. Player builds use P/Invoke automatically. Editor builds compile the
upstream SDK in dynamic binding mode and the game's platform bootstrap must
load the matching library and call Epic.OnlineServices.Bindings.Hook before
PlatformInterface.Initialize. Windows editor also requires WindowsBindings.Hook.
Use Bindings.Unhook (and WindowsBindings.Unhook) when unloading after shutdown.

Game integration
----------------
Use PlatformInterface.Initialize(ref InitializeOptions), then
PlatformInterface.Create(ref Options) once per process. Tick the platform on
the main thread, release the platform before SDK Shutdown, and keep callbacks
from calling disposed game objects. DisableOverlay/DisableSocialOverlay and
null RTCOptions avoid overlay, Epic Launcher, and voice dependencies.

GetConnectInterface().CreateDeviceId(ref options, context, callback) creates a
local pseudo-account. Success and DuplicateNotAllowed both lead to Login with
Credentials.Type=ExternalCredentialType.DeviceidAccessToken, Token=null,
UserLoginInfo.DisplayName set. Handle InvalidUser using CreateUser with the
returned ContinuanceToken. AddNotifyAuthExpiration requires a subsequent login
refresh; AddNotifyLoginStatusChanged reports lost authentication.

CreateDeviceIdOptions.DeviceModel is only a human-readable device description,
not a caller-chosen identity or per-process profile. Official SDK comments say
Device ID credentials belong to the local OS user and persist across desktop
reinstalls. Platform.Options.CacheDirectory is documented for Player/Title Data
Storage, not as a supported Device ID identity selector. Do not delete a real
Device ID to create a second test player: that can orphan its online identity.

GetLobbyInterface() supplies room discovery, attributes, membership, and lobby
notifications. GetP2PInterface() uses ProductUserId and SocketId; configure
RelayControl.AllowRelays for direct connection attempts with relay fallback.
The maximum packet payload is 1,170 bytes. Larger game messages need bounded
fragmentation/reassembly. PacketReliability.ReliableOrdered and
UnreliableUnordered provide the game's two delivery modes.

License
-------
The plugin's MIT permission notice is retained as LICENSE-PLUGIN.md. The EOS
SDK itself is governed by the Epic Online Services Developer Agreement, as
stated in Runtime/EOS_SDK/license.txt. SDK and native third-party notices are
retained alongside their corresponding files. The plugin license does not
override the SDK agreement. No agreement was accepted during this acquisition.

Reproduction
------------
Download the exact archive above, verify its SHA256, extract it in this folder
(creating package/), and run prepare_runtime.py with Python. The script refuses
to overwrite an existing runtime folder. Downloaded full archive/reference
package are ignored by Git; the minimal runtime and provenance are tracked.

Primary documentation
---------------------
https://dev.epicgames.com/docs/epic-online-services/eos-fundamentals/connect-interface
https://dev.epicgames.com/docs/game-services/p-2-p
https://github.com/EOS-Contrib/eos_plugin_for_unity/releases/tag/v6.2.0
