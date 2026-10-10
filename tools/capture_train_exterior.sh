#!/usr/bin/env bash
# Matched real-engine views of the passenger train's exterior shell and equipment.
set -euo pipefail
cd "$(dirname "$0")/.."
stage=${1:?Specify before or after}
[[ "$stage" == before || "$stage" == after ]] || exit 2
output=build/visual-audit
prefix=${2:-train-exterior}
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
capture train-side --route-km 52 --audit-camera 12 3.0 -44 -90 -2
capture train-low --route-km 52 --audit-camera 9 1.3 -44 -90 4
capture train-front --route-km 52 --audit-camera 9 3.0 -78 -145 -3
capture nose-close --route-km 52 --audit-camera 2.5 2.4 -70 -160 -2
capture train-wide --route-km 52 --audit-camera 55 24 15 -70 -20
capture roof --route-km 52 --audit-camera 8 7.5 -30 -90 -25
capture window-outside --route-km 52 --audit-camera 3.8 2.4 -49 -90 0
capture car-gap --route-km 52 --audit-camera 6 2.2 -42 -100 0
capture doors-platform --station 7 --door-view
./build/train-simulator --frames 600 --quality 2 --time-scale 0.000001 \
    --no-audio --no-avatars --hide-hud --config tests/facade-comparison.cfg \
    --route-km 52 --car 1 --report-state >"$output/$prefix-performance-$stage.log" 2>&1
sha256sum ./build/train-simulator >"$output/$prefix-$stage-binary.sha256"
