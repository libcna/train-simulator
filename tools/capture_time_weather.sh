#!/usr/bin/env bash
# Real-engine views of the time-of-day and weather states (ENV-138/139): a matched set of poses
# under morning, midday, golden hour, dusk and night clear skies, and under every weather at
# midday, plus a 600-frame performance sample for night and rain.
set -euo pipefail
cd "$(dirname "$0")/.."
stage=${1:?Specify before or after}
[[ "$stage" == before || "$stage" == after ]] || exit 2
output=build/visual-audit
prefix=${2:-time-weather}
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
poses() {
    local state=$1
    shift
    capture "$state-window" --route-km 52 --car 2 --window-view "$@"
    capture "$state-exterior" --route-km 52 --audit-camera 55 24 15 -70 -20 "$@"
    capture "$state-station" --station 9 --platform-view "$@"
    capture "$state-urban" --route-km 2 --car 4 --window-view "$@"
    capture "$state-freight" --route-km 37.292 --car 2 --left-window-view "$@"
}
for state in "morning 06:50" "midday 13:00" "golden 20:45" "dusk 21:35" "night 23:30"; do
    read -r name clock <<<"$state"
    poses "$name" --time "$clock" --weather clear
done
for weather in cloudy overcast rain fog; do
    poses "$weather" --time 13:00 --weather "$weather"
done
poses "night-rain" --time 23:30 --weather rain
for run in "night 23:30 clear" "rain 13:00 rain"; do
    read -r name clock weather <<<"$run"
    ./build/train-simulator --frames 600 --quality 2 --time-scale 0.000001 \
        --no-audio --no-avatars --hide-hud --config tests/facade-comparison.cfg \
        --route-km 52 --car 1 --time "$clock" --weather "$weather" --report-state \
        >"$output/$prefix-performance-$name-$stage.log" 2>&1
done
sha256sum ./build/train-simulator >"$output/$prefix-$stage-binary.sha256"
