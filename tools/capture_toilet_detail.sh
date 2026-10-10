#!/usr/bin/env bash
# Matched real-engine views of the accessible toilet compartment and its threshold.
set -euo pipefail
cd "$(dirname "$0")/.."
stage=${1:?Specify before or after}
[[ "$stage" == before || "$stage" == after ]] || exit 2
output=build/visual-audit
prefix=${2:-toilet-fittings}
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
capture toilet --toilet-view
capture sink --sink-view
# Compartment: x 0.44..1.35, z 4.6..8.0 (car 4); bowl near z 7.5, sink near z 4.25.
capture bowl-close --route-km 52 --car 4 --audit-camera 0.72 1.55 6.2 165 -28
capture bowl-side --route-km 52 --car 4 --audit-camera 0.62 1.35 7.5 90 -22
capture sink-close --route-km 52 --car 4 --audit-camera 0.72 1.55 5.35 15 -30
capture mirror --route-km 52 --car 4 --audit-camera 0.68 1.65 4.45 90 2
capture flush-dispenser --route-km 52 --car 4 --audit-camera 0.60 1.45 6.9 110 -12
capture threshold --route-km 52 --car 4 --audit-camera 0.0 1.5 6.0 90 -35
./build/train-simulator --frames 600 --quality 2 --time-scale 0.000001 \
    --no-audio --no-avatars --hide-hud --config tests/facade-comparison.cfg \
    --route-km 52 --car 4 --report-state >"$output/$prefix-performance-$stage.log" 2>&1
sha256sum ./build/train-simulator >"$output/$prefix-$stage-binary.sha256"
