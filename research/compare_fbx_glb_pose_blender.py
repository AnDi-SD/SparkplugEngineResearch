"""Compare evaluated FBX/GLB mesh positions through Blender's separate importers."""
import hashlib
import json
from pathlib import Path
import sys
import time

import bpy
import numpy as np


def capture(path, times, placements):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.render.fps = 30
    if path.suffix == ".fbx":
        bpy.ops.import_scene.fbx(filepath=str(path.resolve()), anim_offset=0)
    else:
        bpy.ops.import_scene.gltf(filepath=str(path.resolve()), merge_vertices=False)
    result = []
    for seconds in times:
        scene = bpy.context.scene
        frame = seconds * scene.render.fps / scene.render.fps_base
        scene.frame_set(int(frame), subframe=frame - int(frame))
        depsgraph = bpy.context.evaluated_depsgraph_get()
        snapshot = {}
        for name in placements:
            obj = bpy.data.objects.get(name)
            assert obj is not None and obj.type == "MESH", f"Missing mesh {name}"
            evaluated = obj.evaluated_get(depsgraph)
            mesh = evaluated.to_mesh()
            try:
                positions = np.empty(len(mesh.vertices) * 3, dtype=np.float32)
                mesh.vertices.foreach_get("co", positions)
                positions = positions.reshape(-1, 3).astype(np.float64)
                world = np.array(evaluated.matrix_world, dtype=np.float64)
                snapshot[name] = positions @ world[:3, :3].T + world[:3, 3]
            finally:
                evaluated.to_mesh_clear()
        result.append(snapshot)
    return result


def main():
    source, destination = map(Path, sys.argv[sys.argv.index("--") + 1:])
    data = json.loads(source.read_text(encoding="utf-8-sig"))
    report = {"status": "running", "blenderVersion": bpy.app.version_string, "pairs": [],
              "inputSha256": hashlib.sha256(source.read_bytes()).hexdigest().upper()}
    started = time.perf_counter()
    try:
        for pair in data["pairs"]:
            for suffix in ("fbx", "glb"):
                assert hashlib.sha256((source.parent / pair[suffix]).read_bytes()).hexdigest().upper() == pair[suffix + "Sha256"]
            glb = capture(source.parent / pair["glb"], pair["times"], pair["placements"])
            fbx = capture(source.parent / pair["fbx"], pair["times"], pair["placements"])
            result = {"fbx": pair["fbx"], "glb": pair["glb"], "samples": []}
            report["pairs"].append(result)
            for seconds, reference, actual in zip(pair["times"], glb, fbx):
                scene_points = np.concatenate(list(reference.values()), axis=0)
                scene_extent = float(np.ptp(scene_points, axis=0).max(initial=0))
                # Error accumulates along the entire rig, not just the size of
                # a small face/hand mesh influenced by that rig. 8 ppm is about
                # 67 float32 epsilons for the separate decomposition chains.
                tolerance = 0.00001 + scene_extent * 0.000008
                for name in pair["placements"]:
                    left, right = reference[name], actual[name]
                    assert left.shape == right.shape, f"Vertex count/order needs investigation: {name}"
                    error = float(np.linalg.norm(left - right, axis=1).max(initial=0))
                    extent = float(np.ptp(left, axis=0).max(initial=0))
                    result["samples"].append({"seconds": seconds, "mesh": name,
                                              "vertices": len(left), "maxPositionError": error,
                                              "extent": extent, "sceneExtent": scene_extent,
                                              "tolerance": tolerance,
                                              "passed": error <= tolerance})
            maximum = max(item["maxPositionError"] for item in result["samples"])
            result["maxPositionError"] = maximum
        # Preserve every measured error; the acceptance tolerance is explicit.
        assert all(sample["passed"] for pair in report["pairs"] for sample in pair["samples"]), "FBX/GLB evaluated pose differs; see per-mesh errors"
        report["status"] = "passed"
    except Exception as error:
        report["status"] = "failed"
        report["error"] = repr(error)
        raise
    finally:
        report["elapsedSeconds"] = time.perf_counter() - started
        destination.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("PASS: independent FBX/GLB evaluated bind and SAN pose comparison")


if __name__ == "__main__":
    main()
