#!/usr/bin/env bash
# Run on an isolated display or SDL offscreen; never sends input to a desktop.
set -euo pipefail
cd "$(dirname "$0")/.."
binary=${1:-./build/train-simulator}
output=build/visual-audit
if [[ -e "$output/binary.sha256" ]]; then
    echo "Refusing to overwrite the visual audit baseline; capture repairs under new filenames." >&2
    exit 2
fi
mkdir -p "$output"
sha256sum "$binary" >"$output/binary.sha256"
capture() {
    local id=$1
    shift
    echo "AUDIT_CAPTURE $id $*"
    "$binary" --frames 3 --quality 2 --time-scale 0.000001 --no-audio --no-avatars \
        --hide-hud --config tests/facade-comparison.cfg --report-state \
        --screenshot "$output/$id.png" "$@" >"$output/$id.log" 2>&1
}
for car in 1 2 3 4 5 6; do
    capture "car-$car-aisle" --car "$car" --route-km 52
done
capture seat-front-close --route-km 52 --audit-camera 0.15 1.65 -48 0 -18
capture seat-rear-close --route-km 52 --audit-camera 0.1 1.9 -55 180 -10
capture window-oblique --route-km 52 --audit-camera 0.25 2.0 -49 65 12
capture window-corner --route-km 52 --audit-camera 0.90 2.35 -49 90 25
capture window-outside --route-km 52 --audit-camera 3.8 2.4 -49 -90 0
capture train-side --route-km 52 --audit-camera 12 3.0 -44 -90 -2
capture train-front --route-km 52 --audit-camera 9 3.0 -78 -145 -3
capture train-wide --route-km 52 --audit-camera 55 24 15 -70 -20
capture toilet --toilet-view
capture sink --sink-view
capture gangway --gangway-view --car 3
capture vestibule --vestibule-view --car 3
capture doors-platform --station 7 --door-view
capture terminal-platform --station 1 --platform-view --car 3
capture city-platform --station 7 --platform-view --car 3
capture rural-platform --station 4 --platform-view --car 3
capture waterfront-platform --station 5 --platform-view
capture terminal-wide --station 1 --audit-camera 25 15 -20 -65 -18
capture urban-right --route-km 2 --window-view
capture urban-left --route-km 2 --left-window-view
capture country-near --route-km 52 --left-window-view
capture lake --route-km 84 --window-view
capture tunnel --route-km 123 --window-view
capture river-bridge --route-km 74.5 --window-view
capture viaduct --route-km 177.9 --window-view
capture signal --signal-view
capture track-close --route-km 52 --audit-camera 4 2.2 -40 -35 -32
capture passing-train --route-km 9.55 --car 1 --left-window-view
echo 'Visual audit captures complete. Inspect images before recording findings.'
