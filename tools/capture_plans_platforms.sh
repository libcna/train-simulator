#!/usr/bin/env bash
# Building floor plans (ENV-134) and platform life and furniture (ENV-136): window and aerial
# views of varied house and block plans, and populated platform views of each station family.
# Populated captures run 300 frames so the CNA avatar loader can finish.
set -euo pipefail
cd "$(dirname "$0")/.."
stage=${1:?Specify before or after}
[[ "$stage" == before || "$stage" == after ]] || exit 2
output=build/visual-audit
prefix=${2:-plans-platforms}
[[ "$prefix" =~ ^[a-z][a-z0-9-]*$ ]] || exit 2
[[ "$stage" != before || ! -e "$output/$prefix-before-binary.sha256" ]] || exit 2
mkdir -p "$output"
capture() {
    local id=$1 frames=$2
    shift 2
    ./build/train-simulator --frames "$frames" --quality 2 --time-scale 0.01 \
        --no-audio --hide-hud --config tests/facade-comparison.cfg \
        --report-state --screenshot "$output/$prefix-$id-$stage.png" \
        "$@" >"$output/$prefix-$id-$stage.log" 2>&1
}
capture plans-km2-left 3 --no-avatars --route-km 2 --car 4 --left-window-view
capture plans-km2-right 3 --no-avatars --route-km 2 --car 4 --window-view
capture plans-km60 3 --no-avatars --route-km 60 --car 2 --window-view
capture plans-km163 3 --no-avatars --route-km 163.81 --car 2 --window-view
capture plans-town-aerial 3 --no-avatars --route-km 2 --audit-camera 40 30 0 -60 -25
capture plans-country-aerial 3 --no-avatars --route-km 30 --audit-camera 40 30 0 -60 -25
for station in 4 5 9 12; do
    capture "platform-station-$station" 300 --station "$station" --platform-view
done
./build/train-simulator --frames 600 --quality 2 --time-scale 0.01 --no-audio --hide-hud \
    --config tests/facade-comparison.cfg --station 5 --platform-view --report-state \
    >"$output/$prefix-performance-$stage.log" 2>&1
sha256sum ./build/train-simulator >"$output/$prefix-$stage-binary.sha256"
