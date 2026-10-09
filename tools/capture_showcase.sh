#!/usr/bin/env bash

set -euo pipefail

repository_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
binary=${1:-"${repository_root}/build/train-simulator"}
screenshot_dir="${repository_root}/docs/screenshots"
config_dir=$(mktemp -d /tmp/train-simulator-showcase.XXXXXX)

cleanup() {
    rm -rf -- "${config_dir}"
}
trap cleanup EXIT

if [[ ! -x "${binary}" ]]; then
    echo "Train Simulator executable not found: ${binary}" >&2
    echo "Build the project first or pass the executable path as the first argument." >&2
    exit 2
fi

mkdir -p "${screenshot_dir}"

capture() {
    local output=$1
    local config=$2
    local frames=$3
    shift 3
    local candidate="${config_dir}/${output}"
    local attempt
    for attempt in 1 2 3 4; do
        rm -f -- "${candidate}"
        if ! SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
                "${binary}" --frames "${frames}" --quality 2 --no-audio \
                --config "${config_dir}/${config}.cfg" \
                --screenshot "${candidate}" "$@"; then
            echo "Showcase capture ${output} failed to run (attempt ${attempt}/4); retrying" >&2
            continue
        fi
        if python3 "${repository_root}/tools/validate_showcase.py" --quiet "${candidate}"; then
            mv -f -- "${candidate}" "${screenshot_dir}/${output}"
            return 0
        fi
        echo "Showcase capture ${output} failed UI validation (attempt ${attempt}/4); retrying" >&2
    done
    echo "Unable to capture a complete ${output} after four attempts" >&2
    return 1
}

# The requested frame selects a settled NPC arrangement. The application freezes that state and
# finishes every serialized CNA avatar upload before it reads the framebuffer, so the PNG cannot
# photograph an otherwise valid saloon or platform before its quality-sized population is ready.
capture passenger-saloon.png passenger-saloon 70 --car 2
capture passenger-information.png passenger-information 2 --route-km 133.4 --direction 1 --inspect-display
capture lakehurst-window.png lakehurst-window 2 --route-km 84 --car 4 --window-view
capture stonebridge-platform.png stonebridge-platform 90 --station 7 --platform-view
capture route-map.png route-map 2 --station 7 --map-overlay

echo "Showcase screenshots written to ${screenshot_dir}"
