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
import struct
import sys
import tempfile
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import san_to_vmd as converter

ROOT = Path(__file__).resolve().parents[3]


def poison_ignored_curves(path, ignored):
    """Keep real SAN body keys, but replace unused PRS with unsupported curves.

    This is a test-only FFPS writer. It preserves fields, names and durations;
    representation 99 must fail if an unused track ever reaches the decoder.
    Original game files are never modified.
    """
    entries = converter.read_ffps(path)
    assert len(entries) == 1
    object_id, entry = next(iter(entries.items()))
    output, pending, changed = bytearray(), [], 0

    def emit(kind, payload):
        header = bytes([0xE0 | kind]) if kind < 31 else bytes([0xFF, kind])
        return header + struct.pack("<I", len(payload)) + payload

    for kind, payload in converter.fields(entry.data):
        if kind in (2, 3, 4):
            pending.append((kind, payload))
            continue
        if kind == 1:
            reader = converter.Reader(payload)
            name = reader.text(reader.number("H"))
            reader.done()
            for role, curve in pending:
                if name in ignored:
                    curve = struct.pack("<I", 99)
                    changed += 1
                output += emit(role, curve)
            pending.clear()
        output += emit(kind, payload)
    assert not pending
    body = struct.pack("<I4s", entry.kind, b"SBOO") + output + b"\0"
    name = entry.name.encode("latin1") + b"\0"
    table = struct.pack("<IH", object_id, len(name)) + name
    table += struct.pack("<III", entry.kind, 0, len(body)) + bytes(4)
    start = 32 + len(table)
    header = struct.pack("<4s7I", b"FFPS", 0x26, 0, start+len(body), 2, start, len(body), 1)
    return header + table + body, changed


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


def main(argv=None):
    args = argparse.ArgumentParser(description=__doc__)
    args.add_argument("--reader", required=True, type=Path)
    args.add_argument("--vmd-dir", type=Path, default=ROOT/"local-data/mmd-research/test-vmd-v01")
    args.add_argument("--report", type=Path, default=ROOT/"local-data/mmd-research/validation-v01.json")
    args.add_argument("--input-dir", type=Path, default=ROOT/"local-data/pc-pristine/Media/Characters/Bloom")
    args.add_argument("--skeleton", type=Path, default=ROOT/"local-data/bloom_jeans.smo")
    args.add_argument("--model", type=Path,
                      default=ROOT/"local-data/mmd/MikuMikuDanceE_v932/UserFile/Model/Miku_Hatsune.pmd")
    args.add_argument("--check-ignored", action="store_true",
                      help="Poison unused SAN curves and compare the entire resulting VMD byte for byte")
    args = args.parse_args(argv)
    started = time.perf_counter()
    specification = importlib.util.spec_from_file_location("external_vmd", args.reader)
    external = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(external)
    source_path, target_path = args.skeleton, args.model
    source = converter.read_skeleton(source_path)
    model_name, target, iks = converter.read_pmd(target_path)
    rig = converter.Retargeter(source, target, iks)
    paths = sorted(p for p in args.input_dir.iterdir() if p.is_file() and p.suffix.lower() == ".san")
    if not paths:
        raise RuntimeError("Local SAN corpus is missing")
    total_keys, max_direction_error = 0, 0.0
    evidence, ignored_names, unchanged_files, poisoned_channels = [], set(), 0, 0
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
        assert dict(properties[0].ik_states) == {name: False for name in rig.disabled_ik}

        # Body poses are checked from decoded VMD, not from the writer's pose dict.
        # This catches lost parent motion, double rotations and wrong rest-angle correction.
        # Every clip, including character-specific actions, gets three pose checks.
        for frame in sorted({0, count//2, count-1}):
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
        ignored_names.update(clip.ignored)
        if args.check_ignored and clip.ignored:
            raw, changed = poison_ignored_curves(path, set(clip.ignored))
            assert changed > 0, path
            # A new temporary directory for each clip bounds both disk and RAM usage.
            with tempfile.TemporaryDirectory(prefix="san-vmd-ignored-") as folder:
                changed_san, changed_vmd = Path(folder)/path.name, Path(folder)/vmd.name
                changed_san.write_bytes(raw)
                changed_clip = converter.read_san(changed_san, set(rig.order))
                converter.write_vmd(changed_vmd, changed_clip, rig, model_name)
                assert changed_vmd.read_bytes() == vmd.read_bytes(), path
                try:
                    converter.read_san(changed_san, set(rig.order) | set(clip.ignored))
                except converter.ConversionError as error:
                    assert "99" in str(error), error
                else:
                    raise AssertionError("Mutation did not poison unused curves")
            unchanged_files += 1
            poisoned_channels += changed
        evidence.append({"san": path.name, "san_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                         "vmd_sha256": hashlib.sha256(vmd.read_bytes()).hexdigest(),
                         "frames": count, "ignored_tracks": clip.ignored,
                         "missing_tracks": sorted(set(rig.order)-set(clip.tracks))})
    report = {
        "converter_sha256": hashlib.sha256(Path(converter.__file__).read_bytes()).hexdigest(),
        "independent_reader_sha256": hashlib.sha256(args.reader.read_bytes()).hexdigest(),
        "reader_source": "https://github.com/MMD-Blender/blender_mmd_tools/blob/main/mmd_tools/core/vmd/__init__.py",
        "validator_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "skeleton": source_path.as_posix(), "skeleton_sha256": hashlib.sha256(source_path.read_bytes()).hexdigest(),
        "model": target_path.name, "model_sha256": hashlib.sha256(target_path.read_bytes()).hexdigest(),
        "files": len(paths), "keys": total_keys, "target_tracks": len(rig.mapping)+1,
        "source_nodes": len(rig.order), "total_source_nodes": len(source), "motion_scale": rig.scale,
        "ignored_tracks": sorted(ignored_names),
        "extra_track_mutation_identical_files": unchanged_files,
        "poisoned_unused_prs_channels": poisoned_channels,
        "pose_check": "three frames per clip; ten arm/leg segment directions from independently decoded VMD",
        "max_limb_direction_error": max_direction_error,
        "original_mmd_visual_playback_verified": False,
        "elapsed_seconds": round(time.perf_counter()-started, 3),
        "inputs": evidence,
    }
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2)+"\n", encoding="utf-8")
    print(json.dumps({k: v for k, v in report.items() if k != "inputs"}, indent=2))
    return report


if __name__ == "__main__":
    main()
