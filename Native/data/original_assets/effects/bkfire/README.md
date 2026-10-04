# Evo III exhaust flash

`bkfire.idasmesh` and `textures.idastex` contain the recovered AS3 effect bank: nine mesh chunks and eight textures. Source and output hashes are in `manifest.json`. No replacement artwork is used.

To regenerate, run `Native/tools/extract_original_driving_effects.py --hostfs <legally supplied HOSTFS> --out <scratch directory> --banks bkfire`. Copy only `bkfire/bkfire.idasmesh` and `bkfire/textures.idastex` into this directory; the extractor's other files are research evidence, not required runtime files. `Tools/Stage-GameData.ps1` includes this directory in deployment inventories.

Source references: loader `0C17BEE0`, trigger `0C17CAC0`, draw owner `0C17CB00`, exhaust positions `0C323BB4`, Y rotations `0C323BD8`. The owner renders chunks 0 and 1 on successive frames. Its third exhaust adds X phase `0x059e` and 1.05 XY scale.

The host attaches the effect to the rendered car's body pose and uses the existing accepted Evo III misfire sound command for timing. The body-local mount omits the source owner's 0.3 m Y offset, which otherwise puts the effect above the host's exhaust. Effects volume does not control visibility. Drawing never consumes the driving RNG or emits another sound. Confirmed online opponent events use the same two-frame presentation.

The source point-light pulse is restored for both local and online opponent flames: RGB (1, 0.8, 0.4), attenuation distances 0.001/7.45, and the source's fixed car-local light anchor (0.788, 0.227, -3.156). Its actor-world transform is separate from the body-only flame placement adjustment. Draw-only copies of course/player/opponent light sets retain the existing source lighting and mirror transforms. Replays still do not store the accepted flash event, and source random flame-length jitter remains unimplemented. See `docs/backfire-lighting-2026-10-04.md` for source validation and limits.
