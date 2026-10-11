#!/usr/bin/env bash
# Isolated X11 event-delivery probe, not a physical mouse or desktop-compositor certification.
set -euo pipefail
cd "$(dirname "$0")/.."
backend=${INPUT_PROBE_BACKEND:-xvfb}
quality=${INPUT_PROBE_QUALITY:-1}
if [[ ! "$backend" =~ ^(xvfb|xwayland)$ || ! "$quality" =~ ^[012]$ ]]; then
    echo 'INPUT_PROBE_BACKEND must be xvfb or xwayland; INPUT_PROBE_QUALITY must be 0–2.' >&2
    exit 2
fi
# The dependency runner is inspected and used read-only. Never reuse an arbitrary caller DISPLAY.
if [[ "$backend" == xwayland && "${TRAINSIM_PRIVATE_INPUT_PROBE:-0}" != 1 ]]; then
    exec ../cna/tools/platform/run_gpu_tests_private.sh --exec \
        env TRAINSIM_PRIVATE_INPUT_PROBE=1 bash tools/test_x11_input.sh
fi
if [[ "$backend" == xwayland ]]; then
    [[ "${CNA_WAYLAND_TEST_COMPOSITOR:-}" == weston
       && "${WAYLAND_DISPLAY:-}" == cna-weston-*
       && "${WAYLAND_DISPLAY:-}" == "${CNA_WAYLAND_TEST_DISPLAY:-}"
       && "${XDG_RUNTIME_DIR:-}" == /tmp/cna-wl-*/run
       && -S "$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY"
       && "${DISPLAY:-}" =~ ^:[0-9]+$ ]] || {
        echo 'Refusing an input probe outside the private Weston runner.' >&2; exit 2;
    }
else
    command -v Xvfb >/dev/null
fi
for program in xdotool xmessage timeout import awk; do command -v "$program" >/dev/null; done
output=${INPUT_PROBE_OUTPUT:-build/x11-input-test}
[[ "$output" =~ ^build/[a-zA-Z0-9_/-]+$ ]] || {
    echo 'INPUT_PROBE_OUTPUT must be a relative build/ evidence directory.' >&2; exit 2;
}
phases=${INPUT_PROBE_PHASES:-3}
avatars=${INPUT_PROBE_AVATARS:-0}
if [[ ! "$phases" =~ ^([3-9]|[1-9][0-9]|1[0-7][0-9]|180)$ || ! "$avatars" =~ ^[01]$ ]]; then
    echo 'INPUT_PROBE_PHASES must be 3–180; INPUT_PROBE_AVATARS must be 0 or 1.' >&2
    exit 2
fi
avatar_args=(--no-avatars)
if ((avatars)); then avatar_args=(); fi
probe_dir=$(mktemp -d /tmp/train-simulator-x11.XXXXXX)
mkdir -p "${output}"
printf 'status=running phases=%s avatars=%s backend=%s quality=%s\n' \
    "$phases" "$avatars" "$backend" "$quality" >"${output}"/result.txt
xserver_pid=
simulator_pid=
focus_pid=
cleanup() {
    local probe_status=$?
    if [[ -n "$focus_pid" ]]; then kill "$focus_pid" 2>/dev/null || true; wait "$focus_pid" 2>/dev/null || true; fi
    if [[ -n "$simulator_pid" ]]; then kill "$simulator_pid" 2>/dev/null || true; wait "$simulator_pid" 2>/dev/null || true; fi
    if [[ -n "$xserver_pid" ]]; then kill "$xserver_pid" 2>/dev/null || true; wait "$xserver_pid" 2>/dev/null || true; fi
    # Only known scratch files owned by this invocation; keep test evidence in build/.
    rm -f "$probe_dir/display" "$probe_dir/xvfb.log" "$probe_dir/settings.cfg"
    rmdir "$probe_dir" 2>/dev/null || true
    printf 'exit_status=%s phases=%s avatars=%s backend=%s quality=%s\n' \
        "$probe_status" "$phases" "$avatars" "$backend" "$quality" >"${output}"/result.txt
}
trap cleanup EXIT
if [[ "$backend" == xvfb ]]; then
Xvfb -displayfd 3 -screen 0 1280x720x24 -nolisten tcp 3>"$probe_dir/display" >"$probe_dir/xvfb.log" 2>&1 &
xserver_pid=$!
for ((attempt=0;attempt<100;++attempt)); do
    [[ -s "$probe_dir/display" ]] && break
    kill -0 "$xserver_pid"
    sleep 0.1
done
display_number=$(<"$probe_dir/display")
[[ "$display_number" =~ ^[0-9]+$ ]]
export DISPLAY=":$display_number"
fi
if command -v glxinfo >/dev/null; then
    glxinfo -B >"${output}"/renderer.txt 2>&1
fi
export SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy LP_NUM_THREADS=4
unset WAYLAND_DISPLAY
timeout "$((60+phases*30))" ./build/train-simulator --window-view --route-km 52 --quality "$quality" --no-audio \
    "${avatar_args[@]}" --report-state --config "$probe_dir/settings.cfg" >"${output}"/game.log 2>&1 &
simulator_pid=$!
window=
for ((attempt=0;attempt<100;++attempt)); do
    window=$(xdotool search --onlyvisible --name '^Northern Valley Express - passenger service$' | head -1 || true)
    [[ -n "$window" ]] && break
    kill -0 "$simulator_pid"
    sleep 0.1
done
[[ "$window" =~ ^[0-9]+$ ]]
xdotool windowmove "$window" 0 0
xdotool windowsize "$window" 1280 720
xdotool windowfocus --sync "$window"
press_key() {
    xdotool keydown "$1"
    sleep 0.18
    xdotool keyup "$1"
    sleep 0.2
}
capture_mouse() {
    xdotool mousemove --window "$window" 640 360 mousedown 1
    sleep 0.25
    xdotool mouseup 1
    sleep 0.3
}
sleep 2
# Interactive launches now open on the start menu. Enter uses its supported
# keyboard action; a click at the screen centre sits above the Start button.
press_key Return
capture_mouse
# Do not send a whole motion sequence to an uncaptured window and call that a look test.
for ((attempt=0;attempt<14;++attempt)); do
    if awk '/^INPUT_TRACE/ { captured=($0 ~ /captured=1 native=1/); } END { exit !captured; }' "${output}"/game.log; then break; fi
    capture_mouse
    sleep 0.5
done
awk '/^INPUT_TRACE/ { captured=($0 ~ /captured=1 native=1/); } END { exit !captured; }' "${output}"/game.log
import -window "$window" "${output}"/before.png
motion_samples() {
    awk '/^INPUT_TRACE/ { for(i=1;i<=NF;i++) if($i ~ /^motion_samples=/) { split($i,a,"="); n=a[2]; } } END { print n+0; }' "${output}"/game.log
}
for ((phase=0;phase<phases;++phase)); do
    before_motion=$(motion_samples)
    before_records=$(awk '/^INPUT_TRACE/ { n++ } END { print n+0 }' "${output}"/game.log)
    for ((step=0;step<200;++step)); do
        xdotool mousemove_relative -- 6 0
        sleep 0.05
    done
    after_motion=$(motion_samples)
    if ((after_motion <= before_motion)); then echo "No motion progress in phase $phase" >&2; exit 1; fi
    # Receiving events alone is insufficient: a frozen controller can still count them.
    # Require actual yaw travel in newly emitted traces during every motion phase.
    awk -v before="$before_records" '
    /^INPUT_TRACE/ {
        n++; yaw=0; samples=0; active=($0 ~ /captured=1 native=1 paused=0 map=0/);
        for(i=1;i<=NF;i++) {
            split($i,a,"=");
            if(a[1]=="yaw_rad") yaw=a[2]+0;
            if(a[1]=="motion_samples") samples=a[2]+0;
        }
        if(n>before && active && samples>lastSamples) {
            delta=yaw-lastYaw;
            while(delta>3.141593) delta-=6.283186;
            while(delta< -3.141593) delta+=6.283186;
            travel+=(delta<0 ? -delta : delta);
            records++;
        }
        lastYaw=yaw; lastSamples=samples;
    }
    END { if(records<1 || travel<0.5) exit 1; }
    ' "${output}"/game.log || { echo "Camera yaw did not advance in phase $phase" >&2; exit 1; }
    echo "INPUT_PHASE passed phase=$phase motion_before=$before_motion motion_after=$after_motion"
    import -window "$window" "${output}/phase-$phase.png"
    if ((phase%3 == 0)); then
        press_key Escape
        sleep 6
        press_key Escape
    elif ((phase%3 == 1)); then
        press_key m
        sleep 6
        press_key m
    else
        xmessage -title TrainSimInputFocusProbe -buttons OK 'Private focus test target' >/dev/null 2>&1 &
        focus_pid=$!
        focus_window=
        for ((attempt=0;attempt<30;++attempt)); do
            focus_window=$(xdotool search --onlyvisible --name '^TrainSimInputFocusProbe$' | head -1 || true)
            [[ -n "$focus_window" ]] && break
            sleep 0.1
        done
        [[ "$focus_window" =~ ^[0-9]+$ ]]
        xdotool windowfocus --sync "$focus_window"
        sleep 6
        xdotool windowfocus --sync "$window"
        kill "$focus_pid"
        wait "$focus_pid" 2>/dev/null || true
        focus_pid=
    fi
    capture_mouse
done
press_key e
xdotool keydown w
sleep 2
xdotool keyup w
press_key F12
wait "$simulator_pid"
simulator_pid=
awk -v phases="$phases" '
/^INPUT_TRACE/ {
    captured=0; samples=0; fields=0; frame=-1; interval=-1;
    for (i=1;i<=NF;i++) {
        if ($i=="captured=1") captured=1;
        if ($i ~ /^motion_samples=/) { split($i,a,"="); samples=a[2]+0; }
        if ($i ~ /^help=[01]$/ || $i ~ /^display=[01]$/) fields++;
        if ($i ~ /^frames=/) { split($i,a,"="); frame=a[2]+0; fields++; }
        if ($i ~ /^trace_interval_s=/) { split($i,a,"="); interval=a[2]+0; fields++; }
    }
    if (fields!=4 || frame<0 || (records && frame<=previousFrame) ||
        (!records && interval!=0) || (records && interval<5)) invalidTrace=1;
    records++; previousFrame=frame;
    if (captured && samples>previous) advances++;
    previous=samples;
    print;
}
/^FINAL_STATE/ {
    for (i=1;i<=NF;i++) {
        split($i,a,"=");
        if (a[1]=="mouse_mode_acquires") acquisitions=a[2]+0;
        if (a[1]=="mouse_mode_losses") losses=a[2]+0;
        if (a[1]=="stand_transitions") standing=a[2]+0;
        if (a[1]=="seated") seated=a[2]+0;
        if (a[1]=="world_rebuilds") rebuilds=a[2]+0;
        if (a[1]=="loaded_chunks") chunks=a[2]+0;
        if (a[1]=="route_delta_m") distance=a[2]+0;
    }
    final=1;
}
END { if (invalidTrace || advances<phases+2 || previous<100 || !final || acquisitions<phases+1 || losses!=int(phases/3) || standing<1 || seated || rebuilds<3 || chunks!=7 || distance<500) exit 1; }
' "${output}"/game.log
echo "X11_INPUT_PROBE passed: $phases phases, avatars=$avatars, backend=$backend, quality=$quality, per-phase camera yaw and streaming, pause/map/focus recovery; inspect captures separately."
