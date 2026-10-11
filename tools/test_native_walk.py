#!/usr/bin/env python3
"""Drive the private X11 window through seating, all six cars, display and toilet."""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
os.chdir(ROOT)


class Controller:
    def __init__(self, log: Path, output: Path, window: str):
        self.reader = log.open()
        self.output, self.window = output, window
        self.latest = {}
        self.traces = []
        self.held = set()

    def xd(self, *args):
        subprocess.run(['xdotool', *map(str, args)], check=True)

    def read(self):
        for line in self.reader:
            if line.startswith('INPUT_TRACE '):
                self.latest = dict(re.findall(r'(\w+)=(.*?)(?=\s+\w+=|$)', line.strip()))
                self.traces.append(self.latest.copy())
        return self.latest

    def wait(self, predicate=lambda s: True, timeout=20, fresh=True):
        frame = int(self.read().get('frames', -1)) if fresh else -1
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            state = self.read()
            if int(state.get('frames', -1)) > frame and predicate(state):
                return state
            time.sleep(0.1)
        raise RuntimeError(f'Input/state deadline, last trace: {self.latest}')

    def key(self, key, seconds=0.18):
        self.xd('keydown', key)
        self.held.add(key)
        try:
            time.sleep(seconds)
        finally:
            self.xd('keyup', key)
            self.held.discard(key)
        time.sleep(0.1)

    def capture(self):
        self.xd('mousemove', '--window', self.window, 640, 360, 'mousedown', 1)
        time.sleep(0.2)
        self.xd('mouseup', 1)
        self.wait(lambda s: s['captured'] == '1' and s['native'] == '1')

    def turn(self, target):
        for _ in range(5):
            state = self.read()
            error = math.remainder(target-float(state['yaw_rad']), 2*math.pi)
            if abs(error) < 0.0016:
                return
            pixels = round(error/0.0022)
            while pixels:
                step = max(-80, min(80, pixels))
                self.xd('mousemove_relative', '--', step, 0)
                pixels -= step
                time.sleep(0.055)
            self.wait()
        raise RuntimeError(f'Camera failed to reach heading {target}: {self.latest}')

    def screenshot(self, name):
        subprocess.run(['import', '-window', self.window, str(self.output / f'{name}.png')], check=True)

    def coordinate(self, axis, target, tolerance=0.12):
        self.turn(math.pi/2 if axis == 'x' else 0)
        end = time.monotonic()+240
        stalled = 0
        previous = None
        while time.monotonic() < end:
            position = float(self.read()[axis])
            delta = target-position
            if abs(delta) <= tolerance:
                return
            if previous is not None and abs(position-previous) < 0.015:
                stalled += 1
            else:
                stalled = 0
            if stalled > 3:
                raise RuntimeError(f'Blocked approaching {axis}={target}: {self.latest}')
            previous = position
            positive = delta > 0
            key = ('w' if positive else 's') if axis == 'x' else ('s' if positive else 'w')
            self.key(key, min(3.0, max(0.04, abs(delta)/1.75*0.80)))
            self.wait()
        raise RuntimeError(f'Coordinate deadline for {axis}={target}')

    def traverse(self, target):
        self.turn(0 if target < 0 else math.pi)
        self.xd('keydown', 'w')
        self.held.add('w')
        start = len(self.traces)
        end = time.monotonic()+300
        previous = float(self.latest['z'])
        stalls = 0
        passing_lane = 0.25
        try:
            while time.monotonic() < end:
                state = self.wait()
                if state['captured'] != '1' or state['native'] != '1' or state['seated'] != '0':
                    raise RuntimeError(f'Lost walking input state: {state}')
                z = float(state['z'])
                if (target > 0 and z >= 61.5) or (target < 0 and z <= -61.5):
                    self.key('w', 1.0)  # Keep pressing against the inaccessible cab boundary.
                    break
                stalls = stalls+1 if abs(z-previous) < 0.1 else 0
                previous = z
                if stalls >= 2:
                    # Try the available aisle sides before backing away. Feedback
                    # keeps the player clear of seats and the toilet side wall.
                    self.xd('keyup', 'w')
                    self.held.discard('w')
                    for _ in range(4):
                        x = float(self.latest['x'])
                        delta = passing_lane-x
                        if abs(delta) < 0.02:
                            break
                        right_in_world = delta > 0
                        key = ('d' if right_in_world else 'a') if target < 0 else ('a' if right_in_world else 'd')
                        self.key(key, max(0.025, min(0.5, abs(delta)/1.75*0.75)))
                        self.wait()
                    if abs(float(self.latest['x'])-passing_lane) >= 0.07:
                        self.key('s', 0.6)
                        self.wait()
                    passing_lane = -passing_lane
                    self.xd('keydown', 'w')
                    self.held.add('w')
                    stalls = 0
            else:
                raise RuntimeError(f'Could not traverse to the cab at {target}: {self.latest}')
        finally:
            self.xd('keyup', 'w')
            self.held.discard('w')
        state = self.wait()
        if abs(float(state['z'])) > 62.61:
            raise RuntimeError('Player escaped a cab boundary')
        segment = [float(s['z']) for s in self.traces[start:]]
        print(f'NATIVE_WALK traversal target={target} min_z={min(segment):.3f} max_z={max(segment):.3f}', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rounds', type=int, default=4,
                        help='Full-car round trips; zero runs seating/display/toilet only')
    parser.add_argument('--prefix', default='native-walk')
    args = parser.parse_args()
    if not 0 <= args.rounds <= 12 or not re.fullmatch(r'[a-z][a-z0-9-]*', args.prefix):
        parser.error('Use 0–12 rounds and a simple lowercase prefix')
    if os.environ.get('TRAINSIM_PRIVATE_WALK') != '1':
        os.execvpe('env', ['env', 'LP_NUM_THREADS=4',
                   '../cna/tools/platform/run_gpu_tests_private.sh', '--exec',
                   'env', 'TRAINSIM_PRIVATE_WALK=1', sys.executable, __file__, *sys.argv[1:]], os.environ)
    runtime, wayland = os.environ.get('XDG_RUNTIME_DIR', ''), os.environ.get('WAYLAND_DISPLAY', '')
    if not (os.environ.get('CNA_WAYLAND_TEST_COMPOSITOR') == 'weston'
            and re.fullmatch(r'/tmp/cna-wl-[^/]+/run', runtime)
            and wayland.startswith('cna-weston-') and wayland == os.environ.get('CNA_WAYLAND_TEST_DISPLAY')
            and Path(runtime, wayland).is_socket()
            and re.fullmatch(r':[0-9]+', os.environ.get('DISPLAY', ''))):
        raise RuntimeError('Refusing to send input outside the private GPU display')
    output = ROOT / 'build/visual-audit' / args.prefix
    output.mkdir(parents=True, exist_ok=False)
    (output / 'settings.cfg').write_text('camera_vibration = 0\nmouse_sensitivity = 0.0022\n')
    env = dict(os.environ, SDL_VIDEODRIVER='x11', SDL_AUDIODRIVER='dummy', LP_NUM_THREADS='4')
    command = [str(ROOT/'build/train-simulator'), '--run-seconds', '3600', '--route-km', '52',
               '--time-scale', '1', '--quality', '2', '--window-view', '--car', '4', '--report-state',
               '--config', str(output/'settings.cfg')]
    (output/'manifest.json').write_text(json.dumps({
        'command': command, 'started_unix': time.time(), 'rounds': args.rounds,
        'binary_sha256': hashlib.sha256((ROOT/'build/train-simulator').read_bytes()).hexdigest(),
        'git_head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
        'input': 'Injected native X11 events on a private Weston/Xwayland display',
    }, indent=2)+'\n')
    start = time.monotonic()
    process, controller, failure, final = None, None, None, None
    def terminate(_signum, _frame):
        raise KeyboardInterrupt('Native walk terminated')
    signal.signal(signal.SIGTERM, terminate)
    try:
        with (output/'game.log').open('w') as log:
            process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT, env=env,
                                       start_new_session=True)
            window = None
            for _ in range(100):
                found = subprocess.run(['xdotool', 'search', '--onlyvisible', '--name',
                                        '^Northern Valley Express - passenger service$'], capture_output=True, text=True)
                if found.returncode == 0 and found.stdout.strip():
                    window = found.stdout.splitlines()[0]
                    break
                if process.poll() is not None:
                    raise RuntimeError('Application exited before opening its window')
                time.sleep(0.1)
            if window is None:
                raise RuntimeError('Window startup deadline')
            controller = Controller(output/'game.log', output, window)
            controller.xd('windowfocus', '--sync', window)
            controller.wait(lambda s: s.get('ready_avatar_visuals') == '18', timeout=45, fresh=False)
            controller.capture()
            controller.screenshot('seated-start')
            # Begin in a real reserved seat; exercise repeat stand/sit and end standing.
            for _ in range(7):
                seated = controller.read()['seated']
                controller.key('e')
                controller.wait(lambda s: s['seated'] != seated)
            print('NATIVE_WALK seating passed', flush=True)
            for lap in range(args.rounds):
                controller.traverse(-62.5)
                controller.screenshot(f'front-{lap}')
                controller.traverse(62.5)
                controller.screenshot(f'rear-{lap}')
            controller.coordinate('x', 0.25, 0.02)
            controller.coordinate('z', 10.5)
            controller.turn(0)
            for _ in range(6):
                controller.key('e')
                state = controller.wait()
                if state['display'] == '1':
                    break
            else:
                raise RuntimeError('Passenger information display did not open')
            controller.screenshot('display')
            controller.key('e')
            controller.wait(lambda s: s['display'] == '0')
            controller.capture()
            controller.coordinate('z', 4.10, 0.08)
            # Wait beside the opposite wall, within interaction reach but clear
            # of an NPC's path to/from a reserved toilet.
            controller.coordinate('x', -0.95, 0.05)
            controller.turn(math.pi/2)
            for _ in range(24):
                if controller.read()['toilet_door_open'] == '1':
                    break
                controller.key('e')
                controller.wait()
            else:
                raise RuntimeError('Accessible toilet never became available')
            controller.coordinate('x', 0.66, 0.08)
            controller.coordinate('z', 5.70, 0.10)
            if controller.read()['inside_toilet'] != '1':
                raise RuntimeError('Could not enter the toilet through its aisle door')
            controller.turn(0)
            controller.key('e')
            controller.wait(lambda s: s['toilet_door_open'] == '0' and s['inside_toilet'] == '1')
            controller.screenshot('toilet-closed-inside')
            time.sleep(10)
            controller.key('e')
            controller.wait(lambda s: s['toilet_door_open'] == '1')
            controller.coordinate('z', 4.10, 0.08)
            controller.coordinate('x', 0, 0.08)
            controller.turn(math.pi/2)
            controller.key('e')
            controller.wait(lambda s: s['toilet_door_open'] == '0' and s['inside_toilet'] == '0')
            controller.screenshot('toilet-exited')
            controller.key('F12')
            if process.wait(timeout=20) != 0:
                raise RuntimeError('Application did not exit cleanly')
        for line in (output/'game.log').read_text().splitlines():
            if line.startswith('FINAL_STATE '):
                final = dict(re.findall(r'(\w+)=(.*?)(?=\s+\w+=|$)', line.strip()))
        if not final or int(final['sit_transitions']) < 3 or int(final['stand_transitions']) < 4:
            raise RuntimeError('Seating transition counters missing')
        if int(final['toilet_door_interactions']) < 4 or final['inside_toilet'] != '0':
            raise RuntimeError('Toilet interaction counters missing')
        cars = {max(0, min(5, math.floor((float(s['z'])+63)/21))) for s in controller.traces}
        if args.rounds and cars != set(range(6)):
            raise RuntimeError(f'Incomplete native car traversal: {cars}')
    except BaseException as error:
        failure = f'{type(error).__name__}: {error}'
        if controller and process is not None and process.poll() is None:
            try:
                controller.screenshot('failure')
            except Exception:
                pass  # Preserve the original failure even if the private display exited.
    finally:
        if controller:
            for key in list(controller.held):
                # A dead private display must not prevent application cleanup or the receipt.
                subprocess.run(['xdotool', 'keyup', key], check=False)
            controller.reader.close()
        if process is not None and process.poll() is None:
            os.killpg(process.pid, signal.SIGTERM)
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
        summary = {'passed': failure is None, 'failure': failure,
                   'wall_seconds': round(time.monotonic()-start, 3), 'rounds': args.rounds,
                   'trace_count': len(controller.traces) if controller else 0, 'final_state': final}
        (output/'summary.json').write_text(json.dumps(summary, indent=2)+'\n')
        print(json.dumps({k: v for k, v in summary.items() if k != 'final_state'}), flush=True)
    return 1 if failure else 0


if __name__ == '__main__':
    raise SystemExit(main())
