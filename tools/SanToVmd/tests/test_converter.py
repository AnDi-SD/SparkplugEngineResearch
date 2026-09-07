"""Small synthetic regressions. Run: python -m unittest discover -s tools/SanToVmd/tests -v"""

import math
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import san_to_vmd as converter


def field(kind, payload):
    return bytes([0xE0 | kind]) + struct.pack("<I", len(payload)) + payload


def container(objects):
    """Synthetic FFPS with separate physical objects and explicit child references."""
    table, body = bytearray(), bytearray()
    for number, name, kind, fields in objects:
        name = name.encode("ascii") + b"\0"
        obj = struct.pack("<I4s", kind, b"SBOO") + fields + b"\0"
        table += struct.pack("<IH", number, len(name)) + name
        table += struct.pack("<III", kind, len(body), len(obj))
        body += obj
    table += bytes(4)
    start = 32 + len(table)
    return struct.pack("<4s7I", b"FFPS", 0x26, 0, start+len(body), 2, start, len(body), len(objects)) + table + body


def vector_curve(values, times=None):
    times = times if times is not None else tuple(range(len(values)))
    return struct.pack("<II", 1, len(values)) + struct.pack("<"+"f"*len(times), *times) + b"".join(
        struct.pack("<3f", *value) for value in values)


def named_track(name, payload):
    name = name.encode("ascii") + b"\0"
    return field(2, payload) + field(1, struct.pack("<H", len(name)) + name)


class ConverterTests(unittest.TestCase):
    def test_active_quaternion_and_shortest_arc(self):
        q = (0, 0, math.sqrt(0.5), math.sqrt(0.5))
        actual = converter.rotate(q, (1, 0, 0))
        for a, b in zip(actual, (0, 1, 0)):
            self.assertAlmostEqual(a, b)
        midway = converter.slerp(q, converter.times(q, -1), 0.5)
        self.assertAlmostEqual(abs(sum(a*b for a, b in zip(q, midway))), 1)

    def test_different_arm_rest_angles_are_aligned(self):
        original, desired = (1, 0, 0), (0, -2, 0)
        q = converter.align_directions(original, desired)
        for a, b in zip(converter.rotate(q, original), (0, -1, 0)):
            self.assertAlmostEqual(a, b)
        q = converter.align_directions(original, (-1, 0, 0))
        self.assertAlmostEqual(converter.rotate(q, original)[0], -1)

    def test_cubic_recomputes_coefficients_and_normalizes_time(self):
        curve = converter.Curve(4, (2, 6), ((0, 0, 2, 999, 999), (4, 6, 0, 999, 999)))
        # Analytic polynomial 2*u + 2*u^2; file coefficient placeholders are ignored.
        self.assertAlmostEqual(curve.sample(3)[0], 0.625)
        self.assertAlmostEqual(curve.sample(4)[0], 1.5)
        self.assertEqual(curve.sample(10), (4,))

    def test_scalar_axes_have_independent_times(self):
        payload = bytearray()
        for endpoint in (1.0, 2.0, 4.0):
            payload += struct.pack("<II2f2f", 3, 2, 0, endpoint, 0, 8)
        curves = converter.read_curve(payload, 2)
        self.assertEqual(converter.sample(curves, 1, (0, 0, 0)), (8, 4, 2))

    def test_empty_scale_keeps_rest_and_nonidentity_scale_rejected(self):
        empty = converter.read_curve(struct.pack("<II", 1, 0), 4)
        self.assertEqual(converter.sample(empty, 1, (1, 1, 1)), (1, 1, 1))
        with self.assertRaises(converter.ConversionError):
            converter.read_curve(vector_curve([(1, 1, 1), (2, 1, 1)]), 4)

    def test_invalid_counts_and_times_rejected(self):
        with self.assertRaises(converter.ConversionError):
            converter.read_curve(struct.pack("<II", 1, 0xFFFFFFFF), 2)
        with self.assertRaises(converter.ConversionError):
            converter.read_curve(vector_curve([(0, 0, 0)]*2, (1, 1)), 2)

    def test_child_relationship_instead_of_physical_nesting(self):
        q = (0, 0, math.sqrt(0.5), math.sqrt(0.5))
        root = field(1, struct.pack("<4f", *q)) + field(5, struct.pack("<I", 12))
        child = field(0, struct.pack("<3f", 1, 0, 0))
        raw = container([(11, "parent", 0x695C0F65, root), (12, "child", 0x695C0F65, child)])
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)/"example.smo"
            path.write_bytes(raw)
            bones = converter.read_skeleton(path)
        self.assertEqual(bones["child"].parent, "parent")
        actual = converter.source_world(bones, converter.hierarchy(bones, ["child"]))["child"][0]
        for a, b in zip(actual, (0, 1, 0)):
            self.assertAlmostEqual(a, b)

    def test_cycles_rejected(self):
        bones = {"a": converter.Bone("a", "b", (0, 0, 0)), "b": converter.Bone("b", "a", (0, 0, 0))}
        with self.assertRaises(converter.ConversionError):
            converter.hierarchy(bones, ["a"])

    def test_parent_rotation_is_not_applied_twice(self):
        # A 90-degree root turn must rotate lower body once and leave the local
        # head rotation neutral, even when the source head has a nonidentity bind.
        qz = (0, 0, math.sqrt(0.5), math.sqrt(0.5))
        qy = (0, math.sqrt(0.5), 0, math.sqrt(0.5))
        source = {
            "Root": converter.Bone("Root", None, (0, 0, 0)),
            "Pelvis": converter.Bone("Pelvis", "Root", (0, 0, 0)),
            "Head": converter.Bone("Head", "Pelvis", (0, 1, 0), qy),
        }
        target = {
            "センター": converter.Bone("センター", None, (0, 0, 0)),
            "下半身": converter.Bone("下半身", "センター", (0, 0, 0)),
            "頭": converter.Bone("頭", "下半身", (0, 1, 0)),
        }
        # Test the pose operation on a deliberately tiny, fully known rig.
        rig = converter.Retargeter.__new__(converter.Retargeter)
        rig.source, rig.target = source, target
        rig.mapping = {"下半身": "Pelvis", "頭": "Head"}
        rig.alignment = {name: converter.IDENTITY for name in rig.mapping}
        rig.order = converter.hierarchy(source, ["Head"])
        rig.target_order = converter.hierarchy(target, ["頭"])
        rig.rest = converter.source_world(source, rig.order)
        rig.scale = 1
        curve = converter.Curve(1, (0,), (qz,))
        clip = converter.Clip(1, {"Root": {3: [curve]}}, 0, [])
        pose = rig.pose(clip, 0)
        self.assertAlmostEqual(abs(pose["頭"][1][3]), 1)
        self.assertAlmostEqual(abs(sum(a*b for a, b in zip(pose["下半身"][1], qz))), 1)

    def test_unknown_and_duplicate_unused_tracks_do_not_block_body(self):
        track = named_track("Head", vector_curve([(0, 1, 0)], (0,)))
        unsupported = named_track("movement_tracker", struct.pack("<I", 99))
        body = field(0, struct.pack("<f", 1)) + track + unsupported*2
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)/"example.san"
            path.write_bytes(container([(1, "example", 0x56EE563A, body)]))
            clip = converter.read_san(path, {"Head"})
            self.assertIn("Head", clip.tracks)
            self.assertEqual(len(clip.ignored), 2)
            with self.assertRaises(converter.ConversionError):
                converter.read_san(path, {"movement_tracker"})

    def test_failed_write_preserves_existing_file(self):
        class BrokenRetargeter:
            mapping = {"頭": "Head"}
            disabled_ik = []

            def pose(self, clip, time):
                raise converter.ConversionError("Synthetic failure")

        with tempfile.TemporaryDirectory() as folder:
            target = Path(folder)/"motion.vmd"
            target.write_bytes(b"previous result")
            with self.assertRaises(converter.ConversionError):
                converter.write_vmd(target, converter.Clip(1, {}, 0, []), BrokenRetargeter(), "test")
            self.assertEqual(target.read_bytes(), b"previous result")
            self.assertFalse(target.with_suffix(".vmd.tmp").exists())

    def test_frame_duration_and_byte_limited_names(self):
        float32_duration = struct.unpack("<f", struct.pack("<f", 1.2))[0]
        self.assertEqual(converter.frame_count(float32_duration), 37)
        self.assertEqual(len(converter.encoded_name("左つま先ＩＫ", 15)), 15)
        with self.assertRaises(converter.ConversionError):
            converter.encoded_name("あ"*8, 15)


if __name__ == "__main__":
    unittest.main()
