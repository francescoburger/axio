#!/usr/bin/env python3
"""Controlla versioni installate, senza installare o aggiornare nulla."""
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def query(*args):
    return json.loads(subprocess.check_output(['arduino-cli', *args, '--json'], text=True))


def main():
    lock = json.loads((ROOT/'toolchain.lock.json').read_text())
    cli = query('version')['VersionString']
    cores = {p['id']: p.get('installed_version') for p in query('core','list')['platforms']}
    libraries = query('lib','list')['installed_libraries']
    libs = {p['library']['name']: p['library']['version'] for p in libraries}
    checks = [('Arduino CLI', cli, lock['arduino_cli'])]
    checks += [(k, cores.get(k), v) for k,v in lock['core'].items()]
    checks += [(k, libs.get(k), v) for k,v in lock['libraries'].items()]
    failed = False
    for name, actual, expected in checks:
        ok = actual == expected
        print(f'{"PASS" if ok else "FAIL"}: {name}: {actual or "assente"} (richiesto {expected})')
        failed |= not ok
    build = ROOT/'.build'
    build.mkdir(exist_ok=True)
    (build/'toolchain-observed.json').write_text(json.dumps(dict(cli=cli, cores=cores, libraries=libs), indent=2)+'\n')
    return int(failed)


if __name__ == '__main__':
    sys.exit(main())
