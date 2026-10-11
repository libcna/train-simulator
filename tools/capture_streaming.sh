#!/usr/bin/env bash
# Run through CNA's private GPU display wrapper (see handoff.md).
# Static views prove geometry preservation; the moving run keeps boundary hitches in p99.
set -euo pipefail
cd "$(dirname "$0")/.."
stage=${1:?Specify before or after}
prefix=${2:-streaming}
[[ "$stage" == before || "$stage" == after ]] || exit 2
[[ "$prefix" =~ ^[a-z][a-z0-9-]*$ ]] || exit 2
output=build/visual-audit
[[ "$stage" != before || ! -e "$output/$prefix-static-before-binary.sha256" ]] || exit 2
mkdir -p "$output"
bash tools/capture_track_detail.sh "$stage" "$prefix-static"
/usr/bin/time -v ./build/train-simulator --frames 1800 --route-km 52 --time-scale 8 \
    --window-view --car 2 --quality 2 --no-audio --no-avatars --report-state \
    --config tests/facade-comparison.cfg --screenshot "$output/$prefix-moving-$stage.png" \
    >"$output/$prefix-moving-$stage.log" 2>&1
sha256sum ./build/train-simulator >"$output/$prefix-$stage-binary.sha256"
