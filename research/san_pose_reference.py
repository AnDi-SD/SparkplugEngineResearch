"""Independent NumPy FK/skinning from recorded source inputs and PC PRS.

Returns right-handed Blender Z-up points for target/Viewer comparisons.
"""
import numpy as np


def local_matrix(position, quaternion, scale):
    x, y, z, w = quaternion
    column = np.array([[1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w)],
                       [2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w)],
                       [2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)]], dtype=np.float64)
    result = np.eye(4)
    result[:3, :3] = np.diag(scale) @ column.T
    result[3, :3] = position
    return result


def expected(reference, tracks):
    nodes = {row['index']: row for row in reference['nodes']}
    worlds, active = {}, set()

    def world(index):
        if index in worlds: return worlds[index]
        assert index not in active, 'Reference node cycle'
        active.add(index)
        node = nodes[index]
        channels = tracks.get(node['name'], {})
        position, rotation, scale = node['position'], node['rotation'], node['scale']
        if '2' in channels:
            x, y, z = channels['2']; position = (x, y, -z)
        if '3' in channels:
            x, y, z, w = channels['3']; rotation = (-x, -y, z, w)
        if '4' in channels: scale = channels['4']
        local = local_matrix(position, rotation, scale)
        parent = world(node['parent']) if node['parent'] in nodes else np.eye(4)
        worlds[index] = local @ parent
        active.remove(index)
        return worlds[index]

    for index in nodes: world(index)
    meshes = {row['index']: row for row in reference['meshes']}
    skins = {row['index']: row for row in reference['skins']}
    result = {}
    for placement in reference['placements']:
        mesh = meshes[placement['mesh']]
        positions = np.array(mesh['positions'], dtype=np.float64)
        points = np.concatenate((positions, np.ones((len(positions), 1))), axis=1)
        if mesh['skin'] in skins:
            skin = skins[mesh['skin']]
            transforms = [np.array(inverse, dtype=np.float64).reshape(4, 4) @ worlds[joint]
                          for joint, inverse in zip(skin['joints'], skin['inverseBinds'])]
            weights = np.array(mesh['weights'], dtype=np.float64)
            joints = np.array(mesh['joints'], dtype=np.int64)
            posed = np.zeros((len(points), 4))
            total = weights.sum(axis=1)
            for slot in range(4):
                valid = weights[:, slot] > 0
                if not valid.any(): continue
                matrices = np.array([transforms[joint] for joint in joints[valid, slot]])
                posed[valid] += np.einsum('ni,nij->nj', points[valid], matrices)*weights[valid, slot, None]
            # Target skinning normalizes active weights. Original imported
            # weights remain separately validated; no epsilon weight is added.
            active_weights = total > 0
            posed[active_weights] /= total[active_weights, None]
            posed[~active_weights] = points[~active_weights] @ np.array(placement['world']).reshape(4, 4)
        else:
            transform = np.array(placement['world'], dtype=np.float64).reshape(4, 4)
            if placement['parent'] in nodes:
                parent = placement['parent']
                transform = transform @ np.linalg.inv(np.array(nodes[parent]['bindWorld']).reshape(4, 4)) @ worlds[parent]
            posed = points @ transform
        # Both importers convert exported right-handed Y-up into Blender Z-up:
        # (x,y,z) -> (x,-z,y). This is a coordinate basis, not animation motion.
        result[placement['name']] = posed[:, [0, 2, 1]] * np.array([1, -1, 1])
    return result


