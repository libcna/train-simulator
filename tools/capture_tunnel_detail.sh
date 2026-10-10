#!/usr/bin/env bash
# Matched real-engine tunnel lining, fixture and service-marker views on an isolated display.
set -euo pipefail
cd "$(dirname "$0")/.."
stage=${1:?Specify before or after}
[[ "$stage" == before || "$stage" == after ]] || exit 2
output=build/visual-audit
prefix=${2:-tunnel-lining}
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
capture window --route-km 123 --window-view
capture left-window --route-km 123 --left-window-view
capture niche --route-km 123.003 --window-view
capture wall-oblique --route-km 123.01 --audit-camera 0.3 2.36 -49 60 5
capture fixture-close --route-km 123.012 --audit-camera 0.9 2.36 -49 90 18
capture lining-ahead --route-km 123 --audit-camera 0.9 2.36 -49 25 8
capture marker-close --route-km 123.003 --audit-camera 0.9 2.0 -49 90 -4
if [[ "$prefix" == tunnel-markers ]]; then
    # The audit camera sits 49 m behind the train reference, so the 123.0 km bay is abeam here.
    capture niche-bay --route-km 123.049 --audit-camera 0.9 2.0 -49 90 0
    capture niche-bay-left --route-km 123.049 --audit-camera -3.5 2.0 -49 -90 0
    capture bay-oblique --route-km 123.055 --audit-camera 0.9 2.0 -49 60 0
fi
./build/train-simulator --frames 600 --quality 2 --time-scale 0.000001 \
    --no-audio --no-avatars --hide-hud --config tests/facade-comparison.cfg \
    --route-km 123 --window-view --report-state >"$output/$prefix-performance-$stage.log" 2>&1
sha256sum ./build/train-simulator >"$output/$prefix-$stage-binary.sha256"
