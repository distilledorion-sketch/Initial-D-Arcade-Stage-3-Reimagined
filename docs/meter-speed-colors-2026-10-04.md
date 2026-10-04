# Preserve each meter's original speed colors

Double Ace's white speed digits were incorrectly tinted blue at 150–209 km/h. A separate palette shader also recolored 51 meter entries whose four numbered source atlases are byte-identical. Both were unsupported assumptions introduced by the September speed-color fixes.

Removed those tints from the shared race/preview renderer. The importer now leaves identical families fixed, respects specified digit brush colors (DAC White and Pop Team Epic Stay!), and retains Classic's pastel fourth atlas without adding a rainbow. The 23 designs with distinct eligible atlas families retain their texture selection; the existing high-speed hue animation is restricted to Meter00/Meter30's neutral fourth atlas. No texture artwork, gear colors, drift lamps, or needle effects were recolored.

`recover_meter_speed_colors.py` applies the same rules on reimport. Renderer bounds version 14 invalidates the old catalog cache. This supersedes the synthetic-palette and Double Ace tint statements in the September 26/27 audit notes.

Verification entry point: `Idas3MeterCatalogBuild.VerifySpeedColorsAndBuild`.

- Source audit compares 53 texture files with the original exported bytes and checks that other catalog fields are unchanged.
- `Verify-MeterBrushTint.py` checks all 103 specified S3 brush colors against the recovered widgets.
- GPU checks compare the same ones digit at 88, 98, 158, and 218 km/h for all 54 corrected fixed-color entries, with changing presentation time. Direct-atlas controls verify the remaining 23 families, including Classic's pastel art.
- Signal checks cover the speed thresholds, Double Ace's neutral tint, RPM warnings and HUD placement. The Unity build also regenerates and validates the layout cache.

Evidence is written to `Verification/meter-speed-colors` and `Logs/SpeedColorsBuild.log`. The full Double Ace render is `double-ace-199.png`.

The retained high-speed rainbow cadence is still the earlier reconstruction; this change does not claim exact execution of the original Unreal widget bytecode.
