# Recovered meter import

`Import-ArcadeMeters.py` imports the 87 entries selected by the recovered The Arcade S3 meter registry. The default input is `D:/Initial D games/Extracted HUD Assets - The Arcade S3`. Override it with `--source` when the extraction lives elsewhere. The input must contain `Catalog/Meter Audit` and `Exports`.

```powershell
python Tools/Import-ArcadeMeters.py
# Rebuild metadata while checking the already imported texture copies:
python Tools/Import-ArcadeMeters.py --no-copy
```

The importer copies original converted PNG/HDR bytes without recoloring or resampling them, verifies their SHA-256 hashes, and emits the widget layout, material parameters, animation curves and alpha bounds. `ArcadeMeterNames.json` contains the display names. Missing source IDs 16, 33 and 59 are not invented. Saved style 0 remains Original and 1 remains Stuttgart; other styles use source ID + 2, so list changes do not change saved selections.

## Runtime adaptation

The Unity renderer supplies speed, RPM, gear, AT/MT, pedals, night state and the native drift signal to the recovered art and animation channels. It does not execute the original widget bytecode. Prius intentionally has no tachometer or gear readout. Individual designs expose different indicators.

Source masks are sampled in their retainer canvas, separately from animated children. Circle01 and Circle02 have opposite sweep conventions, established from the recovered polar-coordinate texture, artwork and curve endpoints. The unused duplicate Stuttgart brake animation is excluded. The Retrowave transmission textures use the opposite 01/02 convention to the other families. DAC White uses dark speed text for contrast. Sirius uses a normalized RPM reveal for its black cover because the original negative shader offsets cannot be directly reused.

Several material operators were stripped from the extraction. Radial gradation, sibling material tint and additive retainer composition are reconstructed approximations. Original-only audio-reactive, corner-speed and colored drift-grade inputs are unavailable and remain disabled rather than being presented as live functions. Some decorative/transient shader effects are not reproduced. Disabled source layers and reasons remain in `catalog.json` and the import manifest.

## Verification and build

Run Unity with a graphics device (omit `-nographics`):

```text
-batchmode -force-d3d11 -quit -projectPath <repo> -executeMethod Idas3MeterCatalogBuild.VerifyOnly -logFile <log>
```

This checks the catalog/settings and renders every meter through the production GPU renderer. It also bakes stable content bounds with catalog/alpha hashes, so browsing meters does not sample hundreds of animation states in the player. Missing or stale bounds fall back to sampling. Bump `RendererVersion` in `Idas3MeterLayoutBounds` when changing composition or bounds sampling, then rerun verification. `VerifyAndBuild` performs the checks and builds the local Windows player. The render report and actual low/high PNGs are written to a fresh directory under `Verification/meter-import-20260924`.

For real menu, persistence, layout and race checks, run the player with:

```text
-idas3-attract-options-smoke <new-output-directory> -idas3-hud-customization-check
```

The smoke run uses private saves. It still requires the user's original disc locally; no ROM is supplied by the import or build.
