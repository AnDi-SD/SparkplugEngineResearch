"""Local corpus check with an independently downloaded MMD Tools VMD reader.

No network access. Usage from repository root:
python tools/SanToVmd/tests/validate_local.py --reader local-data/mmd-research/mmd_tools_vmd_reader.py
"""

import argparse
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import san_to_vmd as converter

ROOT = Path(__file__).resolve().parents[3]


def mmd_world(target, keys, frame):
    """Independent column-matrix FK from actual VMD keys and PMD rest positions."""
    result = {}
    identity = [[int(i == j) for j in range(4)] for i in range(4)]
    for name in converter.hierarchy(target, list(target)):
        bone = target[name]
        position = bone.position if bone.parent is None else tuple(
            a-b for a, b in zip(bone.position, target[bone.parent].position))
        q = (0, 0, 0, 1)
        if name in keys:
            key = keys[name][frame]
            position = tuple(a+b for a, b in zip(position, key.location))
            q = key.rotation
        x, y, z, w = q
        local = [
            [1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w), position[0]],
            [2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w), position[1]],
            [2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y), position[2]],
            [0, 0, 0, 1],
        ]
        parent = result.get(bone.parent, identity)
        result[name] = [[sum(parent[i][k]*local[k][j] for k in range(4))
                         for j in range(4)] for i in range(4)]
    return {name: tuple(m[i][3] for i in range(3)) for name, m in result.items()}


def main():
    args = argparse.ArgumentParser(description=__doc__)
    args.add_argument("--reader", required=True, type=Path)
    args.add_argument("--vmd-dir", type=Path, default=ROOT/"local-data/mmd-research/test-vmd-v01")
    args.add_argument("--report", type=Path, default=ROOT/"local-data/mmd-research/validation-v01.json")
    args = args.parse_args()
    specification = importlib.util.spec_from_file_location("external_vmd", args.reader)
    external = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(external)
    source_path = ROOT/"local-data/bloom_jeans.smo"
    target_path = ROOT/"local-data/mmd/MikuMikuDanceE_v932/UserFile/Model/Miku_Hatsune.pmd"
    source = converter.read_skeleton(source_path)
    model_name, target, iks = converter.read_pmd(target_path)
    rig = converter.Retargeter(source, target, iks)
    paths = sorted((ROOT/"local-data/pc-pristine/Media/Characters/Bloom").glob("*.san"))
    if not paths:
        raise RuntimeError("Local Bloom corpus is missing")
    total_keys, max_direction_error = 0, 0.0
    for path in paths:
        clip = converter.read_san(path, set(rig.order))
        vmd = args.vmd_dir/(path.stem+".vmd")
        with vmd.open("rb") as stream:
            header, bones = external.Header(), external.BoneAnimation()
            header.load(stream)
            bones.load(stream)
            sections = [external.ShapeKeyAnimation(), external.CameraAnimation(),
                        external.LightAnimation(), external.SelfShadowAnimation(), external.PropertyAnimation()]
            for section in sections:
                section.load(stream)  # Do not use File.load: it tolerates truncated sections.
            assert stream.tell() == vmd.stat().st_size, path
        assert header.model_name == model_name
        assert set(bones) == set(rig.mapping) | {"センター"}
        count = converter.frame_count(clip.duration)
        for frames in bones.values():
            assert [f.frame_number for f in frames] == list(range(count)), path
            previous = None
            for key in frames:
                assert all(math.isfinite(v) for v in (*key.location, *key.rotation))
                assert abs(sum(v*v for v in key.rotation)-1) < 1e-5
                if previous:
                    assert sum(a*b for a, b in zip(previous, key.rotation)) >= -1e-6
                previous = key.rotation
                for axis in range(4):
                    assert key.interp[axis] == key.interp[axis+4]
                    assert key.interp[axis+8] == key.interp[axis+12]
                total_keys += 1
        assert not any(sections[:-1]), path
        properties = sections[-1]
        assert len(properties) == 1 and properties[0].visible and properties[0].frame_number == 0
        assert dict(properties[0].ik_states) == {name: False for name in converter.LEG_IK}

        # Body poses are checked from decoded VMD, not from the writer's pose dict.
        # This catches lost parent motion, double rotations and wrong rest-angle correction.
        if path.stem in ("blwa", "blru", "bljusm", "blidlo", "blabe"):
            for frame in (0, count//2, count-1):
                posed = mmd_world(target, bones, frame)
                original = converter.source_world(source, rig.order, clip, min(frame/30, clip.duration))
                for side in ("左", "右"):
                    for a, b in (("肩", "腕"), ("腕", "ひじ"), ("ひじ", "手首"),
                                 ("足", "ひざ"), ("ひざ", "足首")):
                        a, b = side+a, side+b
                        first = converter.sub(posed[b], posed[a])
                        second = converter.sub(original[rig.mapping[b]][0], original[rig.mapping[a]][0])
                        first = converter.times(first, 1/converter.length(first))
                        second = converter.times(second, 1/converter.length(second))
                        error = converter.length(converter.sub(first, second))
                        max_direction_error = max(max_direction_error, error)
                        assert error < 0.002, (path.name, frame, a, error)
    report = {
        "converter_sha256": hashlib.sha256(Path(converter.__file__).read_bytes()).hexdigest(),
        "independent_reader_sha256": hashlib.sha256(args.reader.read_bytes()).hexdigest(),
        "reader_source": "https://github.com/MMD-Blender/blender_mmd_tools/blob/main/mmd_tools/core/vmd/__init__.py",
        "files": len(paths), "keys": total_keys, "target_tracks": len(rig.mapping)+1,
        "source_nodes": len(rig.order), "motion_scale": rig.scale,
        "max_limb_direction_error": max_direction_error,
        "original_mmd_visual_playback_verified": False,
    }
    args.report.write_text(json.dumps(report, indent=2)+"\n", encoding="utf-8")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
