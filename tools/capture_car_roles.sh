#!/usr/bin/env bash
# All six production-car views plus close evidence for their distinctive facilities.
set -euo pipefail
cd "$(dirname "$0")/.."
stage=${1:?Specify before or after}
[[ "$stage" == before || "$stage" == after ]] || exit 2
output=build/visual-audit
prefix=car-roles
[[ "$stage" != before || ! -e "$output/$prefix-before-binary.sha256" ]] || exit 2
capture() {
    local id=$1
    shift
    ./build/train-simulator --frames 3 --quality 2 --time-scale 0.000001 \
        --no-audio --no-avatars --hide-hud --config tests/facade-comparison.cfg \
        --route-km 52 --report-state --screenshot "$output/$prefix-$id-$stage.png" \
        "$@" >"$output/$prefix-$id-$stage.log" 2>&1
}
for car in 1 2 3 4 5 6; do capture "car-$car" --car "$car"; done
capture accessible --audit-camera 0.05 2.5 -15.5 0 -30
capture family --audit-camera 0.10 2.15 10.8 65 -30
capture work --audit-camera 0.10 2.15 -31.2 65 -30
capture luggage --audit-camera 0.12 2.3 36 -60 -12
# Supplemental rack view: the original luggage pose sees a table, not the vestibule rack.
if [[ "$stage" == after ]]; then capture rack --audit-camera 0.12 2.3 23 -90 -12; fi
./build/train-simulator --frames 600 --quality 2 --time-scale 0.000001 \
    --no-audio --no-avatars --hide-hud --config tests/facade-comparison.cfg \
    --route-km 52 --car 2 --report-state >"$output/$prefix-performance-$stage.log" 2>&1
sha256sum ./build/train-simulator >"$output/$prefix-$stage-binary.sha256"
