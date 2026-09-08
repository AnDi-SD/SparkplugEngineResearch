"""Interop lifetime, seek and role-selection regressions."""
from pathlib import Path
import sys
import struct
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import san_to_vmd as converter
from san_fixtures import wire, clip, animation, field, container


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


class GraphTests(unittest.TestCase):
    def skeleton(self, rotation=(0, 0, 0, 1), root_class=0x695C0F65):
        child = field(0, struct.pack('<3f', 1, 0, 0))
        root = field(1, struct.pack('<4f', *rotation)) + field(5, struct.pack('<II', 2, 0))
        if root_class == 0x603625D0:
            root += b'\0'  # Node section before the empty RenderNode section.
        raw = container([(2, 'Child', 0x695C0F65, child), (1, 'Root', root_class, root)])
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)/'synthetic.smo';path.write_bytes(raw)
            result = converter.read_skeleton(path)
        self.addCleanup(result.close)
        return result

    def test_graph_preserves_nonunit_authored_matrix_and_retains_nodes(self):
        source = self.skeleton((0, 0, .5, .5))
        with converter.native.Scene(source, ['Root', 'Child']) as scene:
            source.close()
            # Original ToMatrix, with no normalization or quaternion round trip.
            self.assertEqual(scene.sample(0)[1][0], (.5, .5, 0))

    def test_graph_includes_derived_node_ancestors(self):
        source = self.skeleton(root_class=0x603625D0)
        self.assertEqual(source['Child'].parent, 'Root')
        order = converter.hierarchy(source, ['Child'])
        self.assertEqual(order, ['Root', 'Child'])
        self.assertEqual(converter.source_world(source, order)['Child'][0], (1, 0, 0))

    def test_graph_scene_requires_ancestors_and_failure_leaves_graph_usable(self):
        source = self.skeleton()
        with self.assertRaises(converter.ConversionError):
            converter.native.Scene(source, ['Child'])
        self.assertEqual(converter.source_world(source, ['Root', 'Child'])['Child'][0], (1, 0, 0))

    def test_two_graphs_and_scenes_have_independent_node_state(self):
        first = self.skeleton();second = self.skeleton()
        with converter.native.Scene(first, ['Root', 'Child']) as a, \
             converter.native.Scene(second, ['Root', 'Child']) as b, \
             clip([('Root', {2: wire(1, (0,), ((9, 8, 7),))})]) as motion:
            a.bind(motion.native, [0, -1, -1, -1, -1, -1])
            self.assertEqual(a.sample(0)[1][0], (10, 8, 7))
            first.close()
            self.assertEqual(b.sample(0)[1][0], (1, 0, 0))
            self.assertEqual(a.sample(0)[1][0], (10, 8, 7))

    def test_unknown_resource_is_not_silently_skipped(self):
        raw = container([(1, 'Root', 0x695C0F65, b''), (2, 'unused', 0xDEADBEEF, b'')])
        with self.assertRaises(converter.ConversionError):
            converter.native.Graph(raw)


if __name__ == '__main__':
    unittest.main()
