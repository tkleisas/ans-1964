# ANS Synthesizer — Technical Research Dossier

Status: v1.0 implemented (MPC OS 3.10.1 VST2 + desktop VST2); this dossier is the research basis.
Target: an accurate ANS emulation that runs as an Akai MPC OS 3.10.1 VST2 plugin and as a normal desktop VST2.
Instrument emulated: the **1964 second version**, 720 tones, 5 optical discs (the only surviving instrument).

---

## 1. Short description

The ANS is not a conventional oscillator synth. It is a **photoelectronic additive synthesizer**:

1. Five glass discs carry 144 concentric **variable-area optical sine tracks** each (720 total).
   The discs are identical prints, spun at five different speeds (ratio 4:1 between discs).
2. A projector per disc images its 144 tracks as a vertical strip of light; the five strips are
   stacked into one 720-tone vertical spectrum, spanning 10 octaves at 1/72-octave spacing
   (16.67 cents, 1/6 semitone).
3. The composer's **score** is a 7 mm glass plate covered with opaque non-drying mastic. Scratching
   the mastic opens the light path. The vertical position = pitch; horizontal position = time;
   the width/opacity of the scratch = level, and wider scratches spill into neighbouring tracks,
   turning a pure tone into a band/cluster ("sound spot") or noise.
4. The score moves across a fixed optical slit at up to 24 mm/s. Behind it sit **20 flat-lens
   readers**, each covering a 36 mm strip (36 tracks = 1/2 octave), each feeding a **FEU-2
   photomultiplier**.
5. Each of the **20 channels** is a tube chain: photocell preamp → switchable 6N2P preamp
   (×10 / bypass) → manipulator (open / dynamics-expander / modulation) → two-stage 0.5-octave
   twin-T bandpass filter → channel level preset → output line 1 or 2.
6. The composer can also use the **coder**: a carriage with 16 blade cutters that scratch a
   harmonic series (fundamental + overtones) into the score, with a formant template setting
   overtone levels and a decay ruler scaling them together.

The result is somewhere between a spectral synthesizer, a drawn-sound instrument and a
20-band formant vocoder. Its identity is the **combination** of exact 1/72-octave additive
partials, the drawn sonogram (including optical spill between tracks), and the fixed 20-channel
tube/filter bank.

---

## 2. Primary sources used

| # | Source | Private archive reference | What it gives | Reliability |
|---|---|---|---|---|
| 1 | Habr, “Как создавали уникальный советский синтезатор АНС…”, 2025-10-30, https://habr.com/ru/companies/sberbank/articles/961642/ | `habr_ans_article.txt`, `schematics/habr_*.png`, `schematics/named/` | 27 figures: complete signal-chain photos, reader photo, channel-amp block diagram, **preamp schematic, manipulator schematic, filter schematic**, lamp supply, score drive, coder, front panel | High for photos/diagrams; schematics are redrawn, low-res, values partly unreadable |
| 2 | USSR author certificate 118695, E.A. Murzin, applied 1957-06-24, https://patents.google.com/patent/SU118695A1/en and https://findpatent.ru/patent/11/118695.html | `patent_p1.jpg`, `patent_p2.jpg`, `patent_fig12.jpg` | Patent text + Fig.1 (score + optical slit + photocells) and Fig.2 (16-cutter coder with formant template and decay ruler) | Primary, high |
| 3 | A. Smirnov, “Evgeny Murzin and the ANS synthesizer”, https://asmir.info/lib/murzin0.htm (Windows-1251) | cited in web search | **Exact second-version figures**: 720 tones, 21 Hz–21.6 kHz, 10 octaves, 5 discs, speeds 24, 6, 6/4, 6/16, 6/64 rev/s; first version 576 tones, 42–10800 Hz, 4 discs, speeds 12, 3, 3/4, 3/16 rev/s, discs 120–30 mm, track width 0.31 mm | Primary-adjacent (Smirnov/Theremin Center), high |
| 4 | Kreichi, Krivnova, Egorov, “Фотооптический синтезатор АНС…”, Speech Technologies 2013, https://cyberleninka.ru/article/n/fotoopticheskiy-sintezator-ans-i-ego-rol-v-razvitii-metodov-sinteza-rechi | web text | Disc track width 0.3 mm, CD-sized disc, 144 tracks = 2 octaves, 1% frequency step, 5 identical discs with ×4 speeds, score line width 0.1–1.0 mm = 20 dB, 16-cutter coder, computer model details | High; written by ANS custodian S. Kreichi |
| 5 | S. Kreichi, “The ANS Synthesizer: Composing on a Photoelectronic Instrument”, Theremin Center 1997, https://www.theremin.ru/archive/ans.htm | `theremin/` images | Operating principle, score, 20 bandpass amps, 720 tones, 1/72 octave, composer workflow | High |
| 6 | A. Smirnov, *Sound in Z*, chapter 7 “Graphical Sound: The ANS Synthesizer”, pp. 227–236 | `Sound_in_Z.pdf`, `Sound_in_Z.txt` | History, first version 4 discs/576 tones, second 1964 version 5 discs/720 tones, relation to Yankovsky and Sholpo, photos | High (published book) |
| 7 | Yu. Dmitriukova, “У истоков электронной музыки”, Muzelectron 2015, https://muzelectron.ru/pdf/dmitriukova_ans_1.pdf | `dmitriukova_ans_1.pdf` | Museum history; “width affects volume, and as it grows, timbre and a gradual transition from a single tone to a sound spot”; several vertical strips = polyphony | High |
| 8 | Digital Music Academy interview with S. Kreichi, https://digitalmusicacademy.ru/node/576 | web text | One original glass matrix, copies printed; five discs at different speeds; film/emulsion non-uniformity gives constant “life” | Good interview source |
| 9 | Synth DB ANS page, http://synth-db.com/synths/Evgeny%20Murzin/ANS%20Synthesizer/ANS%20Synthesizer.php | web text | Collated technical sheet: 720 VCOs, 20 bandpass, 20 VCA, 20–20,000 Hz at 72 steps, 48/24 dB dynamic range, 1 ms attack, max score speed 24 mm/s, 20 s minimum score, 0.8 mm reading resolution, 22 envelope points/sweep, 5 formant points/octave | Secondary; treat numbers as the museum/technical-sheet values, cross-checked where possible |
| 10 | 120years.net, Monoskop, Wikipedia, ar.culture.ru | web text | Historical context, dates, recordings | Secondary |

The local copies named in the table are the author's private research archive. No
third-party documents, scans, or recordings are redistributed in this repository.

**What is missing:** no official service manual with component values has surfaced. The Habr
schematics are redrawn from museum documents and the filter's twin-T component blocks are drawn as
boxes (“фильтр”) without values. The manipulator schematic is complete topologically but the
component values are not legible at the available resolution. This is the main remaining
uncertainty; see §7.

---

## 3. Confirmed specifications (second version)

### 3.1 Tone bank

| Property | Value |
|---|---|
| Pure tones | 720 |
| Discs | 5, identical optical prints |
| Tracks per disc | 144 |
| Octaves per disc | 2 |
| Total range | 21 Hz – ~21.4–21.6 kHz (10 octaves) |
| Tuning grid | 72 steps/octave = 1/72 octave = 16.67 cents = 1/6 semitone |
| Disc speeds (slowest→fastest) | 0.09375, 0.375, 1.5, 6, 24 rev/s = 5.625, 22.5, 90, 360, 1440 RPM |
| Disc outer/inner diameter | 120 mm / 30 mm |
| Radial track pitch | 0.3125 mm (documents say 0.31 mm) |
| Track width | 0.31 mm (0.3 mm in Kreichi 2013) |
| Spatial wavelength on disc | ~0.423 mm (constant from inner to outer track) |
| Cycles per ring | 225 (inner) to 891.4 (outer) |
| First-version comparison | 576 tones, 4 discs, 42–10800 Hz, 8 octaves, 12/3/0.75/0.1875 rev/s |

The five bands (calc from speed × cycles):

| Disc (slow→fast) | Speed | Frequency band |
|---|---|---|
| 1 | 0.09375 rev/s (5.625 RPM) | 21.1 – 83.6 Hz |
| 2 | 0.375 rev/s (22.5 RPM) | 84.4 – 334.3 Hz |
| 3 | 1.5 rev/s (90 RPM) | 337.5 – 1337 Hz |
| 4 | 6 rev/s (360 RPM) | 1350 – 5348 Hz |
| 5 | 24 rev/s (1440 RPM) | 5400 – 21393 Hz |

Note: 720 grid points span 719 intervals = 9.986 octaves, so the computed top is 21.39 kHz.
The “21.6 kHz” in the sources is the rounded 10-octave boundary. We should generate the grid from
a calibrated f0 (default 21.09 Hz) and expose a master tune/calibration, rather than hard-code
21.6 kHz.

### 3.2 Channel bank

| Property | Value |
|---|---|
| Audio channels | 20 |
| Tracks per channel | 36 (= 1/2 octave) |
| Projected track height | 720 mm total, 20 × 36 mm segments |
| Reader | 20 flat lenses, one per segment |
| Photodetector | FEU-2 photomultiplier per channel |
| Channel filter | Two-stage twin-T RC feedback bandpass, 0.5-octave bandwidth |
| Channel controls | Level preset lever + dB trim; output line 1/2 selector |
| Channel edges (calc) | 21.09, 29.83, 42.18, 59.65, 84.36, 119.30, 168.72, 238.61, 337.44, 477.21, 674.88, 954.42, 1349.76, 1908.85, 2699.52, 3817.70, 5399.04, 7635.40, 10798.08, 15270.79, 21596.16 Hz |
| Channel centers (calc) | 25.08, 35.47, 50.16, 70.94, 100.32, 141.88, 200.64, 283.75, 401.29, 567.50, 802.57, 1135.01, 1605.14, 2270.02, 3210.29, 4540.03, 6420.58, 9080.07, 12841.15, 18160.13 Hz |

The exact 720 frequencies, disc/track/channel assignment are in `ans_tuning.csv`; machine-readable
constants are in `ans_spec.json`.

### 3.3 Score and scanning

| Property | Value |
|---|---|
| Score | 7 mm mirror glass, opaque non-drying mastic (bitumen + toner + oil) |
| Projected pitch field | 720 mm for 720 tracks = 1 mm per track |
| Reading resolution | ~0.8 mm (≈ 1 track) |
| Drawn line width | 0.1 – 1.0 mm |
| Amplitude range from line width | 20 dB |
| Max scan speed | 24 mm/s |
| Minimum score duration | 20 s (at max speed) |
| Scan drive | gear rack + pinion + friction mechanism + motor; direction/speed lever; manual handle |
| Dynamic range | 48 dB or 24 dB (two versions) |
| Attack sharpness | 1 ms |
| Envelope resolution | 22 points per sweep |
| Formant resolution | 5 points per octave |

The score is effectively a **grayscale sonogram**: vertical axis = log frequency on the 1/72-octave
grid, horizontal axis = time, local transparency = partial amplitude, and vertical extent of the
scratch = spectral spread (single track → cluster/noise). This is the central data structure for
the emulation.

### 3.4 Coder

| Property | Value |
|---|---|
| Cutters | 16 blade-shaped cutters |
| Pitch selection | carriage on a piano-like scale, 1/6-semitone subdivisions |
| Harmonic selection | rack-and-pinion positions cutters at harmonics of the fundamental |
| Level control | cutter rotation angle sets groove width → level |
| Formant control | formant template limits cutter angles (sets overtone balance) |
| Envelope | decay ruler scales all overtone levels together |

The patent (Fig. 2) is the authoritative drawing of this mechanism.

---

## 4. Signal chain reconstruction

```
[projection lamp] -> [condenser] -> [spinning optical disc] -> [mirror] -> [objective]
      x5, one projector/disc per 2-octave band, stacked on the pitch axis
                                   |
                                   v
                    [score: mastic-coated glass, moving]
                                   |
                                   v
      [20 flat lenses, 36 mm each] -> [20 x FEU-2 photomultipliers]
                                   |
                       [6Zh32P pentode reader preamp]
                                   |
                         [20 x channel amplifier]
                                   |
             [6N2P preamp, x10 or bypass] -> [manipulator]
                                   |
              [two-stage 6N2P twin-T bandpass, 0.5 oct]
                                   |
                   [channel level preset / dB trim]
                                   |
                     [line 1 / line 2 output bus]
                                   |
                        [master amplifier -> speakers/tape]
```

### 4.1 Optical generator

- Five projector/disc assemblies; one motor and reduction gearbox drives the five discs at
  0.09375, 0.375, 1.5, 6, 24 rev/s. The discs are the same print; speed ratio 4 per band.
- Each disc has 144 concentric variable-area optical tracks, inner track lowest frequency,
  outer highest. The track is a sine wave printed as a variable-width transparent slit
  (like a cinema optical soundtrack), not a thin drawn sine line.
- Each projection lamp is a separate 12 V source switched by the “sound” foot pedal. The lamp
  supply is AC-derived; expect 100 Hz light ripple in the original (see §7).
- The optical system per disc: lamp → condenser → disc → movable mirror → objective. The five
  images are aligned into one vertical 720-track strip at the score.

### 4.2 Score and reader

- The score is a 7 mm mirror-glass plate in a moving metal frame; the mastic is scraped with a
  stylus or the coder. The scratch is the only light aperture; its length along time = duration,
  vertical position = pitch, width = level and spectral spread.
- The 720-track projection is split into 20 contiguous 36 mm segments. Each segment is focused by
  a flat lens onto one FEU-2. Thus each photomultiplier receives the **sum of all open tracks in
  its half-octave band**.
- The FEU-2 output goes to a 6Zh32P pentode voltage amplifier (reader preamp), then to the channel
  amplifier. The reader is therefore a wideband optical-to-electrical converter with the
  photomultiplier's noise, transit-time and gain characteristics.

### 4.3 Channel amplifier (per channel)

Functional block diagram (Habr fig. 22 / local `12_channel_amp_block.png`):

```
reader -> [preamp] -> [x10 switch] -> [manipulator] -> [filter] -> line 1 / line 2
                          ^                              ^
                    open / dynamics / modulation   channel level preset
```

- **Preamp** (`14_preamp_schematic.png`): one half of a 6N2P dual triode, common-cathode stage
  with feedback and a switchable feedback path. It can be bypassed (“depending on the saturation
  of the sound”). Gain around ×10 in the active position. It is the first place where tube
  harmonic distortion and soft overload appear.
- **Manipulator** (`15_manipulator_schematic.png`): four 6N2P triodes and a diode control network,
  with +250 V, +120 V, −4.5 V and −0.5 V rails. Functions: dynamic volume control, dynamic-range
  expansion (expander), and “animation” (modulation). The panel/functional diagram has three
  positions: *open*, *dynamics*, *modulation*.
- **Filter** (`16_filter_schematic.png`): two cascaded inverting 6N2P stages, each with a
  double-T RC network in the feedback path. The passband is 0.5 octave, matching the channel's
  36-track segment. The filter is described as removing optical-phonogram noise as well as
  shaping the channel. Output is switched to line 1 or line 2; connector pins 9/10 and 19/20 are
  visible in the drawing.
- **Channel levels**: each channel has a preset level lever and dB trim on the front panel.
  This 20-band level set is the instrument's main formant/timbre control. The “overall level”
  switches select output level/line.

### 4.4 Output

- Two output lines exist in the filter schematic. The exact original use (dual mono, stereo,
  or two groups of channels) is not fully documented. The front panel has “overall sound level”
  switches, and the filter output switch per channel selects line 1 or line 2. For the emulation
  this should be configurable: per-channel assignment to bus 1/2, plus an optional stereo link.

### 4.5 Mechanical/electrical

- Score drive: gear rack on the glass frame, pinion, friction mechanism, motor, direction/speed
  lever, manual crank. The motor supply schematic has start/end limit switches and an
  electromagnet for the mode switch.
- Lamp supply: 220 V AC input, five 12 V projection lamps, each switched by the “sound” pedal.
- Pedals: sound start, mode switching. Front-panel keys: amplitude envelope mode, dynamic control,
  memory record mode (exact function unclear).

---

## 5. What actually makes the ANS sound like the ANS

Ordered by expected audible importance:

1. **720 exact partials on a 1/72-octave grid.** The dense, slowly beating microtonal spectrum is
   the core. Phases are arbitrary per track and drift; the sum is not a conventional harmonic
   series unless the score draws one.
2. **The drawn sonogram and optical spill.** A 1 mm scratch at a track produces a clean tone; a
   wider scratch partially illuminates adjacent tracks and produces a narrow band, a formant
   cluster, or (at full width) noise. This “sound spot” behaviour is explicitly described by the
   museum and is central to the instrument's vocal/otherworldly character.
3. **The 20 half-octave channel filters.** Each group of 36 partials is filtered by a fixed
   0.5-octave bandpass before the channel level. This creates a 20-band fixed formant/coloration
   and removes optical noise outside each band. It is not a user-sweepable filter.
4. **The 20 channel level presets.** The composer shapes the overall spectrum with 20 levers;
   this is the instrument's “formant” control at the macro level.
5. **Tube stages.** 6N2P/6Zh32P preamps, manipulator and filters add gentle even-harmonic
   distortion, compression and noise. The effect is subtle; overdriving it would be wrong.
6. **Optical/mechanical imperfections.** Non-uniform emulsion, track-edge irregularity, disc
   eccentricity, motor/gear wow, lamp ripple and photomultiplier noise give the sustained “life”
   described by Kreichi. These should be subtle and per-disc, not per-partial random detuning.
7. **Scan behaviour.** Score speed (0–24 mm/s), direction, stop/start and manual scrub control
   duration and the time-smearing of attacks. The 1 ms attack spec means the electronics are fast;
   the optical aperture and score drawing set the actual envelope.
8. **The coder workflow.** Harmonic series with a formant template and a decay ruler is how most
   tonal ANS material was made. A faithful plugin should offer this, not only freehand drawing.

Less important for the instrument itself: tape hiss, room reverb, and the specific transfer of
1960s recordings. Those are recording-chain artifacts; offer them as optional “studio” colour,
off by default.

---

## 6. Implications for the emulation

### 6.1 Sound engine

- **Partial bank**: 720 sine partials at `f0 * 2^(n/72)`, `f0 ≈ 21.09 Hz`, with per-partial random
  initial phase. Amplitudes update at control rate from the score and live voices.
- **Sparse processing**: only synthesize partials whose amplitude is above a threshold. Typical
  scores use tens of partials. For a dense half-octave channel (many lit tracks), the engine
  substitutes a band-limited noise source calibrated to the same RMS as the 36 partials; this
  preserves the noise character and CPU. A 192-partial cap remains as a backstop for unusual
  material.
- **Oscillator choice**: wavetable sine with linear interpolation is the simplest; a recursive
  quadrature oscillator or an inverse-FFT additive bank is possible later. Avoid `sin()` per
  sample. Target a few percent of one MPC core for typical scores; measure on ARM.
- **Per-disc speed model**: one global speed multiplier plus small per-disc offsets; wow/flutter
  applied as a **common cents deviation** (a speed change shifts all partials on a disc by the
  same ratio). Add per-disc slow drift and tiny random walk. Do not detune each partial
  independently by large amounts.
- **Optical model**:
  - Vertical (pitch) blur: a small kernel across adjacent tracks, ~0.3–0.5 track sigma, to model
    the 0.8 mm reader resolution and mastic edge scattering. This produces the spill/cluster
    effect when the score is wide.
  - Horizontal (time) smoothing: ~1–5 ms, from the reading aperture and scan speed.
  - Per-track gain/edge irregularity: slow random amplitude modulation and a little 2nd/3rd
    harmonic distortion (the variable-area track is not a perfect sine).
  - HF loss: the finite track width (0.31 mm vs 0.423 mm wavelength) and lens aperture give a
    gentle top-octave rolloff. A one-pole lowpass around 12–18 kHz, or a measured curve, is a
    reasonable starting point.
  - Lamp ripple: optional 100 Hz hum/AM, very low level.
  - Photomultiplier shot noise and tube thermal noise: low-level noise, gated by the channel
    envelope so it does not hiss in silence.
- **Channel chain** (20×): sum the 36 partials → switchable 6N2P-like preamp (soft saturation,
  ×10/bypass) → manipulator (open / expander / modulation) → 4th-order 0.5-octave bandpass
  (twin-T-style response) → channel level → bus 1/2. The filters and saturation are per channel,
  not per partial.
- **Master**: sum buses, optional output tube stage/soft clip, DC blocker, master volume.
- **Manipulator approximation**: envelope follower + downward expander for “dynamics”; slow
  random/periodic gain modulation for “modulation”; clean path for “open”. The exact tube VCA
  curve is unknown, so expose amount/rate and tune against reference recordings.
- **Filter approximation**: 4th-order bandpass with −3 dB edges at the channel band edges
  (Q ≈ 2.87 per 2nd-order stage if implemented as two cascaded biquads, or a single 4th-order
  design). The original is two cascaded twin-T feedback stages; the exact Q/ripple is unknown.
  The audible target is a flat-ish 0.5-octave passband with a gentle 12 dB/oct skirt.

### 6.2 Score engine

- Store scores as **grayscale PNGs**, height 720 rows (1 row = 1 track), width arbitrary.
  Pixel value = transparency/level. A drawn line's width in rows naturally implements the
  “sound spot”. For sub-track line widths, render at 1440 or 2160 rows and area-average down to
  720 when loading.
- Scan horizontally at 0–24 mm/s. Define the image scale (e.g., 10 px/mm) in a sidecar or use a
  fixed convention; expose score length/duration. Support loop, ping-pong, stop, reverse, and
  manual position.
- Built-in scores: generate original ANS-idiom scores (formant vowels, harmonic glissandi,
  clusters/noise, slow spectral drifts, “Solaris”-style evolving beds). Do **not** copy
  copyrighted ANS scores from the sources; create new ones.
- User scores: drop PNGs into `Content/Scores/`; the plugin lists a fixed number of slots
  (e.g., 8 built-in + 8 user) or selects `user.png`.
- Live coder: 16-harmonic cluster with formant template, decay envelope, pitch quantized to the
  1/72-octave grid; trigger from MIDI notes/pads. This gives an immediately playable instrument
  and reproduces the coder workflow.

### 6.3 MPC plugin (OS 3.10.1)

- Built with the inMusic MPC Plugin SDK, VST2 ARM32/ARM64 + native.
- No plugin window; the UI is JSON. The scripted UI supports knobs/faders/switches/labels and
  static images, **but no dynamic canvas**, so a real score editor cannot live on the MPC screen.
- Proposed pages:
  1. **SCORE** — score select, start/stop, scan speed, direction, position, loop.
  2. **SPECTRUM A/B** — 20 channel levels (2 pages of 10, or 4 pages of 5) + master.
  3. **CODER** — fundamental, harmonic count, formant preset/shape, decay, trigger.
  4. **DYNAMICS** — manipulator mode, expander amount, modulation rate/depth.
  5. **OPTICS** — wow/flutter, track noise, crosstalk/blur, HF loss, hum.
  6. **OUTPUT** — line 1/2 routing, stereo mode, master volume.
- Q-Link layout: performance controls on the first page; 16 Q-Links for channel levels or coder
  harmonics on later pages.
- Parameter 0 must be harmless (master volume), per the SDK rules.
- All file loading in constructor/prepare, never in `process()`.

### 6.4 Desktop VST2

- The SDK's native build is a VST2 shared library and should load in a VST2-capable DAW. It has
  no GUI; parameters appear for automation, and MIDI can drive the live coder.
- The same score presets and Content/Scores work.
- **Phase 2 (recommended)**: a real graphical score editor. Because the MPC UI cannot draw, this
  needs either a separate desktop editor application or a custom GUI in the native build
  (outside the MPC JSON UI). It would open/import PNG/WAV spectrograms, draw with a brush,
  write `.ansscore` files into Content/Scores, and optionally run as a VST3/CLAP. The DSP core
  stays shared.
- VST2 is deprecated by Steinberg but still widely supported; confirm the user's DAW. If it only
  supports VST3, a thin VST3 wrapper around the same core is needed later.

---

## 7. Open technical questions and how to close them

| Question | Impact | Plan |
|---|---|---|
| Twin-T component values / exact filter response | Medium: channel coloration and crossover ripple | Approximate 4th-order 0.5-oct bandpass; optionally measure the real ANS or find a service manual; A/B against recordings |
| Manipulator transfer curve, expander ratio, modulation source | Medium: dynamics/animation character | Model as envelope-following VCA + expander + slow modulation; tune by ear against reference recordings |
| Lamp supply AC/DC and hum level | Low–medium: 100 Hz ripple | Make optional; default very low; verify from recordings/spectra |
| Meaning of output “line 1 / line 2” | Low: routing | Implement configurable per-channel bus assignment; default sensible stereo/dual mono |
| “Memory record” levers / amplitude envelope keys | Low: performance features | Treat as envelope/performance macros; revisit if documentation appears |
| Disc rotational phase and flutter depth | Low–medium: “life”/shimmer | Start subtle; compare spectra/envelopes with ANS recordings; expose depth |
| Exact partial phases | Low: timbre of dense clusters | Random per track; possibly coherent within a disc if evidence appears |
| Score image scale and copyright | Medium: usability/legal | Define px/mm convention; create original scores; allow user images |
| Headroom/gain staging of original | Medium: saturation amount | Calibrate so typical scores are clean, peaks touch the tube knee; expose drive |

---

## 8. Source/file inventory

```
research/
  ANS_TECHNICAL_REPORT.md          this document
  SOURCES.md                       URL list and notes
  ans_spec.json                    machine-readable specs/constants
  ans_tuning.csv                   720 frequencies + disc/track/channel map
```

Third-party texts, scans, and recordings used during the research (the Habr article,
patent scans, *Sound in Z*, museum articles, schematic images, and the live ANS
recording) are not redistributed; see `SOURCES.md` for the links. The filenames below
refer to the author's private archive:

Key named schematics:

- `10_reader_opto_electric.png` — FEU-2 → 6Zh32P reader preamp
- `12_channel_amp_block.png` — preamp → manipulator → filter → line 1/2
- `14_preamp_schematic.png` — 6N2P preamp with switchable feedback
- `15_manipulator_schematic.png` — 4× 6N2P + diode dynamics/modulation network
- `16_filter_schematic.png` — two-stage twin-T bandpass, output switch
- `21_patent_fig1_fig2.jpg` — score/optical slit/photocells and 16-cutter coder
- `07_score_drive.png`, `08_score_drive_supply.png` — mechanical drive
- `05_lamp_supply.png` — five 12 V projection lamps
- `02_disc_and_tracks.png` — disc and variable-area track detail
- `03_optical_projection.png` — lamp/condenser/disc/mirror/objective/score
- `17_coder.png`, `18_coder_drum.png` — coder and duration drum

---

## 9. Decisions needed before coding

1. **Vendor/manufacturer**: reuse `bokontep` from the Polivoks plugin, or another name?
2. **Plugin name**: `ANS`, `ANS 1958`, `ANS 1964`, `ANS Photoelectronic`?
3. **DAW/OS**: which DAW and platform for the desktop VST2 (Linux/macOS/Windows)?
4. **v1 scope**:
   - A) Accurate engine + MPC JSON UI + built-in/imported score images + live MIDI coder;
     graphical score editor later. (Recommended.)
   - B) Require the graphical editor from day one; then plan a separate desktop frontend and
     delay the MPC build.
5. **Reference recordings** for A/B calibration (e.g., specific Kreichi/Artemiev tracks) and
   whether “studio/tape colour” should be included or kept off by default.

Once these are settled, the next step is to scaffold the plugin with the SDK, implement the DSP
core against `ans_spec.json`, and validate with the host simulator + rendered WAVs before the first
MPC install.
