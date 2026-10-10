#!/usr/bin/env bash
# Matched real-engine lineside fence, signal and catenary views on an isolated display.
set -euo pipefail
cd "$(dirname "$0")/.."
stage=${1:?Specify before or after}
[[ "$stage" == before || "$stage" == after ]] || exit 2
output=build/visual-audit
prefix=${2:-lineside}
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
capture signal --signal-view
capture signal-oblique --route-km 52 --audit-camera 9 3.0 -78 -145 -3
capture fence-window --route-km 52 --left-window-view
capture fence-close --route-km 52 --audit-camera -7.2 1.4 -49 -90 -8
capture fence-along --route-km 52 --audit-camera -7.5 1.9 -49 -15 -4
capture fence-end --route-km 73.85 --audit-camera -7.5 1.9 -49 -8 -4
capture wire-along --route-km 52 --audit-camera -1.2 4.9 -49 -6 6
capture wire-curve --route-km 30 --audit-camera -1.2 4.9 -49 -6 6
capture wire-oblique --route-km 30 --audit-camera 12 3.0 -44 -90 10
./build/train-simulator --frames 600 --quality 2 --time-scale 0.000001 \
    --no-audio --no-avatars --hide-hud --config tests/facade-comparison.cfg \
    --route-km 52 --left-window-view --report-state >"$output/$prefix-performance-$stage.log" 2>&1
sha256sum ./build/train-simulator >"$output/$prefix-$stage-binary.sha256"
