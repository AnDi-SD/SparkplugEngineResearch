"""Small synthetic regressions. Run: python -m unittest discover -s tools/SanToVmd/tests -v"""

import math
import contextlib
import io
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

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


def pmd_skeleton(rows):
    """Minimal PMD through IK section; enough for the converter's skeleton reader."""
    raw = b"Pmd" + struct.pack("<f", 1) + b"test".ljust(20, b"\0") + bytes(256)
    raw += struct.pack("<IIIH", 0, 0, 0, len(rows))  # No mesh data.
    for name, parent, kind in rows:
        raw += name.encode("cp932").ljust(20, b"\0")
        raw += struct.pack("<HHBH3f", parent, 0, kind, 0, 0, 0, 0)
    return raw + struct.pack("<H", 0)  # No IK chains.


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

    def test_extra_source_branch_is_ignored_but_body_ancestors_remain(self):
        # Hair/cape siblings must not enter the body graph, even with unsupported
        # scale. An unfamiliar ancestor of Head still contributes its motion.
        source = {
            "Root": converter.Bone("Root", None, (0, 0, 0)),
            "ExtraParent": converter.Bone("ExtraParent", "Root", (0, 2, 0)),
            "Head": converter.Bone("Head", "ExtraParent", (0, 1, 0)),
            "Hair_01": converter.Bone("Hair_01", "Head", (0, 1, 0), scale=(2, 2, 2)),
            "Cape_01": converter.Bone("Cape_01", "Root", (0, 0, 1), scale=(3, 3, 3)),
        }
        qz = (0, 0, math.sqrt(0.5), math.sqrt(0.5))
        curve = converter.Curve(1, (0,), (qz,))
        clip = converter.Clip(1, {"ExtraParent": {3: [curve]}}, 0, [])
        order = converter.hierarchy(source, ["Head"])
        self.assertEqual(order, ["Root", "ExtraParent", "Head"])
        actual = converter.source_world(source, order, clip)
        pruned = {name: source[name] for name in order}
        self.assertEqual(actual, converter.source_world(pruned, order, clip))
        for a, b in zip(actual["Head"][0], (-1, 2, 0)):
            self.assertAlmostEqual(a, b)
        with self.assertRaises(converter.ConversionError):
            converter.source_world(source, converter.hierarchy(source, ["Hair_01"]), clip)

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
            neutral_bones = []

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

    def test_duplicate_pmd_endpoints_keep_separate_identities(self):
        rows = [("root", 65535, 1), ("end", 0, 7), ("end", 0, 7)]
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)/"test.pmd"
            path.write_bytes(pmd_skeleton(rows))
            _, bones, _ = converter.read_pmd(path)
        self.assertEqual(len(bones), 3)
        ends = [bone for bone in bones.values() if bone.name == "end"]
        self.assertEqual(len(ends), 2)
        self.assertTrue(all(bone.parent == "root" for bone in ends))

    def test_ambiguous_animated_pmd_names_still_rejected(self):
        for rows in (
            [("root", 65535, 1), ("arm", 0, 0), ("arm", 0, 0)],
            [("root", 65535, 1), ("end", 0, 7), ("end", 0, 7), ("child", 1, 0)],
        ):
            with self.subTest(rows=rows), tempfile.TemporaryDirectory() as folder:
                path = Path(folder)/"test.pmd"
                path.write_bytes(pmd_skeleton(rows))
                with self.assertRaises(converter.ConversionError):
                    converter.read_pmd(path)

    def test_input_selection_is_case_insensitive_and_never_guesses(self):
        with tempfile.TemporaryDirectory() as folder:
            directory = Path(folder)
            with self.assertRaisesRegex(converter.ConversionError, "Положите в input"):
                converter.single_file(directory, ".smo", "скелет")
            (directory/"Icy.SMO").write_bytes(b"")
            self.assertEqual(converter.single_file(directory, ".smo", "скелет").name, "Icy.SMO")
            (directory/"Bloom.smo").write_bytes(b"")
            with self.assertRaisesRegex(converter.ConversionError, "Оставьте только один"):
                converter.single_file(directory, ".smo", "скелет")
            (directory/"nested").mkdir()
            (directory/"nested"/"hidden.san").write_bytes(b"")
            self.assertEqual(converter.find_files(directory, ".san"), [])

    def test_empty_input_creates_folders_and_has_no_project_fallback(self):
        with tempfile.TemporaryDirectory() as folder:
            directory, output = Path(folder)/"input", Path(folder)/"output"
            with patch.object(converter, "read_skeleton") as reader:
                with self.assertRaisesRegex(converter.ConversionError, "Положите в input"):
                    converter.convert_files(directory, output)
                reader.assert_not_called()
            self.assertTrue(directory.is_dir())
            self.assertTrue(output.is_dir())

    def test_one_bad_san_does_not_stop_batch_or_destroy_old_vmd(self):
        class TestRig:
            order = ["Head"]
            mapping = {"頭": "Head"}
            neutral_bones = ["twist"]
            disabled_ik = []
            scale = 1

            def pose(self, clip, time):
                return {name: (converter.ZERO, converter.IDENTITY)
                        for name in ("センター", "頭", "twist")}

        track = named_track("Head", vector_curve([(0, 1, 0)], (0,)))
        raw = container([(1, "example", 0x56EE563A, field(0, struct.pack("<f", 1))+track)])
        with tempfile.TemporaryDirectory() as folder:
            directory, output = Path(folder)/"input", Path(folder)/"output"
            directory.mkdir(); output.mkdir()
            for name in ("model.smo", "model.pmd"):
                (directory/name).write_bytes(b"reader mocked in this batch test")
            (directory/"a_bad.san").write_bytes(b"broken")
            (directory/"z_good.SAN").write_bytes(raw)
            (output/"a_bad.vmd").write_bytes(b"previous result")
            (output/"z_good.vmd").write_bytes(b"outdated result")
            with patch.object(converter, "read_skeleton", return_value={}), \
                 patch.object(converter, "read_pmd", return_value=("test", {}, [])), \
                 patch.object(converter, "Retargeter", return_value=TestRig()), \
                 contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(converter.convert_files(directory, output), 1)
            self.assertEqual((output/"a_bad.vmd").read_bytes(), b"previous result")
            self.assertTrue((output/"z_good.vmd").read_bytes().startswith(b"Vocaloid Motion Data 0002"))
            self.assertFalse(list(output.glob("*.tmp")))


if __name__ == "__main__":
    unittest.main()
