#!/bin/bash
set -euo pipefail

HERE=$(cd "$(dirname "$0")" && pwd)
WORK=${WORK:-$HERE/build}
export DEVELOPER_DIR=${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}

FW_DIR=${FW_DIR:-$WORK/FFmpegAudio.xcframework/macos-arm64}
BIN=$WORK/decode_smoke

xcrun -sdk macosx clang -target arm64-apple-macos13.0 -Wall -Werror \
    -F "$FW_DIR" -framework FFmpegAudio -Wl,-rpath,"$FW_DIR" \
    "$HERE/smoke/decode_smoke.c" -o "$BIN"

"$BIN" --codecs
"$BIN" dca "$WORK/fixtures/tone-5.1-48k.dts"
if [ -f "$WORK/fixtures/tone-5.1-48k.thd" ]; then
    "$BIN" truehd "$WORK/fixtures/tone-5.1-48k.thd"
fi
