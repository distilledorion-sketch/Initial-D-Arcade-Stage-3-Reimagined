# Evo III exhaust flash

`bkfire.idasmesh` and `textures.idastex` contain the recovered AS3 effect bank: nine mesh chunks and eight textures. Source and output hashes are in `manifest.json`. No replacement artwork is used.

To regenerate, run `Native/tools/extract_original_driving_effects.py --hostfs <legally supplied HOSTFS> --out <scratch directory> --banks bkfire`. Copy only `bkfire/bkfire.idasmesh` and `bkfire/textures.idastex` into this directory; the extractor's other files are research evidence, not required runtime files. `Tools/Stage-GameData.ps1` includes this directory in deployment inventories.

Source references: loader `0C17BEE0`, trigger `0C17CAC0`, draw owner `0C17CB00`, exhaust positions `0C323BB4`, Y rotations `0C323BD8`. The owner renders chunks 0 and 1 on successive frames. Its third exhaust adds X phase `0x059e` and 1.05 XY scale.

The host attaches the effect to the rendered car's body pose and uses the existing accepted Evo III misfire sound command for timing. The body-local mount omits the source owner's 0.3 m Y offset, which otherwise puts the effect above the host's exhaust. Effects volume does not control visibility. Drawing never consumes the driving RNG or emits another sound. Replay/opponent effect timing, the source random length jitter, and the source scene-light pulse are not implemented here; this is a local-player presentation restoration, not a claim of complete effect-owner parity.
