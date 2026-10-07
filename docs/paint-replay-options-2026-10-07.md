# Shared paints and replay presentation options

Implemented after release .43; not published or installed to Desktop by this change.

## Player controls

- Display & Graphics > Sun Glare: On/Off. Apply saves it; Cancel discards the draft. Existing settings default to On. Replay Viewer reads the saved choice too.
- Car selection: the existing paint controls now cycle all 93 distinct factory colors. Eight swatches appear per page, with the current paint number and total. Every car retains its original factory colors at their original save indices, followed by the other colors.
- Replay Viewer > Camera: the existing four views plus Free Camera. C / Y cycles views.
- Free camera: WASD / left stick moves; hold the right mouse button or use the right stick to look. Q/E or LB/RB lowers/raises the camera. Shift increases movement speed; Ctrl slows it. A speed slider is available. R / right-stick click resets beside the recorded car. Camera movement uses real elapsed time, independently of playback speed and pause state.
- Game HUD: G / left-stick click toggles the race HUD, including custom instruments, ornaments and rearview mirror.
- Replay overlay: H / Select hides or restores viewer controls independently of the game HUD. Esc / B opens the library, which restores the overlay.

## Compatibility and release dependency

Paint IDs remain in the existing 32-bit profile/replay field; no save format change. Expanded colors use each model's original paintable material mask and default body variant, keeping fitted tuning parts. Stock color-dependent body variants are unchanged. Native opponent/replay validators, the managed replay reader and leaderboard replay validator accept the expanded palette.

The accompanying `Leaderboard/src/replay.mjs` validator was deployed on October 7 before preparing release .44. It accepts the expanded palette while preserving existing records and replay downloads. Both online participants need the matching client build as usual. Release .44 also updates the managed lobby-car validator to accept paint IDs 0 through 92.

The free camera is passed into native rendering before scenery selection, billboards and camera matrices are generated. It never advances the driving simulation or changes the stored replay. Leaving it restores the selected chase/bumper/overhead/orbit behavior. Replay display toggles are local to the viewer and do not change normal gameplay HUD settings.

## Verification

Evidence is under `Verification/paint-replay-20261007` (private, not release content):

- Native factory/player presentation suites pass, including 3,255 car/paint combinations, original color IDs, parts preservation, material RGB and paint wrap/confirmation.
- Unity settings/navigation suite passes 118 checks, including sun-glare migration, Apply, Cancel and reload.
- Unity production replay rendering passes 32 checks: five cameras, free position, seek isolation, invalid-camera rejection, independent HUD/overlay, expanded local/opponent replay paint acceptance, actual sun flare removal and restoration.
- Native showroom sweep passes 465 live selections across five models, confirmation/transmission transitions and night-race appearance retention.
- Actual captures inspected: chase HUD, free camera with/without HUD, sun glare on/off.
- Leaderboard worker suite passes all 59 tests, including expanded paint replay validation and invalid-ID rejection.

Physical controller freecam and a live two-account online race with new paints have not been exercised. No GitHub release, live service deployment or Desktop installation is part of this implementation.

### Subsequent Desktop installation

On October 7, the user requested all current work on Desktop. These options were included in the complete rebuilt player installed to `C:/Users/Chris/Desktop/Current`, alongside Gunsai and Odawara. Installed game files passed SHA-256 comparison and the Desktop executable passed the isolated startup check; personal saves/settings, ROM and custom music remained unchanged. Backups and proof: `Verification/desktop-all-20261007`. This installation preceded release .44 packaging and the subsequent leaderboard deployment.
