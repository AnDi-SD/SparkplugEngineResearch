"""Interop lifetime, seek and role-selection regressions."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import san_to_vmd as converter
from san_fixtures import wire, clip, animation


class OwnershipTests(unittest.TestCase):
    def setUp(self):
        self.bones = {'Root': converter.Bone('Root', None, (3, 4, 5)),
                      'Child': converter.Bone('Child', 'Root', (0, 2, 0))}
        self.order = ['Root', 'Child']

    def test_bound_scene_retains_animation_after_python_handle_closed(self):
        data = animation([('Root', {2: wire(1, (0,), ((9, 8, 7),))})])
        owner = converter.native.Animation(data)
        with converter.native.Scene(self.bones, self.order) as scene:
            scene.bind(owner, [0, -1, -1, -1, -1, -1])
            channel = owner.tracks[0].channels[0]
            owner.close()
            owner.close()
            self.assertEqual(scene.sample(0)[1][0], (9, 10, 7))
            with self.assertRaises(converter.ConversionError):
                channel.sample(0)
        with self.assertRaises(converter.ConversionError):
            scene.sample(0)

    def test_rebinding_resets_missing_roles_and_seeking_is_repeatable(self):
        with clip([('Root', {2: wire(1, (0, 1, 2), ((0, 0, 0), (10, 0, 0), (20, 0, 0)))})]) as first, \
             clip([('Child', {2: wire(1, (0,), ((0, 7, 0),))})]) as second, \
             converter.native.Scene(self.bones, self.order) as scene:
            a = converter.scene_world(scene, self.order, first, .25)
            converter.scene_world(scene, self.order, first, 1.75)
            self.assertEqual(converter.scene_world(scene, self.order, first, .25), a)
            b = converter.scene_world(scene, self.order, second, 0)
            self.assertEqual(b['Root'][0], (3, 4, 5))
            self.assertEqual(b['Child'][0], (3, 11, 5))
            self.assertEqual(converter.scene_world(scene, self.order, first, .25), a)

    def test_invalid_rebind_keeps_previous_scene_usable(self):
        with clip([('Root', {2: wire(1, (0,), ((9, 8, 7),))})]) as motion, \
             converter.native.Scene(self.bones, self.order) as scene:
            scene.bind(motion.native, [0, -1, -1, -1, -1, -1])
            before = scene.sample(0)
            with self.assertRaises(converter.ConversionError):
                scene.bind(motion.native, [99, -1, -1, -1, -1, -1])
            self.assertEqual(scene.sample(0), before)


if __name__ == '__main__':
    unittest.main()
