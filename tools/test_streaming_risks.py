#!/usr/bin/env python3
"""PERF-002: moving streaming scenarios with per-process GPU memory sampling.

Runs fixed-frame moving rides on the private GPU display and records the streaming counters,
steady frame percentiles, peak RSS and the process's own amdgpu memory from /proc fdinfo
(drm-memory-vram / drm-memory-gtt). Nothing touches the owner's desktop.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
os.chdir(ROOT)

SCENARIOS = {
    # name: (route km, time scale, frames, extra arguments, cpu list for taskset or None)
    'x8-km52': (52.0, 8.0, 1800, [], None),
    'x16-km52': (52.0, 16.0, 1800, [], None),
    'x24-km52': (52.0, 24.0, 1800, [], None),
    'x8-city': (186.0, 8.0, 1800, [], None),
    'x1-city': (193.0, 1.0, 3600, [], None),
    'x8-km52-two-cores': (52.0, 8.0, 1800, [], '0,1'),
}


def fields(line: str) -> dict[str, str]:
    return dict(re.findall(r'(\w+)=(.*?)(?=\s+\w+=|$)', line.strip()))


def gpu_memory_kib(pid: int) -> dict[str, int]:
    """Sum amdgpu client memory over this process's DRM file descriptors."""
    totals = {'vram': 0, 'gtt': 0}
    seen = set()
    fdinfo = Path('/proc') / str(pid) / 'fdinfo'
    try:
        entries = list(fdinfo.iterdir())
    except OSError:
        return totals
    for entry in entries:
        try:
            text = entry.read_text()
        except OSError:
            continue
        if 'drm-driver' not in text:
            continue
        client = re.search(r'drm-client-id:\s*(\d+)', text)
        if client and client.group(1) in seen:
            continue
        if client:
            seen.add(client.group(1))
        for kind in totals:
            match = re.search(rf'drm-memory-{kind}:\s*(\d+)\s*KiB', text)
            if match:
                totals[kind] += int(match.group(1))
    return totals


def run(name: str, binary: Path, output: Path, prefix: str, interval: float) -> dict:
    km, scale, frames, extra, cpus = SCENARIOS[name]
    command = [str(binary), '--frames', str(frames), '--route-km', str(km), '--time-scale',
               str(scale), '--window-view', '--car', '2', '--quality', '2', '--no-audio',
               '--no-avatars', '--report-state', '--config', 'tests/facade-comparison.cfg',
               *extra]
    if cpus:
        command = ['taskset', '-c', cpus, *command]
    log = output / f'{prefix}-{name}.log'
    peak = {'vram': 0, 'gtt': 0, 'rss': 0}
    samples = []
    with log.open('w') as handle:
        process = subprocess.Popen(['/usr/bin/time', '-v', *command], stdout=handle,
                                   stderr=subprocess.STDOUT)
        started = time.monotonic()
        child = None
        while process.poll() is None:
            if child is None:
                try:
                    # /usr/bin/time forks the simulator; taskset execs it in place.
                    children = Path(f'/proc/{process.pid}/task/{process.pid}/children').read_text().split()
                    if children:
                        child = int(children[0])
                except OSError:
                    pass
            if child is not None:
                memory = gpu_memory_kib(child)
                try:
                    rss = int(re.search(r'VmRSS:\s*(\d+)', Path(f'/proc/{child}/status').read_text()).group(1))
                except (OSError, AttributeError):
                    rss = 0
                samples.append({'t': round(time.monotonic()-started, 1), **memory, 'rss': rss})
                for key in peak:
                    peak[key] = max(peak[key], samples[-1][key])
            time.sleep(interval)
        exit_code = process.wait()
    text = log.read_text()
    final = next((fields(line[len('FINAL_STATE '):]) for line in text.splitlines()
                  if line.startswith('FINAL_STATE ')), {})
    keys = ['route_km', 'steady_frame_samples', 'steady_frame_mean_ms', 'steady_frame_p95_ms',
            'steady_frame_p99_ms', 'steady_frame_max_ms', 'world_prefetch_hits',
            'world_synchronous_builds', 'world_late_prefetches', 'world_rebuilds',
            'world_rebuild_max_ms', 'world_generation_max_ms', 'world_upload_max_ms',
            'world_upload_total_ms', 'world_gpu_mb', 'world_largest_material_mb',
            'world_gpu_segments', 'track_rebuilds', 'track_rebuild_max_ms',
            'track_rebuild_total_ms', 'triangles', 'draw_calls']
    result = {'scenario': name, 'exit_code': exit_code, 'command': command,
              **{key: final.get(key) for key in keys},
              'peak_gpu_vram_mib': round(peak['vram']/1024, 1),
              'peak_gpu_gtt_mib': round(peak['gtt']/1024, 1),
              'peak_sampled_rss_mib': round(peak['rss']/1024, 1),
              'gpu_samples': len(samples)}
    hwm = re.search(r'Maximum resident set size \(kbytes\): (\d+)', text)
    result['kernel_peak_rss_mib'] = round(int(hwm.group(1))/1024, 1) if hwm else None
    (output / f'{prefix}-{name}-samples.json').write_text(json.dumps(samples))
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scenarios', default=','.join(SCENARIOS))
    parser.add_argument('--prefix', default='streaming-risk')
    parser.add_argument('--interval', type=float, default=0.5)
    args = parser.parse_args()
    names = [name for name in args.scenarios.split(',') if name]
    if any(name not in SCENARIOS for name in names) or not re.fullmatch(r'[a-z][a-z0-9-]*', args.prefix):
        parser.error(f'Known scenarios: {", ".join(SCENARIOS)}')
    if os.environ.get('TRAINSIM_PRIVATE_STREAMING') != '1':
        os.execvpe('env', ['env', 'LP_NUM_THREADS=4',
                   '../cna/tools/platform/run_gpu_tests_private.sh', '--exec',
                   'env', 'TRAINSIM_PRIVATE_STREAMING=1', 'SDL_VIDEODRIVER=x11',
                   'SDL_AUDIODRIVER=dummy', sys.executable, __file__, *sys.argv[1:]], os.environ)
    if not (re.fullmatch(r':[0-9]+', os.environ.get('DISPLAY', ''))
            and os.environ.get('WAYLAND_DISPLAY', '').startswith('cna-weston-')):
        raise RuntimeError('Refusing to run outside the private GPU display')
    output = ROOT / 'build/visual-audit'
    output.mkdir(parents=True, exist_ok=True)
    binary = ROOT / 'build/train-simulator'
    digest = subprocess.run(['sha256sum', str(binary)], capture_output=True, text=True,
                            check=True).stdout.split()[0]
    results = []
    for name in names:
        result = run(name, binary, output, args.prefix, args.interval)
        results.append(result)
        print(json.dumps(result), flush=True)
    summary = {'binary_sha256': digest, 'load_average': os.getloadavg(), 'results': results}
    (output / f'{args.prefix}-summary.json').write_text(json.dumps(summary, indent=2))
    return 0 if all(result['exit_code'] == 0 for result in results) else 1


if __name__ == '__main__':
    sys.exit(main())
