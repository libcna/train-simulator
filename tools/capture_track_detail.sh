#!/usr/bin/env bash
# Matched real-engine ballast, sleeper and rail views on an isolated display.
set -euo pipefail
cd "$(dirname "$0")/.."
stage=${1:?Specify before or after}
[[ "$stage" == before || "$stage" == after ]] || exit 2
output=build/visual-audit
prefix=${2:-track-surface}
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
capture track-close --route-km 52 --audit-camera 3.5 2.4 -49 -90 -35
capture track-down --route-km 52 --audit-camera 1.2 2.0 -49 90 -40
capture shoulder --route-km 52 --audit-camera 3.5 1.6 -49 -70 -30
capture track-ahead --route-km 52 --audit-camera 0 3.2 -120 180 -12
capture adjacent-line --route-km 52 --audit-camera -6.5 1.4 -49 -90 -25
capture signal --route-km 52 --audit-camera 9 3.0 -78 -145 -3
capture window --route-km 52 --window-view
./build/train-simulator --frames 600 --quality 2 --time-scale 0.000001 \
    --no-audio --no-avatars --hide-hud --config tests/facade-comparison.cfg \
    --route-km 52 --window-view --report-state >"$output/$prefix-performance-$stage.log" 2>&1
sha256sum ./build/train-simulator >"$output/$prefix-$stage-binary.sha256"
