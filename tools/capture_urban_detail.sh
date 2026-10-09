#!/usr/bin/env bash
# Matched passenger views and city-specific performance evidence, private display only.
set -euo pipefail
cd "$(dirname "$0")/.."
stage=${1:?Specify before or after}
prefix=${2:-urban-frontage}
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
capture urban-left --route-km 2 --left-window-view
capture urban-right --route-km 2 --window-view
capture city-left --route-km 191 --left-window-view
capture city-right --route-km 191 --window-view
capture lake --route-km 84 --window-view
if [[ "$prefix" == residential-glass ]]; then
    capture facade-close --route-km 2 --audit-camera -15 3 10 -90 8
    capture facade-oblique --route-km 2 --audit-camera -12 5 -5 -65 8
fi
if [[ "$prefix" == roof-materials || "$prefix" == eave-drainage ]]; then
    capture roof-close --route-km 84 --audit-camera 15 10 2 75 -12
    capture roof-oblique --route-km 2 --audit-camera -15 17 10 -90 -12
fi
if [[ "$prefix" == eave-drainage ]]; then
    capture eave-close --route-km 84 --audit-camera 25 5 -2 90 12
fi
if [[ "$prefix" == entrance-doors || "$prefix" == street-surfaces || "$prefix" == access-gardens ]]; then
    capture townhouse-close --route-km 2 --audit-camera -15 1.5 10 -90 0
    capture house-close --route-km 84 --audit-camera 20 1.6 2 75 0
fi
if [[ "$prefix" == access-gardens ]]; then
    capture access-oblique --route-km 2 --audit-camera -12 5 -5 -65 8
fi
if [[ "$prefix" == road-cars ]]; then
    capture road-close --route-km 2 --audit-camera -10 1.0 11.3 -90 -3
fi
./build/train-simulator --frames 600 --quality 2 --time-scale 0.000001 \
    --no-audio --no-avatars --hide-hud --config tests/facade-comparison.cfg \
    --route-km 2 --car 2 --report-state >"$output/$prefix-performance-$stage.log" 2>&1
sha256sum ./build/train-simulator >"$output/$prefix-$stage-binary.sha256"
