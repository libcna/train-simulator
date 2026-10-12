#!/usr/bin/env bash
# Real-engine views of each oncoming consist type (ENV-137) and of the parked diesel unit.
# Consist centres are the deterministic t=0 positions of the first service of each type; the
# player train is held 300 m ahead so its own cars do not hide the parallel track.
set -euo pipefail
cd "$(dirname "$0")/.."
stage=${1:?Specify before or after}
[[ "$stage" == before || "$stage" == after ]] || exit 2
output=build/visual-audit
prefix=${2:-oncoming}
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
# type centre-metres half-length
for spec in "freight 37292 100" "emu 9542 28.5" "intercity 18180 76" "dmu 28780 23"; do
    read -r type centre half <<<"$spec"
    km=$(awk -v c="$centre" 'BEGIN { printf "%.3f", (c+300)/1000 }')
    front=$(awk -v h="$half" 'BEGIN { printf "%.1f", -300-h-14 }')
    rear=$(awk -v h="$half" 'BEGIN { printf "%.1f", -300+h+8 }')
    lead=$(awk -v h="$half" 'BEGIN { printf "%.1f", -300-h+12 }')
    high=$(awk -v h="$half" 'BEGIN { printf "%.1f", -300+h+45 }')
    capture "$type-front" --route-km "$km" --audit-camera 4 2.8 "$front" -150 -2
    capture "$type-side" --route-km "$km" --audit-camera 6.5 2.4 "$lead" -90 0
    capture "$type-mid" --route-km "$km" --audit-camera 7.5 2.6 -300 -90 0
    capture "$type-rear" --route-km "$km" --audit-camera 4 2.8 "$rear" -42 -2
    capture "$type-high" --route-km "$km" --audit-camera 22 16 "$high" -25 -18
    capture "$type-window" --route-km "$(awk -v c="$centre" 'BEGIN { printf "%.3f", c/1000 }')" \
        --car 2 --left-window-view
done
capture parked-dmu --station 8 --audit-camera 14 9 40 -110 -12
./build/train-simulator --frames 600 --quality 2 --time-scale 0.000001 \
    --no-audio --no-avatars --hide-hud --config tests/facade-comparison.cfg \
    --route-km 37.292 --car 2 --left-window-view --report-state \
    >"$output/$prefix-performance-$stage.log" 2>&1
sha256sum ./build/train-simulator >"$output/$prefix-$stage-binary.sha256"
