#!/usr/bin/env bash
# Supplemental screen checks: rear face, station dwell, reverse service and rotating page.
set -euo pipefail
cd "$(dirname "$0")/.."
output=build/visual-audit
capture() {
    local id=$1
    shift
    ./build/train-simulator --quality 2 --no-audio --no-avatars --hide-hud \
        --config tests/facade-comparison.cfg --report-state \
        --screenshot "$output/display-typography-$id.png" "$@" \
        >"$output/display-typography-$id.log" 2>&1
}
capture rear-face --frames 3 --time-scale 0.000001 --route-km 52 \
    --audit-camera -0.15 2.85 -55.2 180 15
capture station --frames 3 --time-scale 0.000001 --station 4 \
    --audit-camera 0.15 2.85 -49.8 0 15
capture reverse-service --frames 3 --time-scale 0.000001 --route-km 52 --direction -1 \
    --audit-camera 0.15 2.85 -49.8 0 15
capture following --frames 430 --time-scale 1 --route-km 52 \
    --audit-camera 0.15 2.85 -49.8 0 15
