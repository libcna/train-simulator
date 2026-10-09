#!/usr/bin/env bash
# Matched closed-window, open-leaf and exterior views of passenger doors.
set -euo pipefail
cd "$(dirname "$0")/.."
stage=${1:?Specify before or after}
prefix=${2:?Specify unique repair prefix}
[[ "$stage" == before || "$stage" == after ]] || exit 2
[[ "$prefix" =~ ^[a-z0-9-]+$ ]] || exit 2
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
capture closed-right --route-km 52 --audit-camera 0.05 2.3 -44.5 90 0
capture closed-left --route-km 52 --audit-camera -0.05 2.3 -44.5 -90 0
capture exterior --route-km 52 --audit-camera 3.8 2.3 -44.5 -90 0
capture vestibule --station 7 --vestibule-view --car 3
if [[ "$prefix" == door-controls ]]; then
    capture control-right --station 4 --audit-camera 0.90 2.06 -45.28 90 0
    capture control-left --station 4 --audit-camera -0.90 2.06 -45.28 -90 0
fi
./build/train-simulator --frames 600 --quality 2 --time-scale 0.000001 \
    --no-audio --no-avatars --hide-hud --config tests/facade-comparison.cfg \
    --route-km 52 --car 2 --report-state >"$output/$prefix-performance-$stage.log" 2>&1
sha256sum ./build/train-simulator >"$output/$prefix-$stage-binary.sha256"
