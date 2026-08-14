# Guitar / Bass Fusion

The guitar/bass variants are now unified into a single VST3 per effect. Each fused effect exposes an `Instrument` selector with `Guitar` and `Bass` modes.

Bass mode is DSP-aware rather than just a preset: it reduces modulation/drive intensity where appropriate and protects low-end. Reverb uses a dedicated wet-only high-pass at 500 Hz in Bass mode, so the dry bass fundamental remains untouched while the reverb tail stays out of the sub/low region.

Fused plugins:
- Overdrive
- Distortion
- Fuzz
- Chorus
- Reverb
- Phaser
- Envelope Filter
- Octaver

The GitHub Actions build mechanism remains the same.
