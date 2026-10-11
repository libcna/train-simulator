#!/usr/bin/env python3
"""Private GPU journey with a bounded, timestamped process/resource time series."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import statistics
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
os.chdir(ROOT)


def fields(line: str) -> dict[str, str]:
    # Service phase names such as "DOORS CLOSING" contain spaces.
    return dict(re.findall(r'(\w+)=(.*?)(?=\s+\w+=|$)', line.strip()))


def process_sample(pid: int) -> dict[str, int]:
    proc = Path('/proc') / str(pid)
    result = {}
    for line in (proc / 'status').read_text().splitlines():
        key, _, value = line.partition(':')
        if key in ('VmRSS', 'VmHWM', 'VmSize', 'Threads'):
            result[key] = int(value.split()[0])
    result['fd_count'] = len(list((proc / 'fd').iterdir()))
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seconds', type=float, default=9000)
    parser.add_argument('--time-scale', type=float, default=2)
    parser.add_argument('--interval', type=float, default=10)
    parser.add_argument('--prefix', default='stability-long')
    args = parser.parse_args()
    if not (10 <= args.seconds <= 14400 and 0 < args.time_scale <= 10
            and 1 <= args.interval <= 60 and re.fullmatch(r'[a-z][a-z0-9-]*', args.prefix)):
        parser.error('Use 10–14400 seconds, scale (0,10], interval 1–60 and a simple lowercase prefix')
    if os.environ.get('TRAINSIM_PRIVATE_SOAK') != '1':
        os.execvpe('env', ['env', 'LP_NUM_THREADS=4',
                   '../cna/tools/platform/run_gpu_tests_private.sh', '--exec',
                   'env', 'TRAINSIM_PRIVATE_SOAK=1', sys.executable, __file__, *sys.argv[1:]], os.environ)
    runtime = os.environ.get('XDG_RUNTIME_DIR', '')
    wayland = os.environ.get('WAYLAND_DISPLAY', '')
    if not (os.environ.get('CNA_WAYLAND_TEST_COMPOSITOR') == 'weston'
            and re.fullmatch(r'/tmp/cna-wl-[^/]+/run', runtime)
            and wayland.startswith('cna-weston-')
            and wayland == os.environ.get('CNA_WAYLAND_TEST_DISPLAY')
            and Path(runtime, wayland).is_socket()
            and re.fullmatch(r':[0-9]+', os.environ.get('DISPLAY', ''))):
        raise RuntimeError('Refusing to run outside the private GPU display')
    output = ROOT / 'build/visual-audit'
    output.mkdir(parents=True, exist_ok=True)
    prefix = output / args.prefix
    paths = {kind: Path(f'{prefix}-{kind}') for kind in
             ('game.log', 'samples.jsonl', 'manifest.json', 'summary.json', 'progress.json', 'final.png')}
    if any(path.exists() for path in paths.values()):
        raise RuntimeError('Evidence already exists; choose a new prefix instead of overwriting it')
    binary = ROOT / 'build/train-simulator'
    command = [str(binary), '--run-seconds', str(args.seconds), '--time-scale', str(args.time_scale),
               '--station', '1', '--quality', '2', '--passenger-view', '--car', '2',
               '--report-state', '--config', 'tests/facade-comparison.cfg',
               '--screenshot', str(paths['final.png'])]
    manifest = {'command': command, 'started_unix': time.time(),
                'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
                'git_head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
                'requested_seconds': args.seconds, 'time_scale': args.time_scale,
                'sample_interval_seconds': args.interval,
                'audio': 'enabled, SDL dummy output', 'avatars': 'enabled, High budget'}
    paths['manifest.json'].write_text(json.dumps(manifest, indent=2) + '\n')
    env = dict(os.environ, SDL_VIDEODRIVER='x11', SDL_AUDIODRIVER='dummy', LP_NUM_THREADS='4')
    samples = []
    latest = {}
    final = None
    failure = None
    start = time.monotonic()
    process = None
    def terminate(_signum, _frame):
        raise KeyboardInterrupt('Soak terminated')
    signal.signal(signal.SIGTERM, terminate)
    try:
        with paths['game.log'].open('w') as log, paths['samples.jsonl'].open('w') as series:
            process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT,
                                       env=env, start_new_session=True)
            with paths['game.log'].open() as reader:
                while True:
                    for line in reader:
                        if line.startswith('INPUT_TRACE '):
                            latest = fields(line)
                        elif line.startswith('FINAL_STATE '):
                            final = fields(line)
                    elapsed = time.monotonic() - start
                    if process.poll() is not None:
                        break
                    try:
                        sample = {'wall_seconds': round(elapsed, 3),
                                  **process_sample(process.pid), 'trace': latest.copy()}
                    except FileNotFoundError:
                        continue  # The owned process exited between poll and /proc sampling.
                    samples.append(sample)
                    series.write(json.dumps(sample, separators=(',', ':')) + '\n')
                    series.flush()
                    paths['progress.json'].write_text(json.dumps(sample, indent=2) + '\n')
                    if elapsed > args.seconds * 1.5 + 120:
                        raise TimeoutError('Simulation duration did not finish within the wall-clock limit')
                    try:
                        process.wait(timeout=args.interval)
                    except subprocess.TimeoutExpired:
                        pass
            returncode = process.wait()
            # Drain a possible final flush that arrived after the last periodic read.
            for line in paths['game.log'].read_text().splitlines():
                if line.startswith('FINAL_STATE '):
                    final = fields(line)
            if returncode != 0:
                raise RuntimeError(f'Game exited with status {returncode}')
            if final is None or len(samples) < 2:
                raise RuntimeError('Missing final state or resource samples')
            if (final.get('loaded_chunks') != '7' or final.get('passengers') != '38'
                    or final.get('avatar_visuals') != '18' or final.get('ready_avatar_visuals') != '18'
                    or final.get('screenshot_saved') != '1'):
                raise RuntimeError('Final world, population, avatar or screenshot invariant failed')
            for sample in samples:
                trace = sample['trace']
                if trace and (int(trace.get('loaded_chunks', 0)) > 7
                              or int(trace.get('passengers', 0)) > 38
                              or int(trace.get('avatar_visuals', 0)) > 18):
                    raise RuntimeError('Resource population exceeded its bounded budget')
            if args.seconds >= 7200 and args.time_scale >= 2:
                directions = {sample['trace'].get('direction') for sample in samples}
                if not {'1', '-1'} <= directions or int(final['station_legs']) < 12:
                    raise RuntimeError('Long run did not cover an endpoint reversal and return travel')
    except BaseException as error:
        failure = f'{type(error).__name__}: {error}'
    finally:
        if process is not None and process.poll() is None:
            os.killpg(process.pid, signal.SIGTERM)
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
        rss = [sample['VmRSS'] for sample in samples if 'VmRSS' in sample]
        tail = rss[-max(1, len(rss)//4):]
        summary = {'passed': failure is None, 'failure': failure,
                   'wall_seconds': round(time.monotonic()-start, 3), 'samples': len(samples),
                   'peak_rss_kib': max(rss, default=0),
                   'tail_quarter_median_rss_kib': statistics.median(tail) if tail else None,
                   'peak_threads': max((s.get('Threads', 0) for s in samples), default=0),
                   'peak_fds': max((s.get('fd_count', 0) for s in samples), default=0),
                   'final_state': final,
                   'interpretation': 'Resource bounds and clean completion, not a proof of leak freedom; inspect the time series.'}
        paths['summary.json'].write_text(json.dumps(summary, indent=2) + '\n')
        print(json.dumps({k: v for k, v in summary.items() if k != 'final_state'}), flush=True)
    return 1 if failure else 0


if __name__ == '__main__':
    raise SystemExit(main())
