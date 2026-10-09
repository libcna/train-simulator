#!/usr/bin/env bash
# Companion to the immutable visual audit baseline. Use on an isolated display.
set -euo pipefail
cd "$(dirname "$0")/.."
binary=${1:-./build/train-simulator}
output=build/visual-audit
mkdir -p "$output"
capture() {
    local id=$1
    shift
    "$binary" --frames 3 --quality 2 --time-scale 0.000001 --no-audio --no-avatars \
        --hide-hud --config tests/facade-comparison.cfg --report-state \
        --screenshot "$output/$id-after.png" "$@" >"$output/$id-after.log" 2>&1
}
capture window-corner --route-km 52 --audit-camera 0.90 2.35 -49 90 25
capture window-oblique --route-km 52 --audit-camera 0.25 2.0 -49 65 12
capture window-outside --route-km 52 --audit-camera 3.8 2.4 -49 -90 0
capture doors-platform --station 7 --door-view
for car in 1 2 3 4 5 6; do
    capture "car-$car-aisle" --car "$car" --route-km 52
done
"$binary" --frames 600 --quality 2 --time-scale 0.000001 --no-audio --no-avatars \
    --hide-hud --config tests/facade-comparison.cfg --route-km 52 \
    --audit-camera 0.90 2.35 -49 90 25 --report-state \
    --screenshot "$output/window-performance-after.png" \
    >"$output/window-performance-after.log" 2>&1
sha256sum "$binary" >"$output/window-repair-binary.sha256"
