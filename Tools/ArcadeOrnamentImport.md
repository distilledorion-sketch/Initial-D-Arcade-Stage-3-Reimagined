# Hanging ornament import

Run `python Tools/Import-ArcadeOrnaments.py` from the repository. The default read-only source is `D:/Initial D games/Extracted Ornaments - The Arcade S3`; `--source` can select another copy of that recovery.

The source item table selects **280 actual ornament meshes**, with stable item IDs, plus **four skeletal chain types**. The importer creates 284 mesh resources and copies 218 original diffuse PNGs without changing their bytes. Inventory icons and contact sheets are never used as ornament geometry or artwork. The result has 147,962 vertices, 123,488 triangles, 6,223,220 bytes of geometry and 50,098,725 bytes of texture source files. Hashes and provenance are written to `Verification/ornaments-20260924/import-manifest.json`.

## Runtime representation

`Assets/Resources/ArcadeOrnaments/catalog.json` stores English display names, original Japanese names, source mesh references, original strap assignments, Unity-space bounds and diffuse material bindings. ID zero is reserved by the runtime for **Off**. Reserved source table rows are excluded.

Each `Meshes/*.bytes` file uses little-endian `ORN1`:

- Four-byte ASCII magic, followed by a 32-bit primitive count.
- Per primitive: signed 32-bit material index, vertex count and index count.
- Per vertex: eight floats, containing position XYZ, normal XYZ and UV.
- Per triangle: three signed 32-bit indices.

The exported glTF hangs down its positive Z axis, with its decorated face towards positive Y. Import converts position and normal `(x, y, z)` to `(x, -z, -y)`, reverses triangle winding and converts texture V to `1 - V`. The original geometry origin is retained. The resulting ornament hangs along Unity negative Y and faces negative Z. Units remain metres.

`Idas3OrnamentCatalog` loads only the lightweight catalog for selection. `LoadModel(id)` and `LoadStrap(id)` generate meshes for the selected items; returned `MeshParts` own those generated meshes and must be disposed. Textures and renderer materials have separate ownership so a live preview and the race renderer can share resource leases.

## Recovered data and reconstructed behavior

269 ornaments contain a recovered `key_chain` socket at the source origin with a 0.5 scale. Eleven have no serialized socket. The catalog records that distinction; all attachment assembly is marked as inferred because the original native method's placement transform is unavailable.

The four chains preserve source geometry, original joint palettes, bind positions and skin weights. Each chain's skeleton world-bind multiplied by its inverse-bind matrix was checked against identity with maximum error below 0.000001. `Rigs/strap_0001.json` through `strap_0004.json` use the same vertex order as the mesh resources. Run the importer with `--rigs-only` to regenerate those sidecars without touching geometry. Palette entry 4 is the fixed `ChainJoint0`; source joint indices are not renumbered.

A nine-node constrained simulation drives the recovered skeleton. The top ring/loop and first connector stay fixed; moving bone pivots preserve their recovered parent distances. Single-joint metal links rotate without stretching, while blended cord vertices flex. The actual deformed bottom ring determines pendant placement; a separate damped response controls its pitch, roll and twist. Vertical car inertia uses a bounded presentation gain to keep short road impulses visible at HUD scale. There is no periodic idle bob. Mounting and physics remain reconstructed behavior, not the original arcade simulation.

Live swing uses a read-only native car pose and simulation tick. Repeated frames and pauses cannot add acceleration; race transitions and discontinuities reset the pendulum. Replay decorations remain at rest because recorded pose interpolation does not expose a matching precise acceleration timebase.

Rendering blends adjacent 60 Hz solved poses using the native simulation clock's fractional render phase. Chain nodes interpolate before recovered-joint skinning, while pendant rotation uses quaternion interpolation; this keeps the rigid links and actual attachment intact on every rendered frame. Sampling, Verlet integration and acceleration are unchanged. Repeated rendered frames can advance the display phase without advancing physics. Pause holds the exact last displayed ornament pose, and discontinuities clear the interpolation history. Menu previews use the same interpolation with their preview clock.

The imported diffuse textures, masked alpha cutoff and sidedness follow the recovered glTF/material records. Unreal-specific material graphs, tint masks and specular behavior are not fully represented by that interchange recovery. No ROM or original game archive is copied into the Unity project.
