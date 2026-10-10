#!/usr/bin/env bash
# Matched real-engine ceiling HVAC diffuser views on an isolated display.
set -euo pipefail
cd "$(dirname "$0")/.."
stage=${1:?Specify before or after}
[[ "$stage" == before || "$stage" == after ]] || exit 2
output=build/visual-audit
prefix=${2:-hvac-grille}
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
# Car 2 diffusers sit at (-0.56, -28.8) and (0.56, -34.2) in train-local metres.
capture ceiling --route-km 52 --audit-camera 0.20 2.30 -49 0 55
capture vent-close --route-km 52 --audit-camera 0.56 2.30 -32.9 0 55
capture vent-oblique --route-km 52 --audit-camera -0.3 2.0 -27.4 0 40
capture saloon --route-km 52 --car 2
./build/train-simulator --frames 600 --quality 2 --time-scale 0.000001 \
    --no-audio --no-avatars --hide-hud --config tests/facade-comparison.cfg \
    --route-km 52 --car 2 --report-state >"$output/$prefix-performance-$stage.log" 2>&1
sha256sum ./build/train-simulator >"$output/$prefix-$stage-binary.sha256"
