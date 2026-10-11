#!/usr/bin/env bash
# Exercise prediction invalidation through real application settings and station keys.
# Uses only a private Weston/Xwayland; never sends events to the owner's desktop.
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ ${TRAINSIM_PRIVATE_STREAM_PROBE:-0} != 1 ]]; then
    exec ../cna/tools/platform/run_gpu_tests_private.sh --exec \
        env TRAINSIM_PRIVATE_STREAM_PROBE=1 bash tools/test_world_prefetch_input.sh
fi
[[ ${CNA_WAYLAND_TEST_COMPOSITOR:-} == weston \
   && ${WAYLAND_DISPLAY:-} == cna-weston-* \
   && ${WAYLAND_DISPLAY:-} == "${CNA_WAYLAND_TEST_DISPLAY:-}" \
   && ${XDG_RUNTIME_DIR:-} == /tmp/cna-wl-*/run \
   && -S "$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY" \
   && ${DISPLAY:-} =~ ^:[0-9]+$ ]] || exit 2
for program in xdotool timeout import python3; do command -v "$program" >/dev/null; done
output=build/visual-audit
mkdir -p "$output"
config=$(mktemp /tmp/trainsim-stream-settings.XXXXXX)
pid=
cleanup() {
    if [[ -n "$pid" ]]; then kill "$pid" 2>/dev/null || true; wait "$pid" 2>/dev/null || true; fi
    rm -f "$config"
}
trap cleanup EXIT
export SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy LP_NUM_THREADS=4
timeout 60 ./build/train-simulator --run-seconds 45 --route-km 52 --time-scale 8 \
    --quality 2 --window-view --no-audio --no-avatars --report-state --config "$config" \
    >"$output/streaming-invalidation.log" 2>&1 &
pid=$!
window=
for ((attempt=0;attempt<100;++attempt)); do
    window=$(xdotool search --onlyvisible --name '^Northern Valley Express - passenger service$' | head -1 || true)
    [[ -n "$window" ]] && break
    kill -0 "$pid"
    sleep 0.1
done
[[ "$window" =~ ^[0-9]+$ ]]
xdotool windowfocus --sync "$window"
press() {
    xdotool keydown "$1"
    sleep 0.15
    xdotool keyup "$1"
    sleep 0.2
}
sleep 2
press F7
press F6
press Escape
for ((i=0;i<3;++i)); do press Down; done
press Left
press Left
press Right
press Right
for ((i=0;i<5;++i)); do press Down; done
press Right
press Left
import -window "$window" "$output/streaming-invalidation-settings.png"
press Escape
press F7
press F7
press F6
sleep 2
import -window "$window" "$output/streaming-invalidation-world.png"
press F12
wait "$pid"
pid=
python3 - "$output/streaming-invalidation.log" <<'PY'
import sys
from pathlib import Path
lines=[line for line in Path(sys.argv[1]).read_text().splitlines() if line.startswith('FINAL_STATE ')]
assert len(lines)==1, 'Expected one clean final state'
s=dict(field.split('=',1) for field in lines[0].split()[1:])
assert s['graphics_quality']=='2' and s['paused']=='0', 'Settings sequence did not complete'
assert s['loaded_chunks']=='7', 'World window lost chunks after teleport'
assert int(s['world_synchronous_builds'])>=12, 'Settings/teleports did not invalidate prepared geometry'
assert int(s['world_rebuilds'])>=int(s['world_synchronous_builds']), 'Unpublished rebuild'
print('WORLD_PREFETCH_INPUT passed: quality/range changes and repeated station jumps')
PY
