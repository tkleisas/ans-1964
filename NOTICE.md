# Notices and attribution

## ANS 1964

Copyright (C) 2026 bokontep.

The plugin source and binaries are licensed under the GNU General Public License,
version 3 or (at your option) any later version. The full license text is in
[`LICENSE`](LICENSE).

“ANS” is the name of the original 1964 photoelectronic synthesizer designed by
Evgeny Murzin. The name is used here descriptively. This project is an independent
emulation and is not affiliated with, endorsed by, or sponsored by inMusic Brands,
Akai Professional, Steinberg Media Technologies, the Murzin estate, or the original
designers.

The emulation was developed from publicly available documentation and published
measurements. The technical report and source list are in [`research/`](research/).
No article scans, patent scans, schematic scans, score images from the original
instrument, or ANS audio recordings are redistributed in this repository or in the
release archives. The built-in scores are original studies in the ANS idiom.

“VST” is a trademark of Steinberg Media Technologies GmbH. “MPC” and “Akai
Professional” are trademarks of inMusic Brands, Inc.

## MPC Plugin SDK

The MPC plugin is built with the MPC Plugin SDK:

<https://github.com/inmusic-software/mpc-plugin-builder-sdk>

Copyright (c) 2026 inMusic Brands, Inc.

The SDK is MIT-licensed. The full license text is reproduced in
[`LICENSES/MPC-Plugin-SDK-MIT.txt`](LICENSES/MPC-Plugin-SDK-MIT.txt). The SDK
contains no code from Steinberg's VST SDK. The build script pins the SDK commit in
[`build.sh`](build.sh).

The five header-only DSP helpers vendored under `third_party/mpc-sdk/dsp/` are from
that SDK and carry the same MIT license. They are included so the VST3 can build
without a full SDK checkout.

## JUCE and the VST3 SDK

The Linux VST3 is built with JUCE 7.0.12, used under the GNU General Public License
version 3. JUCE is included as the `third_party/JUCE` git submodule; its own
`LICENSE.md` applies. The VST3 SDK bundled with JUCE is used under its GPLv3 option.

The VST3 plugin itself is GPL-3.0-or-later, like the MPC/desktop plugin.
