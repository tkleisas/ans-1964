# ANS 1964 — Linux VST3 with graphical score editor

A VST3 instrument for Linux that shares the exact ANS DSP engine with the MPC plugin.
It adds the thing the MPC scripted UI cannot do: a real score canvas with live draw /
erase / harmonic brush, scan preview, zoom, file I/O and parameter pages.

The built bundle is installed by CMake to:

```
~/.vst3/ANS 1964.vst3
```

YAWN scans `~/.vst3`; a normal VST3 host (REAPER, Bitwig, etc.) shows the same editor
in its plugin window.

---

## Build

Requirements: CMake ≥ 3.22, a C++17 compiler, X11/freetype development files, and
JUCE 7.0.12. The JUCE source is the `third_party/JUCE` submodule (GPLv3):

```bash
git submodule update --init --recursive
```

The shared DSP only needs the five header-only files in
`third_party/mpc-sdk/dsp/` (MIT, from the inMusic MPC Plugin SDK); they are already in
this repository, so the VST3 build does not require a full MPC SDK checkout.

Build from the repository root:

```bash
cmake -S vst3 -B vst3/build -DCMAKE_BUILD_TYPE=Release
cmake --build vst3/build -j"$(nproc)"
```

`COPY_PLUGIN_AFTER_BUILD` installs the VST3 into `~/.vst3/ANS 1964.vst3`; the bundle is
also in `vst3/build/ANS1964_artefacts/Release/VST3/ANS 1964.vst3`. The top-level
`build.sh` copies release assets to `dist/`.

The plugin is built with JUCE 7.0.12 and the VST3 SDK bundled with JUCE; both are used
under GPLv3. The plugin itself is GPL-3.0-or-later.

---

## Use in YAWN

The yawn checkout has been patched in three files:

- `src/vst3/VST3EditorWindow.h/.cpp` — sends the main instance's processor/controller
  state to the editor process when it reports READY.
- `src/vst3/VST3EditorHost.cpp` — applies `STATE`/`CSTATE` to the editor instance and
  polls its state every 500 ms, sending `STATE` back when the GUI changed anything.
- `src/ui/framework/v2/DeviceHeaderWidget.h`, `DeviceWidget.h`,
  `src/ui/panels/DetailPanelWidget.h`, `src/app/App_Lifecycle.cpp` — adds a visible
  **UI** pill to the header of every track-level VST3 device; clicking it opens the
  same `openVST3Editor` path as the track-header context menu.

A ready-to-apply diff is in `yawn-state-sync.patch`. Both `yawn_vst3_host` and `YAWN`
have been rebuilt with it.

1. Launch your rebuilt YAWN (with `yawn-state-sync.patch` applied).
2. Add **ANS 1964** to an instrument track (it is listed from `~/.vst3`).
3. Click the **UI** pill on the ANS 1964 device header (or right-click the track
   header → **Open VST3 Instrument Editor**). The editor opens in yawn's
   process-isolated X11 host as a separate window.
4. Draw / change controls. The audio instance receives the score and parameter state
   within about 0.5 s (parameter changes also use the normal `PARAM` path).
5. Save the yawn project to store the VST3 state, including the drawn score.

If the yawn editor window does not open, check that the rebuilt `yawn_vst3_host` sits
next to the `YAWN` binary and that `~/.vst3/ANS 1964.vst3` exists. If you update yawn
later, reapply `yawn-state-sync.patch` (or merge the three changes).

---

## GUI

**Score canvas**
- Time runs left → right; pitch runs low (bottom) → high (top).
- One row = one 1/72-octave track; horizontal lines mark channel boundaries, brighter
  lines mark octaves.
- Left drag draws, right drag erases. The **Harmonic** tool draws a coder-style harmonic
  stack at the cursor.
- `Brush Lvl`, `Brush Wd`, `Edit Harm` control the brush.
- The bundled demo score is loaded on first use; the canvas edits the `User Score`
  buffer. Select **User Score** in the Score combo to hear the canvas.

**Toolbar**
- `Open` / `Save` — `.ansscore` files.
- `Import PNG` / `Export PNG` — image ↔ score.
- `New` — blank 1024-column score.
- `Undo` / `Redo`, `Zoom -` / `Zoom +`, `Fit`.

**EDIT tab**
- Score selection, `Use User Score`, `Run`, `Backward`, `Loop`
  (One-shot/Loop/Ping-Pong), `Speed`, `Scan Pos`, `Score Level`, `Volume`.
- Tool selection and brush controls.

**PARAMS tab**
- All 20 channel levels, coder, optics, dynamics, output and the on-device edit
  parameters, grouped in a scrollable page.

**File dialogs** use `zenity` on Linux (GTK dialog independent of the plugin's event
loop). Install it if needed: `sudo apt install zenity`. On non-Linux platforms the
normal JUCE file chooser is used.

---

## State and sound

- The score is stored in the VST3 state as a PNG-compressed 720-row image, alongside
  the plugin parameters. Saving the DAW project saves the score.
- `Open`/`Save` use the same `.ansscore` raw format as the MPC plugin
  (`Content/Scores/README.txt` in the MPC project describes it).
- MIDI notes play the 16-harmonic coder, exactly as on the MPC.
- `Run` starts the score scanner; `Backward` and `Ping-Pong` are available. `Run` and
  `Backward` are latching toggles.
- The VST3 score and the MPC user score are separate: use `Save` here and the MPC
  `Content/Scores`/desktop editor workflow there if you want to share a score.

The score engine and tube/channel DSP are the same `ANSDsp.h` used by the MPC plugin
in `mpc/src/`. The MPC plugin remains the standalone-hardware target.

---

## Licence

SPDX-License-Identifier: GPL-3.0-or-later. Copyright (C) 2026 bokontep.

- JUCE 7.0.12 — GPLv3 (see `../third_party/JUCE/LICENSE.md`).
- VST3 SDK bundled with JUCE — GPLv3 / Steinberg dual licence.
- “VST” is a trademark of Steinberg Media Technologies GmbH. This project is not
  affiliated with Steinberg or inMusic.
