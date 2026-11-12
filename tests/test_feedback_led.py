"""Timed LED cues, real GPIO adapter and power integration with simulated IO."""
from pathlib import Path
import shutil
import subprocess
import unittest
from support_dashboard_build import build

ROOT=Path(__file__).resolve().parents[1]


class FeedbackLedTests(unittest.TestCase):
    def compile_run(self,name,paths,defines=()):
        binary=ROOT/'.build/tests'/name
        binary.parent.mkdir(parents=True,exist_ok=True)
        p=subprocess.run([shutil.which('c++'),'-std=c++11','-Wall','-Wextra','-Werror',*defines,
                          '-I',str(ROOT/'tests/stubs/runtime'),'-I',str(ROOT/'firmware/Axio'),
                          *[str(ROOT/p) for p in paths],'-o',str(binary)],capture_output=True,text=True)
        self.assertEqual(p.returncode,0,p.stderr)
        p=subprocess.run([str(binary)],capture_output=True,text=True,timeout=5)
        self.assertEqual(p.returncode,0,p.stderr)

    def test_patterns_priority_delayed_ticks_and_rollover(self):
        self.compile_run('feedback_led',['tests/feedback_led_harness.cpp'])

    def test_gpio_polarity_sleep_hold_and_recovery_on_both_boards(self):
        for board in ('c3','s3'):
            with self.subTest(board=board):
                self.compile_run('feedback_led_runtime_'+board,
                                 ['tests/feedback_led_runtime_harness.cpp','firmware/Axio/src/power/FeedbackLedRuntime.cpp'],
                                 ['-DARDUINO_XIAO_ESP32S3'] if board=='s3' else [])

    def test_s3_button_calibration_and_sleep_share_led_runtime(self):
        binary=build(board='s3')
        p=subprocess.run([str(binary),'--sleep-button-offline'],capture_output=True,text=True,timeout=8)
        self.assertEqual(p.returncode,0,p.stderr)
        self.assertIn('PASS: physical long press',p.stdout)
