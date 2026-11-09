#!/usr/bin/env python3
"""Launch a local detached endurance test; no scheduled AI calls or external service."""
import argparse
import datetime
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--primary', required=True)
    parser.add_argument('--secondary', required=True)
    parser.add_argument('--osc-bind', required=True)
    parser.add_argument('--osc-port', type=int, default=16888)
    parser.add_argument('--seconds', type=int, default=3600)
    parser.add_argument('--run-name', default='phase4-endurance-' + datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ'))
    args = parser.parse_args()
    if not re.fullmatch(r'[a-zA-Z0-9_-]+', args.run_name) or not 5 <= args.seconds <= 86400:
        parser.error('Invalid run name or duration')
    if not 1 <= args.osc_port <= 65535:
        parser.error('Invalid UDP port')
    python = ROOT / '.venv/bin/python'
    if not python.exists() or not Path('/usr/bin/caffeinate').exists():
        parser.error('Existing .venv and macOS caffeinate required; nothing is installed automatically')
    run_dir = ROOT / 'captures' / args.run_name
    if run_dir.exists():
        parser.error('Run directory already exists')
    (ROOT / '.build').mkdir(exist_ok=True)
    log = ROOT / '.build' / (args.run_name + '.log')
    command = ['/usr/bin/caffeinate', '-i', str(python), '-B', str(ROOT / 'tools/endurance_test.py'),
               '--primary', args.primary, '--secondary', args.secondary,
               '--osc-bind', args.osc_bind, '--osc-port', str(args.osc_port),
               '--seconds', str(args.seconds), '--run-dir', str(run_dir)]
    with log.open('x') as output:
        child = subprocess.Popen(command, cwd=ROOT, stdin=subprocess.DEVNULL, stdout=output,
                                 stderr=subprocess.STDOUT, start_new_session=True, close_fds=True)
    launched = {'supervisor_pid': child.pid, 'requested_seconds': args.seconds,
                'run_dir': str(run_dir), 'log': str(log), 'status': str(run_dir / 'status.json'),
                'summary': str(run_dir / 'summary.md'), 'ai_calls': False}
    (ROOT / '.build' / (args.run_name + '-launch.json')).write_text(json.dumps(launched, indent=2) + '\n')
    print(json.dumps(launched, indent=2))


if __name__ == '__main__':
    main()
