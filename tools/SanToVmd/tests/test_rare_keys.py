"""Focused PC SAN representation/endpoint regressions; no local game assets."""
import math
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import san_to_vmd as converter


def wire(representation, times, rows):
    return (struct.pack('<II', representation, len(times)) +
            struct.pack('<' + 'f'*len(times), *times) +
            b''.join(struct.pack('<' + 'f'*len(row), *row) for row in rows))


class RareKeyTests(unittest.TestCase):
    def assertVector(self, actual, expected, places=6):
        self.assertEqual(len(actual), len(expected))
        for a, b in zip(actual, expected):
            self.assertAlmostEqual(a, b, places=places)

    def test_packed_cubic_uses_vector_tangents_and_rewrites_file_coefficients(self):
        # For axis X: 2u + 2u², Y and Z have independent scales/signs.
        a = (0, 0, 0, 999, 999, 999, 2, -4, 6, *([float('nan')]*6))
        b = (4, -8, 12, 6, -12, 18, 0, 0, 0, *([float('nan')]*6))
        curve = converter.read_curve(wire(2, (2, 6), (a, b)), 2)[0]
        self.assertVector(curve.sample(3), (0.625, -1.25, 1.875))
        self.assertVector(curve.sample(4), (1.5, -3, 4.5))
        self.assertVector(curve.sample(6), (0, 0, 0))

    def test_scalar_rotations_use_radians_and_zyx_order(self):
        # X reaches its second of three keys while Y is halfway to its second.
        payload = wire(3, (0, 1, 2), ((0,), (math.pi/2,), (math.pi/2,)))
        payload += wire(3, (0, 2, 4), ((0,), (math.pi,), (math.pi,)))
        payload += wire(3, (0,), ((0,),))
        actual = converter.sample(converter.read_curve(payload, 3), 1, converter.IDENTITY, True)
        # qY(pi/2) * qX(pi/2): X maps to -Z, Y maps to +X.
        self.assertVector(converter.rotate(actual, (1, 0, 0)), (0, 0, -1))
        self.assertVector(converter.rotate(actual, (0, 1, 0)), (1, 0, 0))

    def test_mixed_scalar_linear_and_cubic_rotations(self):
        payload = wire(4, (0, 2, 4), ((0, 0, 2, 99, 99), (4, 6, 0, 99, 99), (4, 0, 0, 99, 99)))
        payload += wire(3, (0,), ((0,),)) + wire(4, (0,), ((0, 0, 0, 99, 99),))
        actual = converter.sample(converter.read_curve(payload, 3), 1, converter.IDENTITY, True)
        self.assertVector(actual, (math.sin(.75), 0, 0, math.cos(.75)))

    def test_quaternion_cubic_recomputes_controls_and_differs_from_linear(self):
        quaternions = [(0, 0, math.sin(a/2), math.cos(a/2)) for a in (0, .4, 2)]
        payload = wire(2, (0, 1, 2), [(*q, *([float('nan')]*4)) for q in quaternions])
        curve = converter.read_curve(payload, 3)[0]
        # Unwrapped same-axis angles: controls -0.1, 0.1, 2.4.
        # At u=.5 in interval 0: endpoint angle .2; control angle 0; result .1.
        self.assertVector(curve.sample(.5), (0, 0, math.sin(.05), math.cos(.05)))
        self.assertGreater(abs(curve.sample(.5)[2] - math.sin(.1)), .04)
        self.assertVector(curve.sample(2), quaternions[-1])

    def test_single_cubic_quaternion_is_bounded_and_repeated(self):
        q = (0, 0, .6, .8)
        curve = converter.read_curve(wire(2, (.5,), ((*q, 99, 99, 99, 99),)), 3)[0]
        for time in (-1, .5, 10):
            self.assertVector(curve.sample(time), q)

    def test_pc_two_key_endpoint_applies_to_linear_rotation_too(self):
        q = (0, 0, math.sin(.5), math.cos(.5))
        curve = converter.read_curve(wire(1, (0, 1), (converter.IDENTITY, q)), 3)[0]
        self.assertGreater(curve.sample(.999)[2], .47)
        self.assertVector(curve.sample(1), converter.IDENTITY)
        self.assertVector(curve.sample(2), converter.IDENTITY)

    def test_native_small_angle_rule_copies_first_rotation(self):
        q = (0, 0, math.sin(.0005), math.cos(.0005))
        self.assertEqual(converter.slerp(converter.IDENTITY, q, .75), converter.IDENTITY)

    def test_frame_time_uses_native_float32_before_endpoint_selection(self):
        last = struct.unpack('<f', struct.pack('<f', 20/30))[0]
        self.assertGreater(last, 20/30)
        curves = converter.read_curve(wire(1, (0, last), ((0, 0, 0), (10, 0, 0))), 2)
        source = {'Root': converter.Bone('Root', None, converter.ZERO)}
        clip = converter.Clip(last, {'Root': {2: curves}}, 0, [])
        self.assertVector(converter.source_world(source, ['Root'], clip, 20/30)['Root'][0], (0, 0, 0))

    def test_cubic_unit_scale_allowed_but_between_key_scale_rejected(self):
        row = (1, 1, 1, 0, 0, 0, 0, 0, 0, *([float('nan')]*6))
        payload = wire(2, (0, 1, 2), (row, row, row))
        curves = converter.read_curve(payload, 4)
        self.assertVector(converter.sample(curves, .5, (1, 1, 1)), (1, 1, 1))
        tangent = (*row[:6], .01, *row[7:])
        with self.assertRaisesRegex(converter.ConversionError, 'масштаба'):
            converter.read_curve(wire(2, (0, 1, 2), (tangent, row, row)), 4)

    def test_empty_and_incomplete_rare_encodings_reject(self):
        for rep in (2, 3, 4):
            with self.subTest(rep=rep), self.assertRaises(converter.ConversionError):
                converter.read_curve(wire(rep, (), ()), 3)
        scalar = wire(3, (0,), ((0,),))
        for suffix in (b'', struct.pack('<I', 0), wire(1, (0,), ((0, 0, 0),))):
            with self.assertRaises(converter.ConversionError):
                converter.read_curve(scalar + suffix, 3)


if __name__ == '__main__':
    unittest.main()
