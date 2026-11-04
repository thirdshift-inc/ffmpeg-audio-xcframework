#!/bin/bash
set -euo pipefail

HERE=$(cd "$(dirname "$0")" && pwd)
SRC=$(cd "$HERE/.." && pwd)
WORK=${WORK:-$HERE/build}
JOBS=${JOBS:-4}
NICE=${NICE:-19}
export DEVELOPER_DIR=${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}

ENC=$WORK/fixture-encoder
OUT=$WORK/fixtures
DURATION=2

if [ ! -x "$ENC/ffmpeg" ]; then
    rm -rf "$ENC"
    mkdir -p "$ENC"
    (
        cd "$ENC"
        nice -n "$NICE" "$SRC/configure" \
            --disable-everything --disable-autodetect --disable-doc --disable-network \
            --disable-avdevice --disable-swscale --disable-ffplay --disable-ffprobe \
            --enable-ffmpeg --enable-swresample \
            --enable-protocol=file \
            --enable-filter=aevalsrc --enable-filter=aresample --enable-filter=aformat \
            --enable-encoder=dca --enable-encoder=truehd \
            --enable-muxer=dts --enable-muxer=truehd \
            "--cc=xcrun -sdk macosx clang" >configure.log 2>&1 || { tail -40 configure.log >&2; exit 1; }
        nice -n "$NICE" make -j"$JOBS" ffmpeg >make.log 2>&1 || { tail -40 make.log >&2; exit 1; }
    )
fi

mkdir -p "$OUT"
TONE="aevalsrc=exprs=0.25*sin(440*2*PI*t)|0.25*sin(550*2*PI*t)|0.25*sin(660*2*PI*t)|0.25*sin(55*2*PI*t)|0.25*sin(880*2*PI*t)|0.25*sin(1100*2*PI*t):channel_layout=5.1(side):sample_rate=48000:duration=$DURATION"

encode() {
    local attempt
    for attempt in 1 2 3 4 5; do
        if "$ENC/ffmpeg" -nostdin -hide_banner -loglevel error -y -filter_complex "$TONE" \
            -strict -2 -fflags +bitexact -flags:a +bitexact "$@"; then
            return 0
        fi
        echo "encoder exited with $? on attempt $attempt, retrying" >&2
    done
    return 1
}

encode -c:a dca -b:a 768k -f dts "$OUT/tone-5.1-48k.dts"
encode -c:a truehd -f truehd "$OUT/tone-5.1-48k.thd"

(cd "$OUT" && shasum -a 256 tone-5.1-48k.dts tone-5.1-48k.thd) >"$OUT/SHA256SUMS"
cat "$OUT/SHA256SUMS"
