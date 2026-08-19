#!/usr/bin/env bash

set -u

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
engine_binary="$repo_root/cmake-build-profile/engine"
profile_dir="$repo_root/profiles"
timestamp="$(date +%Y%m%d-%H%M%S)"
profile_path="$profile_dir/cpu-$timestamp.perf.data"
frame_trace_path="$profile_dir/cpu-$timestamp.frames.log"

if ! command -v perf >/dev/null 2>&1; then
    echo "error: perf is not installed" >&2
    exit 1
fi

if [[ ! -x "$engine_binary" ]]; then
    echo "error: profiling build not found at $engine_binary" >&2
    echo "build cmake-build-profile before capturing" >&2
    exit 1
fi

mkdir -p "$profile_dir"

echo "CPU profile capture: $profile_path"
echo "Frame-dip trace: $frame_trace_path"
echo "Exercise the hitch, then close the engine normally to finish the capture."

cd "$repo_root" || exit 1
perf record \
    --output "$profile_path" \
    --event cycles \
    --freq 999 \
    --call-graph fp \
    --latency \
    --switch-events \
    --timestamp \
    --clockid mono \
    --sample-cpu \
    --mmap-pages 128 \
    --compression-level=1 \
    -- "$engine_binary" \
    --trace-frame-dips-ms=12 \
    --frame-trace-output="$frame_trace_path"
status=$?

if [[ -s "$profile_path" ]]; then
    echo "Capture saved: $profile_path"
else
    echo "error: perf did not produce a capture" >&2
fi

if [[ -s "$frame_trace_path" ]]; then
    echo "Frame-dip trace saved: $frame_trace_path"
else
    echo "No frames exceeded the 12 ms trace threshold."
fi

exit "$status"
