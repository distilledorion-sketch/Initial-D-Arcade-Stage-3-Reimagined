# Discord Rich Presence

The game displays **Initial D Arcade Stage 3** using application ID `1551296157567819807` and the supplied Stage 3 logo. Discord must be running locally with game activity enabled. No login prompt, bot token, client secret, or Steam linking is required.

Presence follows the existing game state: title and selection menus, Time Attack, online matchmaking/lobbies/battles, Legend of the Streets, Bunta Challenge, results, and the replay viewer. Races include the actual course direction and conditions. The elapsed clock uses race time. Replays show the recorded player/opponent names and conditions; the parent game yields presence until the viewer closes. The View Leaderboard button opens the public rankings.

Players can switch **Options > Gameplay > Discord Rich Presence** off and choose Apply. Closing the game clears its presence. Discord being absent or unavailable does not prevent playing.

The game samples its small native state snapshot once per second. Discord IPC uses the library's background thread; changed activities are queued no more often than every 15 seconds. No replay files, card data, credentials, room codes, IP addresses, or local paths are sent to Discord.

Dependencies: [DiscordRPC 1.6.1](https://github.com/Lachee/discord-rpc-csharp/releases/tag/v1.6.1) and [Newtonsoft.Json 13.0.3](https://github.com/JamesNK/Newtonsoft.Json/releases/tag/13.0.3). Their MIT licenses are included under `Assets/Plugins/Discord`.

DiscordRPC includes a two-line null-image fix for Discord replies that omit the optional small badge. The patched source is in `ThirdParty/DiscordRPC`; `Assets/Plugins/Discord/UPSTREAM.txt` records its upstream revision and rebuild command. Distributed players include both licenses in `InitialDUnity_Data/Plugins/Discord-LICENSES.txt`.

Developer verification uses private saves: run the player with `-idas3-attract-options-smoke <new-output-directory> -idas3-discord-check`. Add `-idas3-discord-live-check` with Discord running to verify the actual IPC handshake and activity acknowledgement. Normal diagnostics suppress Rich Presence.
