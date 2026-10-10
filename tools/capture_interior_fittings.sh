#!/usr/bin/env bash
# Matched real-engine close views of seat, vestibule, gangway and cab-end fittings.
set -euo pipefail
cd "$(dirname "$0")/.."
stage=${1:?Specify before or after}
[[ "$stage" == before || "$stage" == after ]] || exit 2
output=build/visual-audit
prefix=${2:-interior-fittings}
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
capture seat-rear-close --route-km 52 --audit-camera 0.1 1.9 -55 180 -10
capture seat-rear-detail --route-km 52 --audit-camera 0.1 1.7 -54.2 180 -14
capture seat-front-close --route-km 52 --audit-camera 0.15 1.65 -48 0 -18
capture seat-pedestal --route-km 52 --audit-camera 0.0 1.1 -49.5 60 -40
capture car-5-aisle --car 5 --route-km 52
capture luggage-rack --car 5 --route-km 52 --audit-camera 0.0 1.9 -52 20 22
capture car-1-aisle --car 1 --route-km 52
capture cab-door --car 1 --route-km 52 --audit-camera 0.0 1.9 -46 0 0
capture car-6-aisle --car 6 --route-km 52
capture vestibule --vestibule-view --car 3
capture gangway --gangway-view --car 3
./build/train-simulator --frames 600 --quality 2 --time-scale 0.000001 \
    --no-audio --no-avatars --hide-hud --config tests/facade-comparison.cfg \
    --route-km 52 --car 3 --report-state >"$output/$prefix-performance-$stage.log" 2>&1
sha256sum ./build/train-simulator >"$output/$prefix-$stage-binary.sha256"
