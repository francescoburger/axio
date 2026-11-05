"""Esegue le funzioni C++ originali estratte, contro un oracolo Python separato."""
import hashlib
import math
import os
from pathlib import Path
import random
import shutil
import struct
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
SHA256 = '40c8113e8cb8d0ef0229ffc50282933e889a94c3e8795f8dac942a1968aaf259'


def f32(x):
    return struct.unpack('f', struct.pack('f', x))[0]


def cpp_remainder(n, d):
    # Python % non replica il resto C++ per numeratori negativi.
    return n - math.trunc(n / d) * d


def reference(v):
    a, b, c, d = v[:4]
    offset = (a, -b, -c, -d)
    yaw_offset = int(v[4])
    e, f, g, h = v[7:11]
    a, b, c, d = offset
    q = list(map(f32, (a*e-b*f-c*g-d*h, a*f+b*e+c*h-d*g,
                         a*g-b*h+c*e+d*f, a*h+b*g-c*f+d*e)))
    w, x, y, z = q
    yaw = f32((cpp_remainder(int(v[11])-yaw_offset+360+180, 360)/360.0)*2-1)
    pitch = f32(int(v[12])/90.0)
    roll = f32((-1*int(v[13]))/180.0)
    acc = list(map(f32, v[14:17]))
    # Ogni prodotto/somma della matrice originale è float, da sinistra a destra.
    mul = lambda a, b: f32(a*b)
    add = lambda a, b: f32(a+b)
    sub = lambda a, b: f32(a-b)
    twice = lambda a, b: mul(mul(2, a), b)
    diag = lambda a, b: sub(sub(1, twice(a, a)), twice(b, b))
    matrix = ((diag(y,z), sub(twice(x,y),twice(w,z)), add(twice(x,z),twice(w,y))),
              (add(twice(x,y),twice(w,z)), diag(x,z), sub(twice(y,z),twice(w,x))),
              (sub(twice(x,z),twice(w,y)), add(twice(y,z),twice(w,x)), diag(x,y)))
    world = [add(add(mul(row[0], acc[0]), mul(row[1], acc[1])), mul(row[2], acc[2])) for row in matrix]
    return list(offset)+[yaw_offset]+q+[yaw,pitch,roll]+[f32(t/35.0) for t in acc+world]+[f32(int(t)/1500.0) for t in v[17:20]]


def extract(source, name):
    start = source.index('void ' + name + '(')
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


class BaselineTests(unittest.TestCase):
    def test_immutable_source_and_working_copy(self):
        original = (ROOT/'reference/HandTrackerV1_1.ino').read_bytes()
        self.assertEqual(hashlib.sha256(original).hexdigest(), SHA256)
        expected = original.replace(b'#define SDA_PIN 5\n#define SCL_PIN 6', b'#include "hardware_config.h"')
        self.assertEqual((ROOT/'firmware/Axio/Axio.ino').read_bytes(), expected)
        header = (ROOT/'firmware/Axio/hardware_config.h').read_text()
        self.assertIn('#define SDA_PIN 5\n#define SCL_PIN 6', header)

    def test_original_cpp_against_reference(self):
        compiler = shutil.which(os.environ.get('CXX', 'c++'))
        self.assertIsNotNone(compiler, 'installare prima gli strumenti C++ macOS')
        library = Path(os.environ.get('BNO055_LIBRARY', str(Path.home()/'Documents/Arduino/libraries/Adafruit_BNO055')))
        self.assertTrue((library/'utility/imumaths.h').is_file(), 'impostare BNO055_LIBRARY alla libreria Adafruit BNO055 1.6.4')
        source = (ROOT/'reference/HandTrackerV1_1.ino').read_text()
        globals_ = source[source.index('float q0,'):source.index('WiFiUDP UDP;')]
        functions = '\n'.join(extract(source, name) for name in ('calibrateReference','osctare','computeWorldAccel','bno_loop'))
        generated = (ROOT/'tests/math_harness.cpp.in').read_text().replace('/*__GLOBALS__*/', globals_).replace('/*__FUNCTIONS__*/', functions)
        build = ROOT/'.build/tests'
        build.mkdir(parents=True, exist_ok=True)
        (build/'baseline.cpp').write_text(generated)
        subprocess.run([compiler,'-std=c++11','-O0','-ffp-contract=off','-I',str(library),str(build/'baseline.cpp'),'-o',str(build/'baseline')], check=True, capture_output=True, text=True)
        cases = []
        def case(tare=0, yaw=0, pitch=0, roll=0, gyro=(0,0,0), quat=(1,0,0,0), origin=(1,0,0,0), acc=(0,0,0)):
            return [*origin,tare,0,0,*quat,yaw,pitch,roll,*acc,*gyro]
        # Tutti i gradi dello yaw, entrambi i lati del wrap e tarature frazionarie.
        for offset in (0, 179.9375, 359.9375):
            cases.extend(case(tare=offset, yaw=degree+.9375) for degree in range(360))
        for value in (-900.75,-180.99,-90.99,-1.99,-.99,0,.99,1.99,90.99,180.99,900.75):
            cases.append(case(tare=359.9,yaw=value,pitch=value,roll=value,gyro=(value,-value,.99),acc=(70,-70,35)))
        # Ordine non commutativo, quaternion non unitari, segno q/-q e tarature ripetute.
        cases += [case(origin=(2,0,0,0),quat=(2,0,0,0)),
                  case(origin=(.5,.5,.5,.5),quat=(.5,-.5,.5,-.5),acc=(35,0,0)),
                  case(quat=(0,0,0,1),acc=(35,0,0)),
                  case(quat=(0,0,0,-1),acc=(35,0,0))]
        rng = random.Random(110)
        for _ in range(250):
            def quaternion():
                q = [rng.uniform(-1,1) for _ in range(4)]
                norm = math.sqrt(sum(t*t for t in q))
                return [t/norm for t in q]
            cases.append(case(tare=rng.uniform(0,360),yaw=rng.uniform(0,360),pitch=rng.uniform(-180,180),roll=rng.uniform(-90,90),origin=quaternion(),quat=quaternion(),acc=[rng.uniform(-80,80) for _ in range(3)],gyro=[rng.uniform(-2000,2000) for _ in range(3)]))
        run = subprocess.run([str(build/'baseline')], input='\n'.join(' '.join(map(repr,v)) for v in cases)+'\n', text=True, capture_output=True, check=True)
        rows = run.stdout.splitlines()
        self.assertEqual(len(rows), len(cases))
        for i, (row, case_) in enumerate(zip(rows, cases)):
            actual = list(map(float, row.split()))
            expected = reference(case_)
            self.assertEqual(len(actual), len(expected))
            for column, (a,b) in enumerate(zip(actual,expected)):
                self.assertAlmostEqual(a,b,delta=max(1e-7, abs(b)*2e-6),msg=f'case {i}, column {column}')
        print(f'PASS: {len(cases)} vettori confrontati con C++ originale e imumaths installata')

    def test_known_edges(self):
        base = [1,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0]
        self.assertEqual(reference(base)[9:12], [0,0,0])
        base[11] = 180
        self.assertEqual(reference(base)[9], -1)
        base[11] = 359.99
        self.assertAlmostEqual(reference(base)[9], f32(-1/180))
        base[11] = -900
        self.assertEqual(reference(base)[9], -1)  # -360 % 360 == 0
        base[11] = -901
        self.assertLess(reference(base)[9], -1)  # resto C++ negativo
        base[0] = base[7] = 2
        self.assertEqual(reference(base)[5], 4)  # coniugato, nessuna inversa/normalizzazione


if __name__ == '__main__':
    unittest.main()
