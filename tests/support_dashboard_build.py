"""Builds the real dashboard runtime for socket/browser integration tests."""
from pathlib import Path
import shutil
import subprocess
ROOT=Path(__file__).resolve().parents[1]
def build(port=17884):
    binary=ROOT/'.build/tests/dashboard_runtime'
    binary.parent.mkdir(parents=True,exist_ok=True)
    library=Path.home()/'Documents/Arduino/libraries/Adafruit_BNO055'
    paths=['tests/dashboard_runtime_harness.cpp','firmware/Axio/src/power/WakeRuntime.cpp','firmware/Axio/src/web/DashboardRuntime.cpp',
           'firmware/Axio/src/connectivity/ConnectivityRuntime.cpp','firmware/Axio/src/connectivity/ApRuntime.cpp',
           'firmware/Axio/src/connectivity/DiscoveryRuntime.cpp','firmware/Axio/src/transport/OscRuntime.cpp']
    command=[shutil.which('c++'),'-std=c++11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',
             '-DAXIO_DASHBOARD_TEST_CLOCK','-DAXIO_DASHBOARD_HTTP_PORT='+str(port),'-I',str(ROOT/'tests/stubs/runtime'),
             '-I',str(ROOT/'tests/stubs'),'-I',str(library),'-I',str(ROOT/'firmware/Axio')]
    p=subprocess.run(command+[str(ROOT/p) for p in paths]+['-o',str(binary)],capture_output=True,text=True)
    if p.returncode:raise RuntimeError(p.stderr)
    return binary
if __name__=='__main__':print(build())
