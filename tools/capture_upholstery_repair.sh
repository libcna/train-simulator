#!/usr/bin/env bash
# Capture before and after a material-only repair without overwriting other evidence.
set -euo pipefail
cd "$(dirname "$0")/.."
stage=${1:?Specify before or after}
[[ "$stage" == before || "$stage" == after ]] || exit 2
output=build/visual-audit
prefix=${2:-upholstery}
[[ "$prefix" =~ ^[a-z0-9-]+$ ]] || exit 2
if [[ "$stage" == before && -e "$output/$prefix-before-binary.sha256" ]]; then
    echo "Refusing to overwrite the upholstery baseline." >&2
    exit 2
fi
capture() {
    local id=$1
    shift
    ./build/train-simulator --frames 3 --quality 2 --time-scale 0.000001 \
        --no-audio --no-avatars --hide-hud --config tests/facade-comparison.cfg \
        --route-km 52 --report-state \
        --screenshot "$output/$prefix-$id-$stage.png" "$@" \
        >"$output/$prefix-$id-$stage.log" 2>&1
}
capture front --audit-camera 0.15 1.65 -48 0 -18
capture rear --audit-camera 0.1 1.9 -55 180 -10
capture shoulder --audit-camera 0.25 2.0 -49 65 12
capture saloon --car 2
./build/train-simulator --frames 600 --quality 2 --time-scale 0.000001 \
    --no-audio --no-avatars --hide-hud --config tests/facade-comparison.cfg \
    --route-km 52 --car 2 --report-state \
    >"$output/$prefix-performance-$stage.log" 2>&1
sha256sum ./build/train-simulator >"$output/$prefix-$stage-binary.sha256"
