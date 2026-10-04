# Retrowave / Vaporwave meter repair

Source meter 76 (saved style 78) had two presentation omissions:

- `DriftNeon`, the permanently visible pink outline, was incorrectly classified as an active drift lamp by the name-based importer. The colored letters and glows still follow the original four-way `DriftLampColor` switcher; the outline now remains visible at rest and after the slide ends.
- The scrolling grid's `M_Homography_Inst` retainer was ignored. Its recovered corners are LT `(0.3, 0)`, RT `(0.7, 0)`, RB `(1, 1)`, LB `(0, 1)`. Both HUD shaders now inverse-project the retainer before sampling the existing scrolling/tiling/fading material. Ordinary layers explicitly disable the projection.

No new artwork was fabricated. Existing transmission labels, RPM faces, needles, pedal gauges, speed digits and shift triangles remain intact. Corner-speed telemetry and the previously documented approximation of the triangle material's color gradient are unchanged.

Validation:

- 31 source/composition checks: permanent outline, four grades, original projective corners and depth, outside-edge rejection, ordinary-layer exclusion, AT/MT labels, and four RPM scales.
- Unity GPU captures: idle outline; blue/green/orange/red drift states; scrolling grid versus a deliberately unprojected control; frozen animation clock; identical outline with and without drift.
- Bounds regenerated and cache verified for all 113 imported/custom layouts; renderer version 13.
- Windows player rebuilt successfully with Unity 6000.6.0f1. Evidence: `Verification/retrowave-gpu/` and `Logs/RetrowaveBuild.log`.

Reproduce with `Idas3MeterCatalogBuild.VerifyRetrowave`, or `VerifyRetrowaveAndBuild` after staging an existing Windows executable at `Builds/Current/InitialDUnity.exe`.
