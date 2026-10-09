#!/usr/bin/env bash
# Reusable matched evidence for window repairs; use an isolated display.
set -euo pipefail
cd "$(dirname "$0")/.."
stage=${1:?Specify before or after}
prefix=${2:?Specify a unique repair prefix}
[[ "$stage" == before || "$stage" == after ]] || exit 2
[[ "$prefix" =~ ^[a-z0-9-]+$ ]] || exit 2
output=build/visual-audit
if [[ "$stage" == before && -e "$output/$prefix-before-binary.sha256" ]]; then
    echo "Refusing to overwrite the repair baseline." >&2
    exit 2
fi
capture() {
    local id=$1
    shift
    ./build/train-simulator --frames 3 --quality 2 --time-scale 0.000001 \
        --no-audio --no-avatars --hide-hud --config tests/facade-comparison.cfg \
        --route-km 52 --report-state --screenshot "$output/$prefix-$id-$stage.png" \
        "$@" >"$output/$prefix-$id-$stage.log" 2>&1
}
capture corner --audit-camera 0.90 2.35 -49 90 25
capture oblique --audit-camera 0.25 2.0 -49 65 12
capture opposite --audit-camera -0.25 2.0 -49 -65 12
capture saloon --car 2
./build/train-simulator --frames 600 --quality 2 --time-scale 0.000001 \
    --no-audio --no-avatars --hide-hud --config tests/facade-comparison.cfg \
    --route-km 52 --car 2 --report-state >"$output/$prefix-performance-$stage.log" 2>&1
sha256sum ./build/train-simulator >"$output/$prefix-$stage-binary.sha256"
