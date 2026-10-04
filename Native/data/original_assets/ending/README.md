# Legend ending artwork

Imported without geometry, color or UV edits from the user's `HOSTFS/model/ending` bank. The manifest records every input and output hash. No executable image or CHD is included.

Reproduce with `Native/tools/extract_original_ending.py --hostfs <HOSTFS> --out <destination>`.

The runtime uses chunks 3/4 for the staff roll and photo strips, and chunks 0/1/2 for the final Takumi/AE86 card. It restores original stream 12, **Dancin' in My Dreams**, as a one-shot. Scroll ranges/offsets come from `0C262738`; the phase timers and fades come from `0C0EB2C0`. The development reference image SHA-256 is `efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335`.

This restores the missing credits and final card. The original driving cinematic behind the roll is not implemented; credits currently appear over black. Start/Escape skips to the final card, then a new press skips to the title. Releasing the preceding dialogue's skip button is required before another skip can register.

`original_ending_tests` executes the original phase/scroll arithmetic for normal playback and five skip boundaries. `ending_application_tests` exercises the real host, artwork, audio ownership, four display frame rates, pause, skip, saved-profile stability and the full 31-rival result flow.
