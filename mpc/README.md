# ANS 1964

An independent, circuit- and spec-informed emulation of the **1964 five-disc ANS
photoelectronic synthesizer** (Evgeny Murzin), for Akai MPC standalone hardware and
desktop VST2 hosts.

Project source and issues: <https://github.com/tkleisas/ans-1964>. GPL-3.0-or-later.

This is not a sample player and not a conventional subtractive synth. It reproduces the
ANS signal chain:

- **720 optical sine tracks** on the exact 1/72-octave grid (16.67 cents, 1/6 semitone),
  21 Hz to ~21.4 kHz.
- **Score scanner**: a drawn sonogram where vertical position = pitch, horizontal = time,
  line width = level *and* spectral spread. Wide scratches spill into neighbouring tracks
  and become bands, clusters or noise ("sound spots").
- **20 photomultiplier channels**, 36 tracks (half an octave) each, with a 6N2P-style
  switchable preamp, the dynamics/modulation manipulator, a two-stage 0.5-octave
  twin-T-style bandpass, and per-channel level presets.
- **16-harmonic coder** with vowel/bright/dark formant templates and a decay control,
  playable from MIDI notes or the test gate.
- Optical imperfections: reader blur/crosstalk, per-disc wow and flutter, emulsion/track
  noise, HF loss, optional 100 Hz lamp hum.

The built-in scores are original studies in the ANS idiom. No score images or audio
recordings from the instrument are included.

---

## Install on an MPC

Requires **MPC OS 3.10.1 or later**.

1. Build with the top-level `./build.sh` (or use the prebuilt folder from the release).
   The script clones the pinned MPC Plugin SDK, copies this `mpc/` project into it, and
   runs the SDK build. Manual build: from the SDK root, copy this folder to
   `plugins/ANS 1964`, then run `python3 tools/build.py "plugins/ANS 1964" --target all`.
2. Copy the whole `build/bokontep - VST - ANS 1964/` folder into a folder named
   `VST2 Plugins` on a USB drive or SD card (create it if needed).
3. On the MPC: **Preferences → Enhancements (Beta)** → enable **Enable Enhancements**
   and **External Plugin Support**.
4. Insert the plugin on an instrument track. It appears under **External Plugins**.

The plugin has no window on the MPC. Its interface is six tabs: **SCORE** (with an
**EDIT** sub-tab — press the SCORE function key twice), **SPEC A**, **SPEC B**,
**CODER** and **COLOR**, plus the Q-Link knobs.

## Desktop VST2

The same native build loads in a VST2-capable host. On Linux the build produces
`build/native/ans-1964.so` (and a `.dylib` / `.dll` on macOS / Windows). It has no
custom editor: use the host's generic parameter panel and automation, or the MPC JSON
UI if you are testing in the SDK host simulator. MIDI notes play the coder.

For a graphical editor, use the Linux **VST3** in this project's `vst3/` folder
(`vst3/README.md`; it installs to `~/.vst3/ANS 1964.vst3` and documents the YAWN
integration). It shares the same `ANSDsp.h` engine.

---

## Pages and controls

**SCORE**
- `Score` — built-in study or `User Score` (from `Content/Scores/user.ansscore`).
- `Run` — start/stop the scan. When stopped, the score freezes at the current position.
- `Speed` — 0–24 mm/s. Lower values stretch time; the original maximum was 24 mm/s.
- `Backward` — Off = forward, On = reverse.
- `Loop` — One-shot / Loop / Ping-Pong.
- `Scan Pos` — manual scrub, 0–100 % of the score.
- `Score Level`, `Volume`.

**EDIT** (sub-tab of SCORE)
- On-device drawing surface for the **User Score** image.
- `XYPad` — horizontal = time, vertical = pitch. Move the puck to the cell you want
  to change. With `Flip Y` on, the pitch axis is inverted.
- `Draw` — while on, paints at the puck position. `Erase` — while on, removes.
  `Clear` clears the whole user image on the rising edge (toggle it off again after).
- `Brush Lvl` — 0–100 % brightness. `Brush Wd` — 1–128 tracks.
- `Edit Harm` — 1 draws a single track; 2–16 also draws a coder-style harmonic stack.
- `Audition` — freezes the scan at the EDIT X position so you hear the column you are
  drawing. Turn it off to let the score run normally.
- Select **User Score** and `Run` on the SCORE tab. Edits are kept in the MPC project
  state when you save the project; the `Content/Scores/user.ansscore` file remains the
  starting image. The pad background is a static snapshot of that starting image.
- `Edit X`/`Edit Y` can also be driven by Q-Links 1/2 on the EDIT page.

**SPECTRUM A/B**
- `Ch 1`–`Ch 20` — the 20 channel level presets. Ch 1 is the lowest half-octave
  (~21–30 Hz), Ch 20 the highest (~15–21 kHz). These are the ANS's main formant/timbre
  controls.

**CODER**
- `Coder Level`, `Harmonics` (1–16), `Formant` (Flat/A/E/I/O/U/Bright/Dark),
  `Frmt Shift` (0.5–2.0×), `Coder Decay` (release), `Coder Note`, `Coder Gate`.
- Play MIDI notes to use the coder polyphonically (8 voices). Each note draws a harmonic
  stack on the 1/72-octave grid, exactly where the mechanical coder would scratch it.
- `Coder Gate` auditions `Coder Note` from the panel.

**COLOR**
- `Wow Depth/Rate` — global speed drift and per-disc flutter.
- `Track Noise` — slow per-track amplitude "life" from the photographic emulsion.
- `Blur` — optical reader/aperture blur across adjacent tracks.
- `HF Loss` — top-octave loss from the finite track width.
- `Hum` — optional 100 Hz lamp ripple.
- `Dyn Mode` — Open / Dynamics (expander) / Modulation (animation).
- `Dyn Amount`, `Dyn Rate`, `Drive`, `Line Mode`, `Width`.
- `Line Mode` — Mono (both lines summed), Split (channels 1–10 left, 11–20 right),
  Stereo (same split with a width control).

---

## Built-in scores

| Name | Character |
|---|---|
| Vocal A | Five vowel chords, 28 harmonics, low-mid channel curve |
| Vocal O | Lower, darker 24-harmonic vowel cycle |
| Solaris | Low drone, slow high clusters, drifting glissandi |
| Harm Glide | Eight harmonics gliding at staggered times |
| Micro Arp | 144 steps on the 1/72-octave grid |
| Noise Swell | Full-height scratches: all 720 tones as swells/noise |
| Bell Garden | Inharmonic partials with exponential decays |
| Empty | No score (coder only) |
| User Score | `Content/Scores/user.ansscore` |

The score time is nominal time at 24 mm/s. At `Speed = 12` a 16 s score takes 32 s.

The **desktop editor** at `tools/ans_score_editor.py` gives a full visual drawing
surface (draw/erase, harmonic brush, PNG import/export, forward/backward preview, and
"Push to MPC"). It is optional; the EDIT tab covers quick edits on the device.

## User scores (.ansscore)

`Content/Scores/user.ansscore` is a simple raw image:

| Offset | Type | Value |
|---|---|---|
| 0 | 4 bytes | `"ANS1"` |
| 4 | u16 LE | version = 1 |
| 6 | u16 LE | width in pixels |
| 8 | u16 LE | height = 720 |
| 10 | u16 LE | pixels per mm (10 recommended) |
| 12 | u32 LE | pixel bytes = width × 720 |
| 16 | bytes | 8-bit grayscale, row 0 = highest pitch, row 719 = lowest |

Duration at full speed = `width / (px_per_mm × 24)` seconds. White = open mastic,
black = closed. To convert a PNG spectrogram:

```
python3 Content/Scores/png_to_ansscore.py my_score.png "Content/Scores/user.ansscore" --px-per-mm 10
# (the same tool is in the project's tools/ folder)
```

The converter resizes the image to 720 rows preserving the horizontal scale. Install
the updated `Content/Scores/user.ansscore` with the plugin, restart/reload the plugin,
then select **User Score**.

---

## Accuracy notes

Modeled from the instrument's own documents:

- 720 tones, 5 discs × 144 tracks, speeds 5.625/22.5/90/360/1440 RPM, 120→30 mm discs,
  0.31 mm tracks, 1/72-octave spacing, 20 × 36-track half-octave channels.
- Score width → level (0.1–1.0 mm, 20 dB) and spill into adjacent tracks.
- 16-cutter harmonic/formant coder and the drawn-sonogram workflow.
- Reader → FEU-2 → 6Zh32P pentode → 6N2P preamp → manipulator → two-stage twin-T
  bandpass → line 1/2, from the available schematics.
- Per-disc speed instability, optical blur, track noise and HF loss.

Approximated because the published schematics do not include the twin-T component values
or the manipulator's exact transfer curve:

- The channel filter is a 4th-order 0.5-octave bandpass (two cascaded RBJ stages,
  Q ≈ 1.85 each) rather than a component-level twin-T simulation.
- The manipulator is an envelope-following VCA with expander and modulation modes; its
  amount/rate are exposed for fine-tuning by ear.
- The tube stages use a soft asymmetric saturator; they are intentionally gentle.
- Dense channels (more than 12 lit tracks in one half-octave) switch to a band-limited
  noise source calibrated to the same RMS as the equivalent 36 sine partials. Sparse and
  tonal material always uses individual partials; a 192-partial cap remains as a backstop.
  This keeps a full 720-tone noise scratch cheap while preserving its spectrum.

The ANS recordings themselves carry tape/room colour. This plugin emulates the
instrument, not the 1960s recording chain. `Hum` and `Track Noise` default low/off.

A private spectral A/B against the CC BY-SA 2009 live ANS demonstration
("The ANS Synthesizer playing doodles", Wikimedia Commons) guided the extended vocal
harmonic series, the formant baseline (the fundamental is not deleted), and the low-mid
vocal channel curve. No recording is included or redistributed.

## CPU

Typical tonal scores use a few dozen partials and are cheap. A full-height noise scratch
uses the 20 band-noise sources instead of 720 oscillators, so it is cheaper than a dense
tonal cluster. If your MPC struggles, use a sparser score, reduce the number of
simultaneous coder notes, or lower `Track Noise`/`Blur` slightly.

## Sources and licence

Research sources are documented in `research/ANS_TECHNICAL_REPORT.md` and
`research/SOURCES.md` in the project repository: USSR certificate 118695, the 2025 Habr
technical article with the reader/channel/manipulator/filter schematics, Smirnov's ANS
specification page, and Kreichi/Krivnova/Egorov (2013). The patent is public domain;
the plugin contains no code or assets from those documents.

SPDX-License-Identifier: GPL-3.0-or-later. Copyright (C) 2026 bokontep.

The full GPL text is shipped as `Content/COPYING.txt`. The plugin is built with the
inMusic MPC Plugin SDK, which is MIT-licensed; its notice is in
`Content/THIRD_PARTY_LICENSES.txt`.
