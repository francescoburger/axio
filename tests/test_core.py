"""Host checks on production core, using only a minimal device and clock fake."""
import os
from pathlib import Path
import shutil
import subprocess
import unittest
from test_baseline import ROOT, extract

class CoreTests(unittest.TestCase):
    def test_state_calibration_errors_and_scheduler(self):
        library = Path(os.environ.get('BNO055_LIBRARY', str(Path.home()/'Documents/Arduino/libraries/Adafruit_BNO055')))
        build = ROOT/'.build/tests'
        build.mkdir(parents=True, exist_ok=True)
        subprocess.run([shutil.which(os.environ.get('CXX', 'c++')), '-std=c++11',
                        '-O0', '-ffp-contract=off', '-I', str(library),
                        '-I', str(ROOT/'firmware/Axio'), str(ROOT/'tests/core_state_harness.cpp'),
                        '-o', str(build/'core_state')], check=True, capture_output=True, text=True)
        subprocess.run([str(build/'core_state')], check=True, capture_output=True, text=True)

    def test_checked_bno_registers_and_conversion(self):
        library = Path(os.environ.get('BNO055_LIBRARY', str(Path.home()/'Documents/Arduino/libraries/Adafruit_BNO055')))
        build = ROOT/'.build/tests'
        build.mkdir(parents=True, exist_ok=True)
        subprocess.run([shutil.which(os.environ.get('CXX', 'c++')), '-std=c++11',
                        '-I', str(ROOT/'tests/stubs'), '-I', str(library),
                        '-I', str(ROOT/'firmware/Axio'), str(ROOT/'tests/bno_device_harness.cpp'),
                        '-o', str(build/'bno_device')], check=True, capture_output=True, text=True)
        subprocess.run([str(build/'bno_device')], check=True, capture_output=True, text=True)

    def test_production_osc_serializer_uses_only_snapshot(self):
        library = Path(os.environ.get('BNO055_LIBRARY', str(Path.home()/'Documents/Arduino/libraries/Adafruit_BNO055')))
        build = ROOT/'.build/tests'
        build.mkdir(parents=True, exist_ok=True)
        # Reuse the Phase 0 bundle fake, plus checks on the production serializer.
        fake = (ROOT/'tests/math_harness.cpp.in').read_text()
        fake = fake[:fake.index('/*__GLOBALS__*/')]
        fake = fake.replace('  void send(int) {', '  void send(int) {\n    for (int i=0; i<4; ++i) for (int k=0; k<3; ++k)\n      if (entries[i].values[k] != float(3*i+k+1)) std::exit(5);')
        source = (ROOT/'firmware/Axio/src/transport/LegacyTransport.cpp').read_text()
        generated = fake + '\n#include "src/core/Sample.h"\n' + extract(source, 'sendLegacySample') + r"""
int main() {
  axio::Sample s;
  s.yaw=1; s.pitch=2; s.roll=3;
  s.axReal=4; s.ayReal=5; s.azReal=6;
  s.axWorld=7; s.ayWorld=8; s.azWorld=9;
  s.gx=10; s.gy=11; s.gz=12;
  sendLegacySample(s);
  return (bno.reads==0 && UDP.packets==1) ? 0 : 6;
}
"""
        (build/'serializer.cpp').write_text(generated)
        subprocess.run([shutil.which(os.environ.get('CXX', 'c++')), '-std=c++11',
                        '-I', str(library), '-I', str(ROOT/'firmware/Axio'),
                        str(build/'serializer.cpp'), '-o', str(build/'serializer')],
                       check=True, capture_output=True, text=True)
        subprocess.run([str(build/'serializer')], check=True, capture_output=True, text=True)
