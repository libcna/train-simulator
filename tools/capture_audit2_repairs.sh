#!/usr/bin/env bash
# Matched real-engine views for the pass-2 audit repairs (ENV-124 onward) on an isolated
# display. Usage: capture_audit2_repairs.sh <stage> [prefix]. The `before` stage is preserved;
# later stages (e.g. `after`, `after-verge`) may be repeated. Ends with a 600-frame stationary
# window sample and a 1,800-frame moving ride at 8x journey time from km 52.
set -euo pipefail
cd "$(dirname "$0")/.."
stage=${1:?Specify a stage such as before or after}
[[ "$stage" =~ ^[a-z][a-z0-9-]*$ ]] || exit 2
output=build/visual-audit
prefix=${2:-audit2-repairs}
[[ "$prefix" =~ ^[a-z][a-z0-9-]*$ ]] || exit 2
[[ "$stage" != before || ! -e "$output/$prefix-before-binary.sha256" ]] || exit 2
mkdir -p "$output"
sha256sum ./build/train-simulator >"$output/$prefix-$stage-binary.sha256"
capture() {
    local id=$1
    shift
    ./build/train-simulator --frames 3 --quality 2 --time-scale 0.000001 \
        --no-audio --no-avatars --hide-hud --config tests/facade-comparison.cfg \
        --report-state --screenshot "$output/$prefix-$id-$stage.png" \
        "$@" >"$output/$prefix-$id-$stage.log" 2>&1
}
# ENV-124 / 128 / 130: rural verge, field boundaries and parcels.
capture verge-km45 --route-km 45 --window-view
capture verge-km101 --route-km 101.37 --window-view
capture verge-km212 --route-km 212.55 --window-view
capture verge-close --route-km 52 --audit-camera 3.2 2.6 -40 60 -18
capture country-aerial --route-km 30 --audit-camera 30 40 0 -90 -30
capture country-wide --route-km 52 --audit-camera 55 24 15 -70 -20
# ENV-125 / 129 / 132: urban lineside, apartment blocks and sheds.
capture urban-right --route-km 2 --window-view
capture urban-left --route-km 2 --left-window-view
capture urban-km163 --route-km 163.81 --window-view
capture town-aerial --route-km 2 --audit-camera 40 35 -40 -90 -25
capture forecourt --station 7 --audit-camera -30 14 -20 -110 -15
# ENV-126: fixed interior signage.
capture gangway --gangway-view --car 3
capture vestibule --vestibule-view --car 3
capture cab-end --car 6 --route-km 52
capture sink --sink-view
# ENV-127: farmsteads.
capture farm-km60 --route-km 60 --window-view
capture farm-km100 --route-km 100 --window-view
capture farm-km140 --route-km 140 --left-window-view
capture farm-km160 --route-km 160 --window-view
# ENV-131: station canopies across families.
for station in 1 4 5 7 9 11; do
    capture "station-$station" --station "$station" --platform-view --car 3
done
# ENV-133: driving cab exterior.
capture cab-front --route-km 52 --audit-camera 9 3.0 -78 -145 -3
capture cab-side --route-km 52 --audit-camera 7 2.6 -66 -100 -2
# Supplementary poses added after the before stage (no matched before image exist for these).
if [[ "$stage" != before ]]; then
    capture cab-rear --route-km 52 --audit-camera -7 2.6 66 80 -2
    capture canopy-rural --station 4 --audit-camera 5 2.4 10 0 12
    capture canopy-waterfront --station 5 --audit-camera -6.5 2.6 -45 180 6
    capture tramlines --route-km 52 --audit-camera 55 24 15 -70 -20
    capture verge-flowers --route-km 45 --audit-camera 6.5 1.6 -40 90 -12
fi
# Stationary and moving performance on this binary.
./build/train-simulator --frames 600 --quality 2 --time-scale 0.000001 --no-audio --no-avatars \
    --hide-hud --config tests/facade-comparison.cfg --route-km 52 --window-view --report-state \
    >"$output/$prefix-performance-$stage.log" 2>&1
/usr/bin/time -v ./build/train-simulator --frames 1800 --quality 2 --time-scale 8 --no-audio --no-avatars \
    --hide-hud --config tests/facade-comparison.cfg --route-km 52 --car 2 --window-view \
    --report-state --screenshot "$output/$prefix-moving-$stage.png" \
    >"$output/$prefix-moving-$stage.log" 2>&1
echo "Captured $prefix $stage"
