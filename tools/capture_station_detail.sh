#!/usr/bin/env bash
# Matched station signage/construction views; run only on a private display.
set -euo pipefail
cd "$(dirname "$0")/.."
stage=${1:?Specify before or after}
prefix=${2:-station-type}
[[ "$stage" == before || "$stage" == after ]] || exit 2
[[ "$prefix" =~ ^[a-z][a-z0-9-]*$ ]] || exit 2
output=build/visual-audit
[[ "$stage" != before || ! -e "$output/$prefix-before-binary.sha256" ]] || exit 2
capture() {
    local id=$1
    shift
    ./build/train-simulator --frames 3 --quality 2 --time-scale 0.000001 \
        --no-audio --no-avatars --hide-hud --config tests/facade-comparison.cfg \
        --report-state --screenshot "$output/$prefix-$id-$stage.png" \
        "$@" >"$output/$prefix-$id-$stage.log" 2>&1
}
capture terminal --station 1 --platform-view --car 3
capture city --station 7 --platform-view --car 3
if [[ "$prefix" == platform-bins ]]; then
    capture bin-close --station 7 --audit-camera 3 1.50 -13 90 -5
    capture bin-oblique --station 1 --audit-camera -3 1.50 -14 -65 -5
fi
if [[ "$prefix" == platform-fencing ]]; then
    capture fence-close --station 1 --audit-camera -3 1.80 -9 -65 -12
    capture fence-end --station 7 --audit-camera 3 1.80 85 75 -8
fi
if [[ "$prefix" == station-glazing || "$prefix" == station-materials ]]; then
    capture glass-close --station 7 --audit-camera 3 1.80 -13 90 5
    capture glass-oblique --station 4 --audit-camera 3 1.90 -11 65 5
    capture major-front --station 9 --platform-view --car 2
fi
if [[ "$prefix" == station-turret ]]; then
    capture turret --station 7 --audit-camera 3 2.0 24 90 25
    capture turret-base --station 7 --audit-camera 3 1.8 23 83 0
fi
if [[ "$prefix" == platform-display ]]; then
    capture display-close --station 5 --audit-camera -3 1.90 14.88 -90 10
    capture display-oblique --station 5 --audit-camera -3 1.90 13 -120 10
fi
if [[ "$prefix" == terminal-forecourt ]]; then
    capture forecourt --station 1 --audit-camera -45 12 5 150 -18
    capture reverse-forecourt --station 12 --audit-camera -45 12 -5 30 -18
    capture forecourt-outward --station 1 --audit-camera -55 16 -95 145 -20
    capture reverse-outward --station 12 --audit-camera -55 16 95 35 -20
fi
if [[ "$prefix" == platform-paving || "$prefix" == platform-tactile ]]; then
    capture paving-close --station 7 --audit-camera 2.5 1.6 -9 90 -40
    capture paving-oblique --station 1 --audit-camera -2.5 1.8 -9 -65 -25
fi
if [[ "$prefix" == canopy-structure || "$prefix" == canopy-lights ]]; then
    capture major --station 9 --platform-view --car 3
    capture soffit-close --station 4 --audit-camera 3 2.4 -15 100 25
fi
if [[ "$prefix" == station-families && "$stage" == after ]]; then
    capture major --station 9 --platform-view --car 3
    capture major-front --station 9 --platform-view --car 2
    capture waterfront-front --station 5 --platform-view --car 2
fi
capture rural --station 4 --platform-view --car 3
capture waterfront --station 5 --platform-view
capture doors --station 7 --door-view
capture wide --station 1 --audit-camera 25 15 -20 -65 -18
if [[ "$prefix" == station-sign-layout && "$stage" == after ]]; then
    capture city-name --station 7 --audit-camera 1 1.90 -9.7 90 4
    capture terminal-name --station 1 --audit-camera -1 1.90 -9.7 -90 4
fi
performance_station=7
if [[ "$prefix" == terminal-forecourt ]]; then performance_station=1; fi
./build/train-simulator --frames 600 --quality 2 --time-scale 0.000001 \
    --no-audio --no-avatars --hide-hud --config tests/facade-comparison.cfg \
    --station "$performance_station" --platform-view --car 3 --report-state >"$output/$prefix-performance-$stage.log" 2>&1
sha256sum ./build/train-simulator >"$output/$prefix-$stage-binary.sha256"
