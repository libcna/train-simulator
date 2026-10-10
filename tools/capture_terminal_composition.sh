#!/usr/bin/env bash
# Matched real-engine overviews of both route terminals on an isolated display.
set -euo pipefail
cd "$(dirname "$0")/.."
stage=${1:?Specify before or after}
[[ "$stage" == before || "$stage" == after ]] || exit 2
output=build/visual-audit
prefix=${2:-terminal-composition}
[[ "$prefix" =~ ^[a-z][a-z0-9-]*$ ]] || exit 2
[[ "$stage" != before || ! -e "$output/$prefix-before-binary.sha256" ]] || exit 2
mkdir -p "$output"
capture() {
    local id=$1
    shift
    ./build/train-simulator --frames 3 --quality 2 --time-scale 0.000001 \
        --no-audio --no-avatars --hide-hud --config tests/facade-comparison.cfg \
        --report-state --screenshot "$output/$prefix-$id-$stage.png" \
        "$@" >"$output/$prefix-$id-$stage.log" 2>&1
}
capture terminal-wide --station 1 --audit-camera 25 15 -20 -65 -18
capture terminal-aerial --station 1 --audit-camera 70 45 -60 -55 -30
capture head-end --station 1 --audit-camera 2 3.2 -30 0 -2
capture head-end-high --station 1 --audit-camera -30 12 40 -20 -12
capture terminal-platform --station 1 --platform-view --car 3
capture reverse-wide --station 12 --audit-camera -25 15 20 115 -18
capture reverse-head-end --station 12 --audit-camera -2 3.2 30 180 -2
./build/train-simulator --frames 600 --quality 2 --time-scale 0.000001 \
    --no-audio --no-avatars --hide-hud --config tests/facade-comparison.cfg \
    --station 1 --platform-view --car 3 --report-state >"$output/$prefix-performance-$stage.log" 2>&1
sha256sum ./build/train-simulator >"$output/$prefix-$stage-binary.sha256"
