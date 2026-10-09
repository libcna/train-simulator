#!/usr/bin/env bash
# Fixed engine evidence for trunk scaling and branch continuity across route contexts.
set -euo pipefail
cd "$(dirname "$0")/.."
stage=${1:?Specify before or after}
[[ "$stage" == before || "$stage" == after ]] || exit 2
output=build/visual-audit
prefix=${2:-tree-continuity}
[[ "$prefix" =~ ^[a-z][a-z0-9-]*$ ]] || exit 2
[[ "$stage" != before || ! -e "$output/$prefix-before-binary.sha256" ]] || exit 2
capture() {
    local id=$1
    shift
    ./build/train-simulator --frames 3 --quality 2 --time-scale 0.000001 \
        --no-audio --no-avatars --hide-hud --config tests/facade-comparison.cfg \
        --report-state --screenshot "$output/$prefix-$id-$stage.png" \
        "$@" >"$output/$prefix-$id-$stage.log" 2>&1
}
capture urban --route-km 2 --left-window-view
capture country --route-km 52 --left-window-view
capture oblique --route-km 52 --audit-camera 0.25 2.0 -49 65 12
capture lake --route-km 84 --window-view
if [[ "$prefix" == field-margins || "$prefix" == woodland-groups || "$prefix" == grounded-hills || "$prefix" == cloud-variety ]]; then
    capture river --route-km 74.5 --window-view
    capture viaduct --route-km 177.9 --window-view
fi
./build/train-simulator --frames 600 --quality 2 --time-scale 0.000001 \
    --no-audio --no-avatars --hide-hud --config tests/facade-comparison.cfg \
    --route-km 52 --car 2 --report-state >"$output/$prefix-performance-$stage.log" 2>&1
sha256sum ./build/train-simulator >"$output/$prefix-$stage-binary.sha256"
