#!/usr/bin/env bash
# Matched real-engine river-bridge and dry-viaduct views on an isolated display.
set -euo pipefail
cd "$(dirname "$0")/.."
stage=${1:?Specify before or after}
[[ "$stage" == before || "$stage" == after ]] || exit 2
output=build/visual-audit
prefix=${2:-bridge-parapet}
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
capture river-window --route-km 74.5 --window-view
capture river-left --route-km 74.5 --left-window-view
capture river-down --route-km 74.5 --audit-camera 1.2 2.0 -49 90 -28
capture river-oblique --route-km 74.5 --audit-camera 14 6 -30 -60 -14
capture viaduct-window --route-km 177.9 --window-view
capture viaduct-down --route-km 177.9 --audit-camera 1.2 2.0 -49 90 -28
capture viaduct-oblique --route-km 177.9 --audit-camera 30 4 -10 -70 -8
capture viaduct-valley --route-km 177.9 --audit-camera 120 6 40 -110 -4
capture viaduct-approach --route-km 177.9 --audit-camera 2 10 120 180 -10
if [[ "$prefix" == viaduct-valley ]]; then
    capture valley-floor --route-km 177.9 --audit-camera 220 -12 -250 -25 2
    capture valley-diagonal --route-km 177.9 --audit-camera -400 10 0 45 -6
    capture valley-along --route-km 177.9 --audit-camera 2 6 -300 0 -6
    capture valley-underside --route-km 177.9 --audit-camera 40 -22 0 -90 12
    capture abutment-begin --route-km 177.9 --audit-camera 45 -2 -600 -60 -4
    capture abutment-end --route-km 179.3 --audit-camera -45 -2 600 120 -4
fi
./build/train-simulator --frames 600 --quality 2 --time-scale 0.000001 \
    --no-audio --no-avatars --hide-hud --config tests/facade-comparison.cfg \
    --route-km 177.9 --window-view --report-state >"$output/$prefix-performance-$stage.log" 2>&1
sha256sum ./build/train-simulator >"$output/$prefix-$stage-binary.sha256"
