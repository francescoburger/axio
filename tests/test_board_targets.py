"""Board pin isolation and build/upload target selection; no hardware access."""
from pathlib import Path
import shutil
import subprocess
import unittest

ROOT=Path(__file__).resolve().parents[1]


class BoardTargetTests(unittest.TestCase):
    def test_pad_mapping_and_reported_board(self):
        build=ROOT/'.build/tests'
        build.mkdir(parents=True,exist_ok=True)
        for board,pins in (('C3',(6,7,5,2)),('S3',(5,6,4,1))):
            with self.subTest(board=board):
                source='#include "hardware_config.h"\n#include <cstring>\n'
                for name,value in zip(('SDA_PIN','SCL_PIN','BUTTON_PIN','BATTERY_ADC_PIN'),pins):
                    source+=f'static_assert({name}=={value},"{name}");\n'
                ledPin,ledActive=(21,0) if board=='S3' else (3,1)
                source+=f'static_assert(FEEDBACK_LED_PIN=={ledPin},"LED pin");\n'
                source+=f'static_assert(FEEDBACK_LED_ACTIVE_LEVEL=={ledActive},"LED polarity");\n'
                source+=f'int main(){{return std::strcmp(AXIO_BOARD_NAME,"XIAO_ESP32{board}");}}\n'
                binary=build/('board_'+board)
                run=subprocess.run([shutil.which('c++'),'-std=c++11','-DARDUINO_ARCH_ESP32',
                                    '-DARDUINO_XIAO_ESP32'+board,'-I',str(ROOT/'firmware/Axio'),
                                    '-x','c++','-','-o',str(binary)],input=source,text=True,capture_output=True)
                self.assertEqual(run.returncode,0,run.stderr)
                subprocess.run([str(binary)],check=True)

    def test_build_and_upload_share_options_and_separate_artifacts(self):
        configurations=[]
        for board in ('c3','s3'):
            run=subprocess.run(['bash','-c','source scripts/board_config.sh\naxio_board_config "$1"\nprintf "%s\\n" "$FQBN" "$BOARD_OPTIONS" "$BUILD" "$OUTPUT"',
                                'board-test',board],cwd=ROOT,text=True,capture_output=True,check=True)
            fqbn,options,build,output=run.stdout.splitlines()
            self.assertEqual(fqbn,'esp32:esp32:XIAO_ESP32'+board.upper())
            self.assertIn('EraseFlash=none',options)
            self.assertIn('CDCOnBoot=default',options)
            configurations.append((build,output))
        self.assertNotEqual(configurations[0][0],configurations[1][0])
        self.assertNotEqual(configurations[0][1],configurations[1][1])

    def test_invalid_target_and_s3_baseline_rejected_before_build(self):
        for args in (['scripts/compile.sh','--board','other'],
                     ['scripts/compile.sh','--board','s3','--baseline'],
                     ['scripts/upload.sh','--board','other','--port','unused']):
            run=subprocess.run(['bash',*args],cwd=ROOT,capture_output=True,text=True)
            self.assertEqual(run.returncode,2,run.stdout+run.stderr)
            self.assertIn('FAIL:',run.stderr)
