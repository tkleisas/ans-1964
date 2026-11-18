# ANS 1964

An accurate, circuit- and spec-informed emulation of the Soviet **ANS**
photoelectronic synthesizer (Evgeny Murzin, 1964 five-disc version), packaged as:

- an **Akai MPC OS 3.10.1+ VST2 instrument**,
- a generic **desktop VST2**,
- a **Linux VST3 with a full graphical score editor** for YAWN and other VST3 hosts.

The plugin is **GPL-3.0-or-later**. The MPC Plugin SDK used to build it is
MIT-licensed; see [`LICENSES/MPC-Plugin-SDK-MIT.txt`](LICENSES/MPC-Plugin-SDK-MIT.txt).
The plugin does not contain any Steinberg VST SDK code.

“ANS” is the name of the original instrument and is used here descriptively. This
project is not affiliated with inMusic, Akai Professional, Steinberg, the Murzin
estate, or the original designers. See [`NOTICE.md`](NOTICE.md).

## Download

Use the assets on the [latest release](https://github.com/tkleisas/ans-1964/releases/latest):

| Asset | What it is |
|---|---|
| `ans-1964-1.1.2-mpc.zip` | The MPC plugin folder (`bokontep - VST - ANS 1964`) plus install notes and licenses. |
| `ans-1964-1.1.2-linux-vst2.zip` | The Linux desktop VST2 build. |
| `ans-1964-1.1.2-linux-vst3.zip` | The Linux VST3 bundle with the graphical score editor. |

The MPC plugin must be copied into a folder called exactly `VST2 Plugins` at the
root of a USB drive, SD card, or the MPC's internal drive. On MPC OS 3.10.1+, enable
**Preferences → Enhancements (Beta) → Enable Enhancements** and **External Plugin
Support**, then restart the MPC app. The instrument appears under **External
Plugins**.

## Repository layout

- `mpc/` — the Akai MPC / desktop VST2 plugin project.
  - `src/ANS1964.cpp` — MPC plugin shell, parameters, programs, state.
  - `src/ANSDsp.h` — the shared ANS engine.
  - `skins/`, `ui.json` — the MPC scripted UI (SCORE + EDIT, SPEC A/B, CODER, COLOR).
  - `Content/Scores/` — original built-in scores and the user score.
  - `tools/` — desktop score editor, skin generator, PNG → `.ansscore`.
- `vst3/` — the JUCE VST3 with the graphical score editor.
  - `Source/` — processor, editor, parameter layout, score I/O.
  - `Assets/user.ansscore` — the bundled demo score.
  - `yawn-state-sync.patch` — optional YAWN editor/state-sync integration.
- `research/` — the technical report, source list, ANS spec and tuning table.
- `third_party/mpc-sdk/dsp/` — five MIT-licensed DSP headers from the MPC SDK,
  vendored so the VST3 builds without the full SDK.
- `third_party/JUCE/` — JUCE 7.0.12, as a git submodule (GPLv3).

## Build

### One-command build (MPC + desktop VST2 + VST3)

Requirements: Python 3.9+, Git, CMake ≥ 3.22, a C++17 compiler, X11/freetype
development files, `zip`, and network access for the first build (the script fetches
the pinned MPC Plugin SDK and may fetch Zig if it is not on `PATH`). About 2 GB free
disk space.

```bash
git clone https://github.com/tkleisas/ans-1964.git
cd ans-1964
git submodule update --init --recursive
./build.sh
```

`build.sh` checks out the pinned SDK commit, copies `mpc/` into the SDK, builds the
native/ARM MPC plugin and desktop VST2, runs the SDK host-simulator test, builds the
VST3, and writes the release zips to `dist/`.

Useful environment variables:

| Variable | Meaning |
|---|---|
| `SDK_DIR` | Reuse a specific MPC Plugin SDK checkout instead of `.sdk`. |
| `SDK_COMMIT` | Override the pinned SDK commit. |
| `ZIG` | Path to a Zig executable (or a Python interpreter with the `ziglang` package). |
| `PYTHON` | Python executable (default `python3`). |
| `SKIP_VST3` | Set to `1` to build only the MPC/desktop targets. |

### Manual MPC / desktop VST2 build

```bash
git clone https://github.com/inmusic-software/mpc-plugin-builder-sdk.git .sdk
cd .sdk
git checkout febd2ca5bd9eb16c4a412837196e145566bc981b
cp -a ../mpc "plugins/ANS 1964"
python3 tools/build.py "plugins/ANS 1964" --target all
python3 tools/build.py "plugins/ANS 1964" --target desktop
```

The MPC output folder is
`plugins/ANS 1964/build/bokontep - VST - ANS 1964/`. The desktop VST2 is under
`plugins/ANS 1964/build/desktop/`.

The SDK host-simulator validation for v1.1.2 is **19 passed, 0 warnings, 0 failures**
at 44.1 kHz/128 frames and 48 kHz/512 frames.

### Manual VST3 build

```bash
git submodule update --init --recursive
cmake -S vst3 -B vst3/build -DCMAKE_BUILD_TYPE=Release
cmake --build vst3/build -j"$(nproc)"
```

`COPY_PLUGIN_AFTER_BUILD` installs the bundle to `~/.vst3/ANS 1964.vst3`. The build
uses the shared `mpc/src/ANSDsp.h` and the vendored MIT DSP headers in
`third_party/mpc-sdk/`; it does not need a full MPC SDK checkout.

## Install and use

### MPC

Copy the unzipped `bokontep - VST - ANS 1964/` folder into `VST2 Plugins/`, enable
External Plugin Support, and insert it on an instrument track. The plugin has no
window on the MPC. Its interface is six tabs: **SCORE** (with an **EDIT**
sub-tab — press the SCORE function key twice), **SPEC A**, **SPEC B**, **CODER** and
**COLOR**, plus the Q-Link knobs. See [`mpc/README.md`](mpc/README.md) for every
control and the `.ansscore` format.

### Desktop VST2

Copy the `ANS 1964` folder from the desktop build into `~/.vst` (Linux) or the host's
VST2 folder. It has no custom editor; the host shows the automatable parameters.
MIDI notes play the 16-harmonic coder.

### Linux VST3 / YAWN

Copy `ANS 1964.vst3` to `~/.vst3/`. Add **ANS 1964** to an instrument track in
YAWN, then click the **UI** pill on the device header (or right-click the track
header → **Open VST3 Instrument Editor**). Draw a score, select **User Score**, and
press **Run**. `Run` and `Backward` are latching toggles. The editor opens in
YAWN's process-isolated X11 host; the `yawn-state-sync.patch` adds the initial-state
push, the 500 ms state poll, and the UI pill. See [`vst3/README.md`](vst3/README.md)
for GUI details, the zenity file picker, and the patch.

## What it is

This is not a sample player and not a conventional subtractive synth. It reproduces
the ANS signal chain:

- **720 optical sine tracks** on the exact 1/72-octave grid (16.67 cents, 1/6
  semitone), 21 Hz to ~21.4 kHz.
- **Score scanner**: a drawn sonogram where vertical position = pitch, horizontal =
  time, line width = level *and* spectral spread. Wide scratches spill into
  neighbouring tracks and become bands, clusters or noise (“sound spots”).
- **20 photomultiplier channels**, 36 tracks (half an octave) each, with a 6N2P-style
  switchable preamp, the dynamics/modulation manipulator, a two-stage 0.5-octave
  twin-T-style bandpass, and per-channel level presets.
- **16-harmonic coder** with vowel/bright/dark formant templates and a decay control,
  playable from MIDI notes or the test gate.
- Optical imperfections: reader blur/crosstalk, per-disc wow and flutter,
  emulsion/track noise, HF loss, optional 100 Hz lamp hum.

The built-in scores are original studies in the ANS idiom. No score images or audio
recordings from the instrument are included.

## Research

The implementation was built from public documentation, not from copied code or
samples:

- USSR author certificate 118695 (Murzin, 1957): score, optical slit, 16-cutter coder.
- The 2025 Habr article with the reader/channel/manipulator/filter schematics.
- Smirnov's ANS specification page and *Sound in Z*, chapter 7.
- Kreichi/Krivnova/Egorov, Speech Technologies 2013.

The full discussion, including what is measured, inferred, and approximated, is in
[`research/ANS_TECHNICAL_REPORT.md`](research/ANS_TECHNICAL_REPORT.md). Links are in
[`research/SOURCES.md`](research/SOURCES.md). Third-party articles, scans, and
recordings are not redistributed.

## Status

v1.1.2: 57 parameters, 6 tabs (SCORE + EDIT sub-tab), 8 programs, ARM32/ARM64 MPC
builds, on-device XYPad editing with project-state persistence, a desktop score
editor tool, and a Linux VST3 with a full score-editor GUI, YAWN initial-state/state
poll integration, and a zenity-backed Linux file picker.

## Licence

SPDX-License-Identifier: GPL-3.0-or-later. Copyright (C) 2026 bokontep.

The full GPL text is in [`LICENSE`](LICENSE); attribution and third-party notices are
in [`NOTICE.md`](NOTICE.md). The MPC Plugin SDK is MIT-licensed; JUCE and the VST3
SDK are GPLv3.
