#!/usr/bin/env python3
"""ANS 1964 score editor - draw, erase, import and push ANS sonogram scores.

The ANS score is a 720-row sonogram:
    row 0 (top)    = highest pitch (~21.4 kHz)
    row 719        = lowest pitch  (~21 Hz)
    columns        = time (at 24 mm/s, 1 column = 1/px_per_mm mm)
    grey value     = open mastic / level (0 = closed, 255 = fully open)

Usage:
    python3 ans_score_editor.py [score.ansscore]

Left drag draws, right drag erases. The harmonic brush draws a coder-style
harmonic stack. PNG images can be imported and exported. "Push to MPC" saves
Content/Scores/user.ansscore in this plugin's project and runs the SDK's
mpc_remote.py update command.

Requires: Python 3.9+, tkinter, numpy, Pillow.
"""

from __future__ import annotations

import argparse
import math
import queue
import struct
import subprocess
import sys
import threading
import time
from pathlib import Path

import tkinter as tk
from tkinter import filedialog, messagebox, simpledialog, ttk

import numpy as np

try:
    from PIL import Image, ImageTk
except ImportError as exc:  # pragma: no cover
    raise SystemExit("Pillow is required: python3 -m pip install pillow") from exc

H = 720
F0 = 21.09
MAGIC = b"ANS1"
MAX_WIDTH = 16384
PX_PER_MM_DEFAULT = 10
MAX_SPEED = 24.0

# Amber/heat palette, close to the MPC skin.
_STOPS = [
    (0, (8, 8, 10)),
    (36, (42, 14, 10)),
    (90, (120, 42, 12)),
    (150, (210, 105, 24)),
    (210, (255, 185, 70)),
    (255, (255, 248, 220)),
]
LUT = np.zeros((256, 3), np.uint8)
for _i in range(256):
    for _j in range(len(_STOPS) - 1):
        a, ca = _STOPS[_j]
        b, cb = _STOPS[_j + 1]
        if _i <= b:
            t = 0.0 if b == a else (_i - a) / (b - a)
            LUT[_i] = np.array([round(ca[k] + (cb[k] - ca[k]) * t) for k in range(3)], np.uint8)
            break
    else:
        LUT[_i] = _STOPS[-1][1]


def track_to_hz(track: float) -> float:
    return F0 * (2.0 ** (track / 72.0))


def hz_to_track(hz: float) -> float:
    return 72.0 * math.log2(max(1e-9, hz) / F0)


def load_ansscore(path: str | Path) -> tuple[np.ndarray, int]:
    raw = Path(path).read_bytes()
    if len(raw) < 16 or raw[:4] != MAGIC:
        raise ValueError("not an .ansscore file (missing ANS1 header)")
    version, w, h, px_per_mm, size = struct.unpack_from("<HHHHI", raw, 4)
    if version != 1:
        raise ValueError(f"unsupported .ansscore version {version}")
    if h != H:
        raise ValueError(f"expected {H} rows, found {h}")
    if w == 0 or w > MAX_WIDTH:
        raise ValueError(f"invalid width {w}")
    if size != w * h or len(raw) < 16 + size:
        raise ValueError("truncated or inconsistent .ansscore data")
    data = np.frombuffer(raw, np.uint8, count=size, offset=16).reshape(h, w).copy()
    return data, max(1, int(px_per_mm))


def save_ansscore(path: str | Path, data: np.ndarray, px_per_mm: int) -> None:
    h, w = data.shape
    if h != H or w <= 0:
        raise ValueError("bad score shape")
    payload = np.ascontiguousarray(data, dtype=np.uint8).tobytes()
    header = MAGIC + struct.pack("<HHHHI", 1, w, h, max(1, int(px_per_mm)), len(payload))
    Path(path).write_bytes(header + payload)


def _as_uint8(data: np.ndarray) -> np.ndarray:
    return np.ascontiguousarray(np.clip(data, 0, 255).astype(np.uint8))


class ScoreEditor:
    def __init__(self, root: tk.Tk, path: str | None = None) -> None:
        self.root = root
        self.root.title("ANS 1964 Score Editor")
        self.root.geometry("1280x900")
        self.root.minsize(1000, 700)

        self.data = np.zeros((H, 1024), np.uint8)
        self.px_per_mm = PX_PER_MM_DEFAULT
        self.path: Path | None = None
        self.zoom = 1.0
        self.tool = tk.StringVar(value="draw")
        self.level = tk.IntVar(value=220)
        self.brush = tk.IntVar(value=6)
        self.harmonics = tk.IntVar(value=8)
        self.show_grid = tk.BooleanVar(value=True)
        self.playing = False
        self.play_dir = 1
        self.play_pos = 0.0
        self.play_speed = tk.DoubleVar(value=24.0)
        self.loop_mode = tk.StringVar(value="Loop")
        self.auto_scroll = tk.BooleanVar(value=True)

        self.photo: ImageTk.PhotoImage | None = None
        self.image_item: int | None = None
        self.playhead_item: int | None = None
        self.grid_items: list[int] = []
        self.drag: tuple[float, float] | None = None
        self.drag_erase = False
        self.undo_stack: list[np.ndarray] = []
        self.redo_stack: list[np.ndarray] = []
        self._redraw_pending = False
        self._last_play_time = time.monotonic()
        self._status_var = tk.StringVar(value="")
        self._path_var = tk.StringVar(value="(unsaved)")

        self._build_menu()
        self._build_toolbar()
        self._build_canvas()
        self._build_status()
        self._bind_keys()

        if path:
            try:
                self.load(path)
            except Exception as exc:  # noqa: BLE001
                messagebox.showerror("Open failed", str(exc))
        self.redraw_all()
        self.root.after(30, self._tick)

    # ------------------------------------------------------------------ UI
    def _build_menu(self) -> None:
        m = tk.Menu(self.root)
        fm = tk.Menu(m, tearoff=False)
        fm.add_command(label="New...", accelerator="Ctrl+N", command=self.new_score)
        fm.add_command(label="Open...", accelerator="Ctrl+O", command=self.open_score)
        fm.add_command(label="Save", accelerator="Ctrl+S", command=self.save_score)
        fm.add_command(label="Save As...", command=self.save_score_as)
        fm.add_separator()
        fm.add_command(label="Import PNG...", command=self.import_png)
        fm.add_command(label="Export PNG...", command=self.export_png)
        fm.add_separator()
        fm.add_command(label="Push to MPC...", command=self.push_dialog)
        fm.add_separator()
        fm.add_command(label="Quit", command=self.root.destroy)
        m.add_cascade(label="File", menu=fm)

        em = tk.Menu(m, tearoff=False)
        em.add_command(label="Undo", accelerator="Ctrl+Z", command=self.undo)
        em.add_command(label="Redo", accelerator="Ctrl+Y", command=self.redo)
        em.add_separator()
        em.add_command(label="Clear score", command=self.clear_score)
        em.add_command(label="Flip vertically", command=self.flip_vertical)
        em.add_command(label="Invert", command=self.invert_score)
        m.add_cascade(label="Edit", menu=em)

        sm = tk.Menu(m, tearoff=False)
        sm.add_command(label="Play / Pause", accelerator="Space", command=self.toggle_play)
        sm.add_command(label="Scan forward", command=lambda: self.set_direction(1))
        sm.add_command(label="Scan backward", command=lambda: self.set_direction(-1))
        m.add_cascade(label="Scan", menu=sm)

        hm = tk.Menu(m, tearoff=False)
        hm.add_command(label="Controls / workflow", command=self.show_help)
        hm.add_command(label="About", command=self.show_about)
        m.add_cascade(label="Help", menu=hm)
        self.root.config(menu=m)

    def _build_toolbar(self) -> None:
        top = ttk.Frame(self.root, padding=(8, 6))
        top.pack(side="top", fill="x")

        ttk.Button(top, text="New", command=self.new_score).pack(side="left", padx=2)
        ttk.Button(top, text="Open", command=self.open_score).pack(side="left", padx=2)
        ttk.Button(top, text="Save", command=self.save_score).pack(side="left", padx=2)
        ttk.Separator(top, orient="vertical").pack(side="left", fill="y", padx=6)
        ttk.Button(top, text="Import PNG", command=self.import_png).pack(side="left", padx=2)
        ttk.Button(top, text="Export PNG", command=self.export_png).pack(side="left", padx=2)
        ttk.Button(top, text="Push to MPC", command=self.push_dialog).pack(side="left", padx=2)

        row2 = ttk.Frame(self.root, padding=(8, 2))
        row2.pack(side="top", fill="x")

        ttk.Label(row2, text="Tool:").pack(side="left")
        for text, value in (("Draw", "draw"), ("Erase", "erase"), ("Harmonic", "harmonic")):
            ttk.Radiobutton(row2, text=text, value=value, variable=self.tool).pack(side="left", padx=2)
        ttk.Label(row2, text="  Level").pack(side="left")
        ttk.Scale(row2, from_=0, to=255, variable=self.level, length=130).pack(side="left", padx=2)
        ttk.Label(row2, textvariable=self.level, width=4).pack(side="left")
        ttk.Label(row2, text="  Width").pack(side="left")
        ttk.Spinbox(row2, from_=1, to=128, textvariable=self.brush, width=5).pack(side="left", padx=2)
        ttk.Label(row2, text="  Harmonics").pack(side="left")
        ttk.Spinbox(row2, from_=1, to=16, textvariable=self.harmonics, width=5).pack(side="left", padx=2)
        ttk.Button(row2, text="Undo", command=self.undo).pack(side="left", padx=(12, 2))
        ttk.Button(row2, text="Redo", command=self.redo).pack(side="left", padx=2)
        ttk.Checkbutton(row2, text="Grid", variable=self.show_grid, command=self.redraw_grid).pack(side="left", padx=(12, 2))

        row3 = ttk.Frame(self.root, padding=(8, 2))
        row3.pack(side="top", fill="x")
        ttk.Button(row3, text="◀ Back", command=lambda: self.set_direction(-1)).pack(side="left", padx=2)
        ttk.Button(row3, text="Play", command=self.toggle_play).pack(side="left", padx=2)
        ttk.Button(row3, text="Fwd ▶", command=lambda: self.set_direction(1)).pack(side="left", padx=2)
        self.play_button = row3.winfo_children()[1]
        ttk.Label(row3, text=" Speed").pack(side="left")
        ttk.Spinbox(row3, from_=1, to=int(MAX_SPEED), increment=1, textvariable=self.play_speed, width=5).pack(side="left", padx=2)
        ttk.Label(row3, text="mm/s").pack(side="left")
        ttk.Label(row3, text="  Loop").pack(side="left")
        ttk.Combobox(row3, textvariable=self.loop_mode, values=("One-shot", "Loop", "Ping-Pong"),
                     width=10, state="readonly").pack(side="left", padx=2)
        ttk.Checkbutton(row3, text="Auto-scroll", variable=self.auto_scroll).pack(side="left", padx=(10, 2))
        ttk.Button(row3, text="Zoom −", command=lambda: self.set_zoom(self.zoom / 1.25)).pack(side="left", padx=(18, 2))
        ttk.Button(row3, text="Zoom +", command=lambda: self.set_zoom(self.zoom * 1.25)).pack(side="left", padx=2)
        ttk.Button(row3, text="Fit", command=self.fit_zoom).pack(side="left", padx=2)

    def _build_canvas(self) -> None:
        frame = ttk.Frame(self.root)
        frame.pack(side="top", fill="both", expand=True)
        self.canvas = tk.Canvas(frame, bg="#08080a", highlightthickness=0, cursor="crosshair")
        xsb = ttk.Scrollbar(frame, orient="horizontal", command=self.canvas.xview)
        ysb = ttk.Scrollbar(frame, orient="vertical", command=self.canvas.yview)
        self.canvas.configure(xscrollcommand=xsb.set, yscrollcommand=ysb.set)
        self.canvas.grid(row=0, column=0, sticky="nsew")
        ysb.grid(row=0, column=1, sticky="ns")
        xsb.grid(row=1, column=0, sticky="ew")
        frame.rowconfigure(0, weight=1)
        frame.columnconfigure(0, weight=1)

        self.canvas.bind("<ButtonPress-1>", lambda e: self._mouse_down(e, False))
        self.canvas.bind("<B1-Motion>", self._mouse_move)
        self.canvas.bind("<ButtonRelease-1>", self._mouse_up)
        self.canvas.bind("<ButtonPress-3>", lambda e: self._mouse_down(e, True))
        self.canvas.bind("<B3-Motion>", self._mouse_move)
        self.canvas.bind("<ButtonRelease-3>", self._mouse_up)
        self.canvas.bind("<Motion>", self._update_status)
        self.canvas.bind("<Button-4>", lambda e: self.canvas.yview_scroll(-2, "units"))
        self.canvas.bind("<Button-5>", lambda e: self.canvas.yview_scroll(2, "units"))
        self.canvas.bind("<Control-Button-4>", lambda e: self.set_zoom(self.zoom * 1.1))
        self.canvas.bind("<Control-Button-5>", lambda e: self.set_zoom(self.zoom / 1.1))

    def _build_status(self) -> None:
        bar = ttk.Frame(self.root, padding=(8, 3))
        bar.pack(side="bottom", fill="x")
        ttk.Label(bar, textvariable=self._status_var).pack(side="left")
        ttk.Label(bar, textvariable=self._path_var).pack(side="right")

    def _bind_keys(self) -> None:
        self.root.bind("<Control-n>", lambda e: self.new_score())
        self.root.bind("<Control-o>", lambda e: self.open_score())
        self.root.bind("<Control-s>", lambda e: self.save_score())
        self.root.bind("<Control-z>", lambda e: self.undo())
        self.root.bind("<Control-y>", lambda e: self.redo())
        self.root.bind("<space>", lambda e: self.toggle_play())
        self.root.bind("b", lambda e: self.tool.set("draw"))
        self.root.bind("e", lambda e: self.tool.set("erase"))
        self.root.bind("h", lambda e: self.tool.set("harmonic"))
        self.root.bind("[", lambda e: self.brush.set(max(1, self.brush.get() - 1)))
        self.root.bind("]", lambda e: self.brush.set(min(128, self.brush.get() + 1)))

    # --------------------------------------------------------------- drawing
    def set_zoom(self, zoom: float) -> None:
        self.zoom = max(0.25, min(4.0, zoom))
        self.redraw_all()

    def fit_zoom(self) -> None:
        self.root.update_idletasks()
        w = max(100, self.canvas.winfo_width() - 10)
        self.zoom = max(0.25, min(4.0, w / max(1, self.data.shape[1])))
        self.redraw_all()

    def _image(self) -> Image.Image:
        rgb = LUT[self.data]
        img = Image.fromarray(rgb, "RGB")
        if abs(self.zoom - 1.0) > 1e-6:
            resample = Image.NEAREST if self.zoom >= 1.0 else Image.BILINEAR
            img = img.resize((max(1, int(round(self.data.shape[1] * self.zoom))),
                              max(1, int(round(H * self.zoom)))), resample)
        return img

    def redraw_all(self) -> None:
        self._redraw_pending = False
        h, w = self.data.shape
        img = self._image()
        self.photo = ImageTk.PhotoImage(img)
        if self.image_item is None:
            self.image_item = self.canvas.create_image(0, 0, anchor="nw", image=self.photo, tags="score")
        else:
            self.canvas.itemconfig(self.image_item, image=self.photo)
        self.canvas.tag_lower(self.image_item)
        self.canvas.configure(scrollregion=(0, 0, w * self.zoom, H * self.zoom))
        self.redraw_grid()
        self._draw_playhead()
        self._path_var.set(str(self.path) if self.path else "(unsaved)")

    def schedule_redraw(self) -> None:
        if self._redraw_pending:
            return
        self._redraw_pending = True
        self.root.after(15, self.redraw_all)

    def redraw_grid(self) -> None:
        for item in self.grid_items:
            self.canvas.delete(item)
        self.grid_items.clear()
        if not self.show_grid.get():
            return
        z = self.zoom
        w = self.data.shape[1] * z
        h = H * z
        # Horizontal: channel boundaries every 36 tracks, octaves every 72.
        for track in range(0, H, 36):
            row = (H - 1 - track) * z
            is_oct = (track % 72) == 0
            self.grid_items.append(self.canvas.create_line(
                0, row, w, row, fill="#6a5a40" if is_oct else "#2e2a24",
                width=1 if is_oct else 1, tags="grid"))
            if is_oct:
                self.grid_items.append(self.canvas.create_text(
                    5, row - 9, anchor="w", text=f"{track_to_hz(track):.0f} Hz",
                    fill="#b7a98c", font=("TkDefaultFont", 8), tags="grid"))
        # Vertical: one second at 24 mm/s.
        seconds_px = self.px_per_mm * 24.0
        t = 0.0
        while True:
            x = t * seconds_px * z
            if x > w:
                break
            self.grid_items.append(self.canvas.create_line(
                x, 0, x, h, fill="#3a352b", width=1, tags="grid"))
            self.grid_items.append(self.canvas.create_text(
                x + 3, 5, anchor="nw", text=f"{t:g}s", fill="#b7a98c",
                font=("TkDefaultFont", 8), tags="grid"))
            t += 1.0

    def _draw_playhead(self) -> None:
        x = self.play_pos * self.zoom
        h = H * self.zoom
        if self.playhead_item is None:
            self.playhead_item = self.canvas.create_line(x, 0, x, h, fill="#ffffff", width=2, tags="playhead")
        else:
            self.canvas.coords(self.playhead_item, x, 0, x, h)

    # ---------------------------------------------------------------- brush
    def _paint(self, col: int, track: float, *, erase: bool, level: int, width: float) -> bool:
        h, w = self.data.shape
        if col < 0 or col >= w:
            return False
        half = max(0.5, width * 0.5)
        lo, hi = track - half, track + half
        r0 = max(0, int(math.floor(lo)))
        r1 = min(h - 1, int(math.ceil(hi)))
        changed = False
        for t in range(r0, r1 + 1):
            ov = min(t + 0.5, hi) - max(t - 0.5, lo)
            if ov <= 0.0:
                continue
            row = h - 1 - t
            v = int(round(level * min(1.0, ov)))
            if erase:
                if self.data[row, col] != 0:
                    self.data[row, col] = 0
                    changed = True
            else:
                if v > self.data[row, col]:
                    self.data[row, col] = v
                    changed = True
        return changed

    def _paint_harmonics(self, col: int, track: float, *, erase: bool) -> bool:
        changed = False
        n = max(1, min(16, int(self.harmonics.get())))
        base = float(track)
        for h in range(1, n + 1):
            tr = base + round(72.0 * math.log2(h))
            if tr < 0 or tr >= H:
                break
            lev = int(self.level.get() * (h ** -0.5))
            if self._paint(col, tr, erase=erase, level=lev, width=self.brush.get()):
                changed = True
        return changed

    def _stroke(self, col: int, track: float, *, erase: bool) -> None:
        if self.drag is None:
            self.drag = (float(col), float(track))
            if self.tool.get() == "harmonic":
                self._paint_harmonics(col, track, erase=erase)
            else:
                self._paint(col, track, erase=erase, level=self.level.get(), width=float(self.brush.get()))
            self.schedule_redraw()
            return
        c0, t0 = self.drag
        steps = max(1, int(abs(col - c0)))
        for i in range(1, steps + 1):
            c = int(round(c0 + (col - c0) * i / steps))
            t = t0 + (track - t0) * i / steps
            if self.tool.get() == "harmonic":
                self._paint_harmonics(c, t, erase=erase)
            else:
                self._paint(c, t, erase=erase, level=self.level.get(), width=float(self.brush.get()))
        self.drag = (float(col), float(track))
        self.schedule_redraw()

    # ---------------------------------------------------------------- mouse
    def _canvas_pos(self, event: tk.Event) -> tuple[int, float]:
        x = self.canvas.canvasx(event.x) / self.zoom
        y = self.canvas.canvasy(event.y) / self.zoom
        col = int(math.floor(x))
        track = (H - 1) - y
        return max(0, min(self.data.shape[1] - 1, col)), max(0.0, min(H - 1.0, track))

    def _mouse_down(self, event: tk.Event, right: bool) -> None:
        self.undo_stack.append(self.data.copy())
        if len(self.undo_stack) > 30:
            self.undo_stack.pop(0)
        self.redo_stack.clear()
        self.drag = None
        self.drag_erase = right or self.tool.get() == "erase"
        col, track = self._canvas_pos(event)
        self._stroke(col, track, erase=self.drag_erase)

    def _mouse_move(self, event: tk.Event) -> None:
        if self.drag is None:
            self._update_status(event)
            return
        col, track = self._canvas_pos(event)
        self._stroke(col, track, erase=self.drag_erase)
        self._update_status(event)

    def _mouse_up(self, _event: tk.Event) -> None:
        self.drag = None
        self.redraw_all()

    def _update_status(self, event: tk.Event) -> None:
        col, track = self._canvas_pos(event)
        t = col / (self.px_per_mm * 24.0)
        self._status_var.set(
            f"Time {t:.2f}s / {self.duration():.2f}s   ·   Track {track:.0f}  ({track_to_hz(track):.1f} Hz)"
            f"   ·   Level {self.level.get()}  Width {self.brush.get()}")

    def duration(self) -> float:
        return self.data.shape[1] / (max(1, self.px_per_mm) * 24.0)

    # ---------------------------------------------------------------- play
    def set_direction(self, direction: int) -> None:
        self.play_dir = 1 if direction >= 0 else -1
        self._last_play_time = time.monotonic()

    def toggle_play(self) -> None:
        self.playing = not self.playing
        self._last_play_time = time.monotonic()
        self.play_button.configure(text="Pause" if self.playing else "Play")

    def _tick(self) -> None:
        now = time.monotonic()
        dt = min(0.25, now - self._last_play_time)
        self._last_play_time = now
        if self.playing:
            w = self.data.shape[1]
            self.play_pos += self.play_dir * self.play_speed.get() * self.px_per_mm * dt
            if self.play_pos >= w:
                if self.loop_mode.get() == "One-shot":
                    self.play_pos = float(w - 1)
                    self.playing = False
                    self.play_button.configure(text="Play")
                elif self.loop_mode.get() == "Ping-Pong":
                    self.play_pos = max(0.0, 2.0 * (w - 1) - self.play_pos)
                    self.play_dir = -1
                else:
                    self.play_pos %= max(1, w)
            elif self.play_pos < 0:
                if self.loop_mode.get() == "One-shot":
                    self.play_pos = 0.0
                    self.playing = False
                    self.play_button.configure(text="Play")
                elif self.loop_mode.get() == "Ping-Pong":
                    self.play_pos = -self.play_pos
                    self.play_dir = 1
                else:
                    self.play_pos = (self.play_pos % max(1, w)) + max(1, w)
            self._draw_playhead()
            if self.auto_scroll.get():
                x = self.play_pos * self.zoom
                view_x = self.canvas.canvasx(0)
                view_w = self.canvas.winfo_width()
                if x < view_x or x > view_x + view_w:
                    total = max(1.0, self.data.shape[1] * self.zoom)
                    self.canvas.xview_moveto(max(0.0, min(1.0, (x - view_w * 0.2) / total)))
        self.root.after(30, self._tick)

    # ---------------------------------------------------------------- files
    def new_score(self) -> None:
        width = simpledialog.askinteger("New score", "Width in pixels (10 px = 1 mm):",
                                        initialvalue=1024, minvalue=64, maxvalue=MAX_WIDTH)
        if width is None:
            return
        self.data = np.zeros((H, width), np.uint8)
        self.path = None
        self.undo_stack.clear()
        self.redo_stack.clear()
        self.play_pos = 0.0
        self.redraw_all()

    def open_score(self) -> None:
        name = filedialog.askopenfilename(title="Open ANS score",
                                          filetypes=[("ANS score", "*.ansscore"), ("All files", "*")])
        if not name:
            return
        self.load(name)

    def load(self, name: str) -> None:
        data, px = load_ansscore(name)
        self.data = data
        self.px_per_mm = px
        self.path = Path(name)
        self.undo_stack.clear()
        self.redo_stack.clear()
        self.play_pos = 0.0
        self.redraw_all()

    def save_score(self) -> None:
        if self.path is None:
            self.save_score_as()
            return
        try:
            save_ansscore(self.path, self.data, self.px_per_mm)
            self._path_var.set(str(self.path))
        except Exception as exc:  # noqa: BLE001
            messagebox.showerror("Save failed", str(exc))

    def save_score_as(self) -> None:
        name = filedialog.asksaveasfilename(title="Save ANS score", defaultextension=".ansscore",
                                            filetypes=[("ANS score", "*.ansscore")])
        if not name:
            return
        self.path = Path(name)
        self.save_score()

    def clear_score(self) -> None:
        if not messagebox.askyesno("Clear score", "Erase the whole score?"):
            return
        self.undo_stack.append(self.data.copy())
        self.data = np.zeros_like(self.data)
        self.redraw_all()

    def flip_vertical(self) -> None:
        self.undo_stack.append(self.data.copy())
        self.data = self.data[::-1].copy()
        self.redraw_all()

    def invert_score(self) -> None:
        self.undo_stack.append(self.data.copy())
        self.data = (255 - self.data).astype(np.uint8)
        self.redraw_all()

    def import_png(self) -> None:
        name = filedialog.askopenfilename(title="Import image as score",
                                          filetypes=[("Images", "*.png *.jpg *.jpeg *.bmp *.tif *.tiff"),
                                                     ("All files", "*")])
        if not name:
            return
        try:
            im = Image.open(name).convert("L")
        except Exception as exc:  # noqa: BLE001
            messagebox.showerror("Import failed", str(exc))
            return
        w = max(1, round(im.width * H / max(1, im.height)))
        if w > MAX_WIDTH:
            w = MAX_WIDTH
        resample = getattr(Image, "Resampling", Image).LANCZOS
        im = im.resize((w, H), resample)
        self.undo_stack.append(self.data.copy())
        self.data = np.array(im, dtype=np.uint8).copy()
        px = simpledialog.askinteger("Scale", "Pixels per mm:",
                                     initialvalue=PX_PER_MM_DEFAULT, minvalue=1, maxvalue=100)
        if px:
            self.px_per_mm = px
        self.redraw_all()

    def export_png(self) -> None:
        name = filedialog.asksaveasfilename(title="Export score as image", defaultextension=".png",
                                            filetypes=[("PNG", "*.png")])
        if not name:
            return
        try:
            Image.fromarray(self.data, "L").save(name)
        except Exception as exc:  # noqa: BLE001
            messagebox.showerror("Export failed", str(exc))

    def undo(self) -> None:
        if not self.undo_stack:
            return
        self.redo_stack.append(self.data.copy())
        self.data = self.undo_stack.pop()
        self.redraw_all()

    def redo(self) -> None:
        if not self.redo_stack:
            return
        self.undo_stack.append(self.data.copy())
        self.data = self.redo_stack.pop()
        self.redraw_all()

    # ---------------------------------------------------------------- push
    def push_dialog(self) -> None:
        project = Path(__file__).resolve().parent.parent
        dlg = tk.Toplevel(self.root)
        dlg.title("Push score to MPC")
        dlg.transient(self.root)
        dlg.grab_set()
        dlg.geometry("720x480")

        frm = ttk.Frame(dlg, padding=10)
        frm.pack(fill="both", expand=True)

        ip = tk.StringVar(value="192.168.1.75")
        drive = tk.StringVar(value="4")
        proj = tk.StringVar(value=str(project))
        restart = tk.BooleanVar(value=False)
        log = tk.Text(dlg, height=14, width=88, background="#101014", foreground="#d8d3c5",
                      insertbackground="#ffffff", font=("TkFixedFont", 9))
        log.pack(side="bottom", fill="both", expand=True, padx=10, pady=(0, 10))
        log.insert("end", "Saves Content/Scores/user.ansscore, then runs mpc_remote.py update.\n"
                          "The plugin on the MPC must be re-added or the MPC restarted to reload the score.\n")

        def add_row(label: str, var: tk.StringVar, browse: bool = False) -> None:
            row = ttk.Frame(frm)
            row.pack(fill="x", pady=3)
            ttk.Label(row, text=label, width=16).pack(side="left")
            ttk.Entry(row, textvariable=var).pack(side="left", fill="x", expand=True)
            if browse:
                ttk.Button(row, text="...", width=3,
                           command=lambda: var.set(filedialog.askdirectory(initialdir=var.get()) or var.get())
                           ).pack(side="left", padx=4)

        add_row("MPC address", ip)
        add_row("Drive number", drive)
        add_row("Plugin project", proj, browse=True)
        ttk.Checkbutton(frm, text="Restart MPC after install (loses unsaved data)",
                        variable=restart).pack(anchor="w", pady=(8, 2))

        def do_push() -> None:
            p = Path(proj.get()).expanduser()
            score = p / "Content" / "Scores" / "user.ansscore"
            sdk = p.parent.parent
            tool = sdk / "tools" / "mpc_remote.py"
            if not (p / "plugin.json").exists():
                messagebox.showerror("Push failed", f"Not a plugin project: {p}", parent=dlg)
                return
            if not tool.exists():
                messagebox.showerror("Push failed", f"mpc_remote.py not found: {tool}", parent=dlg)
                return
            try:
                score.parent.mkdir(parents=True, exist_ok=True)
                save_ansscore(score, self.data, self.px_per_mm)
            except Exception as exc:  # noqa: BLE001
                messagebox.showerror("Save failed", str(exc), parent=dlg)
                return
            log.insert("end", f"\nSaved {score}\nRunning update on {ip.get()} drive {drive.get()}...\n")
            log.see("end")
            cmd = [sys.executable, str(tool), ip.get().strip(), "--drive", drive.get().strip(),
                   "--yes", "--no-color", "update", str(p)]
            self.root.config(cursor="watch")
            dlg.update_idletasks()
            try:
                r = subprocess.run(cmd, text=True, capture_output=True, timeout=240)
                out = (r.stdout or "") + (r.stderr or "")
                log.insert("end", out + f"\n[exit {r.returncode}]\n")
                log.see("end")
                if r.returncode != 0:
                    messagebox.showerror("Push failed", "mpc_remote.py returned an error. See the log.", parent=dlg)
                    return
            except Exception as exc:  # noqa: BLE001
                messagebox.showerror("Push failed", str(exc), parent=dlg)
                return
            finally:
                self.root.config(cursor="")
            log.insert("end", "Done. Re-add the plugin or restart the MPC to hear the new score.\n")
            if restart.get():
                if messagebox.askyesno("Restart MPC?",
                                       "Restart the MPC app now? Unsaved data on the MPC will be lost.",
                                       parent=dlg):
                    cmd2 = [sys.executable, str(tool), ip.get().strip(), "--yes", "--no-color", "restart"]
                    try:
                        r2 = subprocess.run(cmd2, text=True, capture_output=True, timeout=120)
                        log.insert("end", (r2.stdout or "") + (r2.stderr or "") + f"\n[exit {r2.returncode}]\n")
                    except Exception as exc:  # noqa: BLE001
                        log.insert("end", f"Restart failed: {exc}\n")
                    log.see("end")

        buttons = ttk.Frame(frm)
        buttons.pack(fill="x", pady=(8, 0))
        ttk.Button(buttons, text="Push score", command=do_push).pack(side="left")
        ttk.Button(buttons, text="Close", command=dlg.destroy).pack(side="left", padx=6)

    # ---------------------------------------------------------------- help
    def show_help(self) -> None:
        messagebox.showinfo(
            "ANS 1964 score editor",
            "Draw a sonogram like the ANS mastic score.\n\n"
            "• Left drag = draw, right drag = erase.\n"
            "• Harmonic tool = coder-style harmonic stack at the cursor.\n"
            "• Level controls how open the mastic is; Width is the vertical brush size in tracks.\n"
            "• One row = one 1/72-octave tone. Top = highest pitch.\n"
            "• Time: 10 px = 1 mm; at 24 mm/s, 240 px = 1 second.\n\n"
            "• Back / Play / Fwd preview the scan. Loop = One-shot, Loop or Ping-Pong.\n"
            "• Save .ansscore, or Push to MPC to install it as Content/Scores/user.ansscore.\n"
            "• After pushing, re-add the plugin or restart the MPC to reload the user score.\n\n"
            "Shortcuts: Space play, B/E/H tools, [ ] brush width, Ctrl+Z/Y undo/redo, Ctrl+S save.",
            parent=self.root)

    def show_about(self) -> None:
        messagebox.showinfo("About", "ANS 1964 score editor\n\n"
                                     "Part of the bokontep ANS 1964 plugin.\n"
                                     "Score format: ANS1, 720 rows, 8-bit grayscale.",
                            parent=self.root)


def main() -> int:
    ap = argparse.ArgumentParser(description="Draw/edit ANS .ansscore sonograms")
    ap.add_argument("score", nargs="?", help="optional .ansscore file to open")
    args = ap.parse_args()
    root = tk.Tk()
    try:
        root.tk.call("tk", "scaling", 1.15)
    except tk.TclError:
        pass
    ScoreEditor(root, args.score)
    root.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
