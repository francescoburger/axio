#!/usr/bin/env python3
"""Build an isolated copy of Axio with a bounded mDNS RX gate; no upload."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[1]
WORK=ROOT/'.build/mdns-partition'
OPTIONS='CDCOnBoot=default,CPUFreq=160,FlashFreq=80,FlashMode=qio,FlashSize=4M,PartitionScheme=default,DebugLevel=none,EraseFlash=none,UploadSpeed=921600'

def main():
    WORK.mkdir(exist_ok=False)
    restore=WORK/'restore';restore.mkdir()
    for path in (ROOT/'.build/artifacts').iterdir():
        if path.suffix=='.bin':shutil.copy2(path,restore/path.name)
    original=restore/'Axio.ino.bin'
    if not original.is_file():raise RuntimeError('Build production firmware first')
    sketch=WORK/'Axio'
    shutil.copytree(ROOT/'firmware/Axio',sketch,ignore=shutil.ignore_patterns('build','__pycache__'))
    for name in ('PartitionHook.h','PartitionHook.cpp'):
        shutil.copy2(ROOT/'tests/hardware/mdns_partition'/name,sketch/name)
    source=sketch/'Axio.ino';text=source.read_text()
    needle='return connectivityCommand(id,command,out)||oscConfigCommand(id,command,out);'
    if text.count(needle)!=1:raise RuntimeError('Unexpected diagnostic adapter')
    source.write_text('#include "PartitionHook.h"\n'+text.replace(needle,'return mdnsPartitionCommand(id,command,out)||connectivityCommand(id,command,out)||oscConfigCommand(id,command,out);'))
    env=dict(os.environ,TMPDIR=str(ROOT/'.build/tmp'),ARDUINO_BUILD_CACHE_PATH=str(ROOT/'.build/cache'))
    command=['arduino-cli','compile','--fqbn','esp32:esp32:XIAO_ESP32C3','--board-options',OPTIONS,
             '--build-property','compiler.c.elf.extra_flags=-Wl,--wrap=udp_recv',
             '--build-path',str(WORK/'build'),'--warnings','all',str(sketch)]
    result=subprocess.run(command,cwd=ROOT,env=env,capture_output=True,text=True)
    (WORK/'compile.log').write_text(result.stdout+result.stderr)
    print(result.stdout+result.stderr)
    if result.returncode:raise SystemExit(result.returncode)
    artifacts=WORK/'artifacts';artifacts.mkdir()
    for path in (WORK/'build').iterdir():
        if path.suffix in ('.bin','.elf','.map'):shutil.copy2(path,artifacts/path.name)
    manifest={'production_sha256':hashlib.sha256(original.read_bytes()).hexdigest(),
              'probe_sha256':hashlib.sha256((artifacts/'Axio.ino.bin').read_bytes()).hexdigest(),
              'production_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
              'hook':'udp_recv native PCB port 5353 only; HOLD automatically expires after 60 seconds'}
    (WORK/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps(manifest,indent=2))

if __name__=='__main__':main()
