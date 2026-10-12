#!/usr/bin/env bash
# Second screenshot-based audit baseline (2026-10-12). Run on an isolated display;
# never sends input to a desktop. Usage: capture_visual_audit_pass2.sh [prefix]
set -euo pipefail
cd "$(dirname "$0")/.."
output=build/visual-audit
prefix=${1:-audit2}
[[ "$prefix" =~ ^[a-z][a-z0-9-]*$ ]] || exit 2
if [[ -e "$output/$prefix-binary.sha256" ]]; then
    echo "Refusing to overwrite the $prefix baseline; use a new prefix." >&2
    exit 2
fi
mkdir -p "$output"
sha256sum ./build/train-simulator >"$output/$prefix-binary.sha256"
capture() {
    local id=$1
    shift
    echo "AUDIT_CAPTURE $id $*"
    ./build/train-simulator --frames 3 --quality 2 --time-scale 0.000001 --no-audio \
        --no-avatars --hide-hud --config tests/facade-comparison.cfg --report-state \
        --screenshot "$output/$prefix-$id.png" "$@" >"$output/$prefix-$id.log" 2>&1
}
# Populated frames: avatars need simulated time to load and settle in seats.
populated() {
    local id=$1
    shift
    echo "AUDIT_CAPTURE $id (avatars) $*"
    ./build/train-simulator --frames 240 --quality 2 --time-scale 0.01 --no-audio \
        --hide-hud --config tests/facade-comparison.cfg --report-state \
        --screenshot "$output/$prefix-$id.png" "$@" >"$output/$prefix-$id.log" 2>&1
}

# Interior, every car, plus seated/standing details.
for car in 1 2 3 4 5 6; do
    capture "car-$car-aisle" --car "$car" --route-km 52
done
capture ceiling-up --route-km 52 --audit-camera 0 2.36 -50 0 28
capture table-bay --route-km 52 --audit-camera -0.4 2.5 -46 -60 -35
capture cab-bulkhead --route-km 52 --audit-camera 0 2.36 56 180 0
capture vestibule --vestibule-view --car 3
capture gangway --gangway-view --car 3
capture toilet --toilet-view
capture sink --sink-view
populated populated-car-2 --passenger-view --car 2 --route-km 238
populated populated-car-4 --passenger-view --car 4 --route-km 120
populated populated-platform --station 7 --platform-view --car 3

# Train exterior.
capture train-side --route-km 52 --audit-camera 12 3.0 -44 -90 -2
capture train-front --route-km 52 --audit-camera 9 3.0 -78 -145 -3
capture train-wide --route-km 52 --audit-camera 55 24 15 -70 -20
capture train-roof --route-km 52 --audit-camera 14 12 -20 -90 -35
capture train-underframe --route-km 52 --audit-camera 5 0.9 -36 -100 4

# Every station platform.
for station in 1 2 3 4 5 6 7 8 9 10 11 12; do
    capture "station-$station" --station "$station" --platform-view --car 3
done
capture station-7-forecourt --station 7 --audit-camera -30 14 -20 -110 -15

# Route window views, both sides.
for km in 2 15 30 45 60 100 140 160 200 220 250 270; do
    capture "window-km$km-right" --route-km "$km" --window-view
done
for km in 2 30 140 250; do
    capture "window-km$km-left" --route-km "$km" --left-window-view
done
# Non-round distances separate real repetition from round-kilometre alignment with chunks.
for km in 30.13 101.37 163.81 212.55; do
    capture "window-km$km-right" --route-km "$km" --window-view
done
capture lake --route-km 84 --window-view
capture tunnel-portal --route-km 120.85 --audit-camera 7 4 40 170 -2
capture river-bridge --route-km 74.5 --window-view
capture viaduct --route-km 177.9 --window-view

# Exterior settlement and landscape compositions.
capture town-aerial --route-km 2 --audit-camera 40 35 -40 -90 -25
capture town-street --route-km 2 --audit-camera 22 3 -30 -90 0
capture town-street-left --route-km 2 --audit-camera -22 3 -30 90 0
capture country-aerial --route-km 30 --audit-camera 30 40 0 -90 -30
capture country-ground --route-km 60 --audit-camera 25 2.5 -20 -60 2
echo 'Visual audit pass 2 captures complete. Inspect images before recording findings.'
