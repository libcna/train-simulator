#!/usr/bin/env bash
# Matched real-engine water views on an isolated display.
set -euo pipefail
cd "$(dirname "$0")/.."
stage=${1:?Specify before or after}
[[ "$stage" == before || "$stage" == after ]] || exit 2
output=build/visual-audit
prefix=${2:-water-surface}
[[ "$prefix" =~ ^[a-z][a-z0-9-]*$ ]] || exit 2
[[ "$stage" != before || ! -e "$output/$prefix-before-binary.sha256" ]] || exit 2
capture() {
    local id=$1
    shift
    ./build/train-simulator --frames 3 --quality 2 --time-scale 0.000001 \
        --no-audio --no-avatars --hide-hud --config tests/facade-comparison.cfg \
        --report-state --screenshot "$output/$prefix-$id-$stage.png" \
        "$@" >"$output/$prefix-$id-$stage.log" 2>&1
}
capture lake --route-km 84 --window-view
capture lake-oblique --route-km 84 --audit-camera 85 9 -40 65 -12
capture river --route-km 74.5 --window-view
capture river-oblique --route-km 74.5 --audit-camera 20 12 -20 90 -20
capture waterfront --station 5 --platform-view
if [[ "$prefix" == lake-bank ]]; then
    capture near-bank --route-km 84 --audit-camera 60 3 -40 90 -20
    capture far-bank --route-km 84 --audit-camera 440 3 -40 90 -8
fi
if [[ "$prefix" == river-bank ]]; then
    capture river-left --route-km 74.5 --left-window-view
    capture river-oblique-left --route-km 74.5 --audit-camera -20 12 -20 -90 -20
fi
performance_km=84
if [[ "$prefix" == river-bank ]]; then performance_km=74.5; fi
./build/train-simulator --frames 600 --quality 2 --time-scale 0.000001 \
    --no-audio --no-avatars --hide-hud --config tests/facade-comparison.cfg \
    --route-km "$performance_km" --window-view --report-state >"$output/$prefix-performance-$stage.log" 2>&1
sha256sum ./build/train-simulator >"$output/$prefix-$stage-binary.sha256"
