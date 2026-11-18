#!/usr/bin/env python3
"""Generate the ANS 1964 MPC skin, including the hand-written EDIT/XYPad page.

Run after a native build (which writes build/params.json), from anywhere:

    python3 "plugins/ANS 1964/tools/make_skins.py"

The script runs the SDK's gen_ui.py from ui.json, then injects an EDIT sub-tab
under the SCORE function key, adds the XYPad/puck images, and updates Q-Links.
The resulting skins/ folder is copied verbatim by tools/build.py.
"""

from __future__ import annotations

import json
import shutil
import subprocess
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

PLUGIN = Path(__file__).resolve().parent.parent
SDK = PLUGIN.parent.parent
PARAMS = PLUGIN / "build" / "params.json"
TMP = PLUGIN / "build" / "ui-base"
SKINS = PLUGIN / "skins"

PAD_W, PAD_H = 600, 540
PAD_X, PAD_Y = 23, 45

# Amber palette for the static user-score preview.
STOPS = [(0, (8, 8, 10)), (36, (42, 14, 10)), (90, (120, 42, 12)),
         (150, (210, 105, 24)), (210, (255, 185, 70)), (255, (255, 248, 220))]
LUT = np.zeros((256, 3), np.uint8)
for i in range(256):
    for j in range(len(STOPS) - 1):
        a, ca = STOPS[j]
        b, cb = STOPS[j + 1]
        if i <= b:
            t = 0.0 if b == a else (i - a) / (b - a)
            LUT[i] = [round(ca[k] + (cb[k] - ca[k]) * t) for k in range(3)]
            break
    else:
        LUT[i] = STOPS[-1][1]


def load_params() -> dict[str, int]:
    data = json.loads(PARAMS.read_text())
    return {p["name"]: int(p["index"]) for p in data["params"]}


def load_user_score() -> np.ndarray | None:
    path = PLUGIN / "Content" / "Scores" / "user.ansscore"
    if not path.exists():
        return None
    raw = path.read_bytes()
    if len(raw) < 16 or raw[:4] != b"ANS1":
        return None
    import struct
    ver, w, h, px, size = struct.unpack_from("<HHHHI", raw, 4)
    if ver != 1 or h != 720 or size != w * h or len(raw) < 16 + size:
        return None
    return np.frombuffer(raw, np.uint8, count=size, offset=16).reshape(h, w).copy()


def font(size: int) -> ImageFont.ImageFont:
    for name in ("DejaVuSans.ttf", "LiberationSans-Regular.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except Exception:  # noqa: BLE001
            pass
    return ImageFont.load_default()


def make_pad_images() -> None:
    # Puck: amber ring + bright centre, transparent background.
    puck = Image.new("RGBA", (48, 48), (0, 0, 0, 0))
    d = ImageDraw.Draw(puck)
    d.ellipse((3, 3, 44, 44), fill=(255, 190, 80, 210), outline=(255, 245, 220, 255), width=3)
    d.ellipse((19, 19, 28, 28), fill=(20, 14, 8, 255))
    puck.save(SKINS / "puck.png")

    # Pad background: current user score as a faint static preview + grid/axes.
    bg = Image.new("RGB", (PAD_W, PAD_H), (16, 13, 10))
    score = load_user_score()
    if score is not None:
        small = Image.fromarray(score, "L").resize((PAD_W, PAD_H), Image.LANCZOS)
        rgb = LUT[np.array(small)]
        preview = Image.fromarray(rgb, "RGB")
        bg = Image.blend(bg, preview, 0.82)
    d = ImageDraw.Draw(bg)
    f_small = font(13)
    f_title = font(16)

    # 20 half-octave channels (27 px each), octave lines every 54 px.
    for track in range(0, 721, 36):
        row = int(round((540 - (track / 720.0) * (PAD_H - 20)) - 10))
        if row < 0 or row >= PAD_H:
            continue
        col = (70, 55, 35) if track % 72 == 0 else (42, 34, 26)
        d.line((0, row, PAD_W, row), fill=col, width=1)
    for i in range(9):
        x = int(round(i * (PAD_W - 1) / 8))
        d.line((x, 0, x, PAD_H), fill=(52, 43, 31), width=1)
        if i in (0, 2, 4, 6, 8):
            label = f"{i * 12.5:.0f}%"
            d.text((x + 3, PAD_H - 18), label, fill=(190, 175, 145), font=f_small)

    for track, label in ((0, "21"), (144, "84"), (288, "337"), (432, "1350"),
                         (576, "5400"), (719, "21k")):
        row = int(round((540 - (track / 720.0) * (PAD_H - 20)) - 10))
        d.text((5, max(2, min(PAD_H - 16, row - 7))), label, fill=(210, 190, 155), font=f_small)

    d.rectangle((0, 0, PAD_W - 1, PAD_H - 1), outline=(120, 95, 55), width=2)
    d.text((10, 8), "HIGH PITCH", fill=(255, 220, 160), font=f_title)
    d.text((10, PAD_H - 38), "LOW", fill=(200, 180, 150), font=f_small)
    d.text((PAD_W - 90, 8), "TIME →", fill=(200, 180, 150), font=f_small)
    d.text((PAD_W - 190, PAD_H - 38), "USER SCORE", fill=(200, 180, 150), font=f_small)
    bg.save(SKINS / "xy_pad_bg.png")


def slot(name: str, ctype: str, param: int, bounds: str) -> dict:
    return {
        "version": 2,
        "componentData": {"version": 1, "name": name, "type": ctype,
                          "data": {"version": 1, "handleName": "Data"}},
        "handle remapping": {"version": 1, "map": [{"key": "Data", "value": f"Parameter {param}"}]},
        "bounds": {"version": 1, "acceptsHWFocus": "Yes", "showWhenDataModelInvalid": "Hide",
                   "whenVisible": "Always", "boundsType": "Absolute", "bounds": bounds},
    }


def raw(name: str, ctype: str, data: dict, bounds: str, focusable: bool = False) -> dict:
    return {
        "version": 2,
        "componentData": {"version": 1, "name": name, "type": ctype, "data": data},
        "handle remapping": {"version": 1, "map": []},
        "bounds": {"version": 1, "acceptsHWFocus": "Yes" if focusable else "No",
                   "showWhenDataModelInvalid": "Show", "whenVisible": "Always",
                   "boundsType": "Absolute", "bounds": bounds},
    }


def inject_edit_tab(tui: dict, qlinks: dict, idx: dict[str, int]) -> None:
    tabs = tui["pageData"]["tabs"]
    score_tab = next(t for t in tabs if t.get("tabName") == "SCORE")
    root = json.loads(json.dumps(score_tab["componentDefinition"]))  # deep copy

    pad_data = {
        "version": 1,
        "xPositionHandle": f"Parameter {idx['Edit X']}",
        "yPositionHandle": f"Parameter {idx['Edit Y']}",
        "image": "puck.png",
    }
    bg_data = {"version": 2, "imageType": "Regular", "colour": "0", "image": "xy_pad_bg.png"}

    comps = [
        raw("PadBG", "Image", bg_data, f"{PAD_X} {PAD_Y} {PAD_W} {PAD_H}", focusable=False),
        raw("XY", "XYPad", pad_data, f"{PAD_X} {PAD_Y} {PAD_W} {PAD_H}", focusable=True),
        slot("Draw", "toggle", idx["Draw"], "663 45 278 107"),
        slot("Erase", "toggle", idx["Erase"], "983 45 278 107"),
        slot("Brush Level", "faderUni", idx["Brush Lvl"], "663 196 278 107"),
        slot("Brush Width", "knobUni", idx["Brush Wd"], "983 196 278 107"),
        slot("Edit Harmonics", "knobUni", idx["Edit Harm"], "663 346 278 107"),
        slot("Clear", "toggle", idx["Clear"], "983 346 278 107"),
        slot("Audition", "toggle", idx["Audition"], "663 497 278 107"),
        slot("Flip Y", "toggle", idx["Flip Y"], "983 497 278 107"),
    ]
    root["componentsData"] = comps

    tab = {"version": 1, "tabName": "EDIT", "fnKeyIndex": 0, "fnKeySubIndex": 1,
           "componentDefinition": root, "qlinkBoundsData": []}
    # Put the sub-tab right after SCORE.
    pos = tabs.index(score_tab) + 1
    tabs.insert(pos, tab)

    # Off/On labels for the new toggles.
    rs = tui["pageData"].get("replacementStrings", [])
    existing = {entry.get("key") for entry in rs if isinstance(entry, dict)}
    for name in ("Draw", "Erase", "Clear", "Audition", "Flip Y"):
        key = f"Parameter {idx[name]}"
        if key not in existing:
            rs.append({"key": key, "value": ["Off", "On"]})
    tui["pageData"]["replacementStrings"] = rs

    order = ["Edit X", "Edit Y", "Brush Lvl", "Brush Wd", "Edit Harm",
             "Draw", "Erase", "Clear", "Audition", "Flip Y"]
    qmap = {f"Q-Link {i + 1}": idx[name] for i, name in enumerate(order)}
    for i in range(len(order) + 1, 17):
        qmap[f"Q-Link {i}"] = -1
    qlinks["Screen Mode Q-Links"]["map"] = [
        m for m in qlinks["Screen Mode Q-Links"]["map"]
        if not (int(m.get("Tab", 0)) == 1 and int(m.get("SubTab", 0)) == 2)
    ]
    qlinks["Screen Mode Q-Links"]["map"].append(
        {"Tab": 1, "SubTab": 2, "Bank Direction": "Column", "Q-Links": qmap})
    qlinks["Screen Mode Q-Links"]["map"].sort(
        key=lambda m: (int(m.get("Tab", 0)), int(m.get("SubTab", 0))))


def main() -> int:
    if not PARAMS.exists():
        print(f"missing {PARAMS}; run a native build first", file=sys.stderr)
        return 1
    if TMP.exists():
        shutil.rmtree(TMP)
    SKINS.mkdir(parents=True, exist_ok=True)
    for old in SKINS.iterdir():
        if old.is_file():
            old.unlink()

    cmd = [sys.executable, str(SDK / "tools" / "gen_ui.py"),
           "--params", str(PARAMS), "--ui", str(PLUGIN / "ui.json"),
           "--out", str(TMP), "--kit-cache", str(PLUGIN / "build" / "kit-cache"), "--quiet"]
    subprocess.run(cmd, check=True)

    for f in TMP.iterdir():
        if f.is_file():
            shutil.copy2(f, SKINS / f.name)

    idx = load_params()
    tui = json.loads((SKINS / "TUI.json").read_text())
    qlinks = json.loads((SKINS / "Q-Links.json").read_text())
    inject_edit_tab(tui, qlinks, idx)
    (SKINS / "TUI.json").write_text(json.dumps(tui, separators=(",", ":")))
    (SKINS / "Q-Links.json").write_text(json.dumps(qlinks, separators=(",", ":")))
    make_pad_images()

    print(f"wrote {SKINS} with EDIT/XYPad tab "
          f"(params Edit X={idx['Edit X']}, Edit Y={idx['Edit Y']})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
