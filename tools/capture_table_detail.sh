#!/usr/bin/env bash
# Matched high/low passenger viewpoints for tabletop, edge and cup-insert repairs.
set -euo pipefail
cd "$(dirname "$0")/.."
stage=${1:?Specify before or after}
prefix=${2:?Specify a unique repair prefix}
[[ "$stage" == before || "$stage" == after ]] || exit 2
[[ "$prefix" =~ ^[a-z0-9-]+$ ]] || exit 2
output=build/visual-audit
[[ "$stage" != before || ! -e "$output/$prefix-before-binary.sha256" ]] || exit 2
capture() {
    local id=$1
    shift
    ./build/train-simulator --frames 3 --quality 2 --time-scale 0.000001 \
        --no-audio --no-avatars --hide-hud --config tests/facade-comparison.cfg \
        --route-km 52 --report-state --screenshot "$output/$prefix-$id-$stage.png" \
        "$@" >"$output/$prefix-$id-$stage.log" 2>&1
}
capture front --audit-camera 0.15 1.65 -48 0 -18
capture saloon --car 2
capture right --audit-camera 0.10 2.10 -49 65 -35
capture left --audit-camera -0.10 2.10 -49 -65 -35
./build/train-simulator --frames 600 --quality 2 --time-scale 0.000001 \
    --no-audio --no-avatars --hide-hud --config tests/facade-comparison.cfg \
    --route-km 52 --car 2 --report-state >"$output/$prefix-performance-$stage.log" 2>&1
sha256sum ./build/train-simulator >"$output/$prefix-$stage-binary.sha256"
