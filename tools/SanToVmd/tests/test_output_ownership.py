"""File-output ownership only; no alternative SAN reader or sampler."""
from pathlib import Path
from types import SimpleNamespace
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import san_to_vmd as converter


class PreparedPose:
    mapping = {}
    neutral_bones = []
    disabled_ik = []

    def pose(self, clip, time):
        return {'センター': ((0, 0, 0), (0, 0, 0, 1))}


class OutputOwnershipTests(unittest.TestCase):
    def test_existing_sibling_is_preserved_on_success_and_failure(self):
        class FailingPose(PreparedPose):
            def pose(self, clip, time):
                raise ValueError('intentional output fixture failure')

        for fail in (False, True):
            with self.subTest(fail=fail), tempfile.TemporaryDirectory() as directory:
                target = Path(directory) / 'motion.vmd'
                sibling = target.with_suffix('.vmd.tmp')
                target.write_bytes(b'previous result')
                sibling.write_bytes(b'unrelated sibling')
                clip = SimpleNamespace(duration=0)
                if fail:
                    with self.assertRaisesRegex(ValueError, 'intentional'):
                        converter.write_vmd(target, clip, FailingPose(), 'test')
                    self.assertEqual(target.read_bytes(), b'previous result')
                else:
                    self.assertEqual(converter.write_vmd(target, clip, PreparedPose(), 'test'), (1, 1))
                    self.assertTrue(target.read_bytes().startswith(b'Vocaloid Motion Data 0002'))
                self.assertEqual(sibling.read_bytes(), b'unrelated sibling')
                self.assertEqual(set(Path(directory).iterdir()), {target, sibling})

    def test_overlapping_writes_own_distinct_temporary_files(self):
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / 'motion.vmd'
            clip = SimpleNamespace(duration=0)

            class NestedPose(PreparedPose):
                def pose(self, clip, time):
                    converter.write_vmd(target, clip, PreparedPose(), 'inner')
                    return super().pose(clip, time)

            self.assertEqual(converter.write_vmd(target, clip, NestedPose(), 'outer'), (1, 1))
            self.assertEqual(target.read_bytes()[30:50], b'outer'.ljust(20, b'\0'))
            self.assertEqual(list(Path(directory).iterdir()), [target])


if __name__ == '__main__':
    unittest.main()
