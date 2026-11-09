"""Phase 4 bounded HTTP parser and user-provided battery math; no device claims."""
from pathlib import Path
import shutil
import subprocess
import unittest
ROOT=Path(__file__).resolve().parents[1]
class Phase4Tests(unittest.TestCase):
    def test_low_wake_mode_and_short_press_rejection(self):
        binary=ROOT/'.build/tests/wake_runtime'
        binary.parent.mkdir(parents=True,exist_ok=True)
        result=subprocess.run([shutil.which('c++'),'-std=c++11','-Wall','-Wextra','-Werror',
                               '-I',str(ROOT/'tests/stubs/runtime'),'-I',str(ROOT/'firmware/Axio'),
                               str(ROOT/'tests/wake_runtime_harness.cpp'),'-o',str(binary)],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)
        subprocess.run([str(binary)],check=True,timeout=3)

    def test_http_budgets_origin_token_timeout_and_battery_math(self):
        binary=ROOT/'.build/tests/phase4'
        binary.parent.mkdir(parents=True,exist_ok=True)
        subprocess.run([shutil.which('c++'),'-std=c++11','-Wall','-Wextra','-Werror',
                        '-Wno-misleading-indentation','-I',str(ROOT/'firmware/Axio'),
                        str(ROOT/'tests/phase4_harness.cpp'),'-o',str(binary)],check=True,capture_output=True,text=True)
        subprocess.run([str(binary)],check=True,capture_output=True,text=True)

    def test_dashboard_routes_acknowledgements_and_power_sequence(self):
        binary=ROOT/'.build/tests/dashboard_service'
        library=Path.home()/'Documents/Arduino/libraries/Adafruit_BNO055'
        p=subprocess.run([shutil.which('c++'),'-std=c++11','-Wall','-Wextra','-Werror',
                        '-Wno-misleading-indentation','-I',str(library),'-I',str(ROOT/'firmware/Axio'),
                        str(ROOT/'tests/dashboard_service_harness.cpp'),'-o',str(binary)],capture_output=True,text=True)
        self.assertEqual(p.returncode,0,p.stderr)
        p=subprocess.run([str(binary)],capture_output=True,text=True)
        self.assertEqual(p.returncode,0,p.stderr)
        self.assertEqual(p.stdout.strip(),'PASS')
