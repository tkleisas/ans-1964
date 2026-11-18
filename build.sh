#!/usr/bin/env bash
# Build the ANS 1964 MPC plugin, desktop VST2, and Linux VST3.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SDK_REPO="${SDK_REPO:-https://github.com/inmusic-software/mpc-plugin-builder-sdk.git}"
SDK_COMMIT="${SDK_COMMIT:-febd2ca5bd9eb16c4a412837196e145566bc981b}"
PYTHON="${PYTHON:-python3}"
PLUGIN="$ROOT/mpc"
PLUGIN_NAME="ANS 1964"

if [ -n "${SDK_DIR:-}" ]; then
    case "$SDK_DIR" in
        /*) ;;
        *) SDK_DIR="$ROOT/$SDK_DIR" ;;
    esac
else
    SDK_DIR="$ROOT/.sdk"
fi

echo "== ANS 1964 build =="
echo "   repo: $ROOT"
echo "   SDK:  $SDK_DIR @ $SDK_COMMIT"

if [ ! -d "$SDK_DIR/.git" ]; then
    echo "-- cloning MPC Plugin SDK"
    git clone "$SDK_REPO" "$SDK_DIR"
fi
git -C "$SDK_DIR" fetch --quiet origin "$SDK_COMMIT" || true
git -C "$SDK_DIR" checkout --quiet "$SDK_COMMIT"

rm -rf "$SDK_DIR/plugins/$PLUGIN_NAME"
mkdir -p "$SDK_DIR/plugins"
cp -a "$PLUGIN" "$SDK_DIR/plugins/$PLUGIN_NAME"

ZIG_ARGS=()
if [ -n "${ZIG:-}" ]; then ZIG_ARGS=(--zig "$ZIG"); fi

echo "-- building MPC (native + ARM32 + ARM64)"
"$PYTHON" "$SDK_DIR/tools/build.py" "$SDK_DIR/plugins/$PLUGIN_NAME" --target all "${ZIG_ARGS[@]}"

echo "-- building desktop VST2"
"$PYTHON" "$SDK_DIR/tools/build.py" "$SDK_DIR/plugins/$PLUGIN_NAME" --target desktop "${ZIG_ARGS[@]}"

echo "-- running host simulator test"
HOST_SIM="$SDK_DIR/.sdk-tools/host_sim"
NATIVE=""
for ext in so dylib; do
    f="$SDK_DIR/plugins/$PLUGIN_NAME/build/native/ans-1964.$ext"
    if [ -f "$f" ]; then NATIVE="$f"; break; fi
done
if [ -z "$NATIVE" ]; then
    echo "error: native test library not found" >&2
    exit 1
fi
if [ ! -x "$HOST_SIM" ] && [ ! -f "$HOST_SIM" ]; then
    echo "error: host_sim not found at $HOST_SIM" >&2
    exit 1
fi
mkdir -p "$ROOT/dist"
TEST_WAV="$ROOT/dist/.host-sim-test.wav"
"$HOST_SIM" "$NATIVE" test --seconds 3 --notes 60,64,67 --wav "$TEST_WAV"
rm -f "$TEST_WAV"

echo "-- packaging MPC release asset"
"$PYTHON" "$SDK_DIR/tools/package.py" "$SDK_DIR/plugins/$PLUGIN_NAME" --out "$ROOT/dist"

echo "-- packaging desktop VST2"
DESKTOP="$SDK_DIR/plugins/$PLUGIN_NAME/build/desktop/$PLUGIN_NAME"
rm -rf "$ROOT/dist/desktop-VST2"
cp -a "$DESKTOP" "$ROOT/dist/desktop-VST2"
cp "$ROOT/LICENSE" "$ROOT/NOTICE.md" "$ROOT/dist/"
( cd "$ROOT/dist" && rm -f ans-1964-1.1.2-linux-vst2.zip && zip -qr ans-1964-1.1.2-linux-vst2.zip desktop-VST2 LICENSE NOTICE.md )

if [ "${SKIP_VST3:-0}" != "1" ]; then
    if [ ! -f "$ROOT/third_party/JUCE/CMakeLists.txt" ]; then
        if [ -d "$ROOT/.git" ]; then
            echo "-- fetching JUCE submodule"
            git -C "$ROOT" submodule update --init --recursive
        else
            echo "error: third_party/JUCE is missing; clone with --recursive or run git submodule update --init" >&2
            exit 1
        fi
    fi
    echo "-- building VST3"
    cmake -S "$ROOT/vst3" -B "$ROOT/vst3/build" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$ROOT/vst3/build" -j"$(nproc)"
    VST3_BUNDLE="$ROOT/vst3/build/ANS1964_artefacts/Release/VST3/$PLUGIN_NAME.vst3"
    if [ ! -d "$VST3_BUNDLE" ]; then
        echo "error: VST3 bundle not found at $VST3_BUNDLE" >&2
        exit 1
    fi
    rm -rf "$ROOT/dist/$PLUGIN_NAME.vst3"
    cp -a "$VST3_BUNDLE" "$ROOT/dist/"
    ( cd "$ROOT/dist" && rm -f ans-1964-1.1.2-linux-vst3.zip && zip -qr ans-1964-1.1.2-linux-vst3.zip "$PLUGIN_NAME.vst3" LICENSE NOTICE.md )
fi

echo
echo "Done. Release assets:"
ls -lh "$ROOT/dist"/*.zip
