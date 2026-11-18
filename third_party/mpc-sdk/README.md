# MPC SDK DSP headers

The five header-only DSP helpers in `dsp/` are vendored from the inMusic MPC
Plugin Builder SDK:

<https://github.com/inmusic-software/mpc-plugin-builder-sdk>

Copyright (c) 2026 inMusic Brands, Inc. MIT-licensed; see
[`../../LICENSES/MPC-Plugin-SDK-MIT.txt`](../../LICENSES/MPC-Plugin-SDK-MIT.txt).

They are included so the VST3 in `vst3/` can build without a full MPC SDK
checkout. The MPC plugin build uses the SDK's own copy of these headers.

Files: `math.h`, `biquad.h`, `onepole.h`, `noise.h`, `oscillator.h`.
