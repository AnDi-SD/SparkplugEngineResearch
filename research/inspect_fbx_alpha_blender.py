"""Independently import the focused FBX alpha fixtures in Blender 4.5.

blender --background --factory-startup --disable-autoexec --threads 4
  --python research/inspect_fbx_alpha_blender.py -- expected.json report.json

The expected pixels are the original decoded SMO/synthetic BGRA, before export.
No exporter PNG decoder or FBX SDK reader is reused. Source files are not saved.
"""

import hashlib
import json
from pathlib import Path
import re
import sys
import time

import bpy
import numpy as np


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def image_pixels(image):
    values = np.empty(len(image.pixels), dtype=np.float32)
    image.pixels.foreach_get(values)
    return values.reshape(image.size[1], image.size[0], 4)


def source_image(socket):
    if not socket.is_linked:
        return None, None
    assert len(socket.links) == 1, "Unexpected multiple shader input links"
    link = socket.links[0]
    assert link.from_node.type == "TEX_IMAGE", "Unexpected alpha shader topology"
    assert link.from_node.image is not None, "Missing embedded FBX image"
    return link.from_node.image, link.from_socket.name


def main():
    expected_path, report_path = map(Path, sys.argv[sys.argv.index("--") + 1:])
    expected = json.loads(expected_path.read_text(encoding="utf-8-sig"))
    started = time.perf_counter()
    report = {
        "schemaVersion": 1, "blenderVersion": bpy.app.version_string,
        "expectedSha256": digest(expected_path), "exports": [], "checks": 0,
        "alphaTolerance": expected["alphaTolerance"],
        "status": "running", "maxAlphaError": 0.0,
    }
    try:
        for entry in expected["exports"]:
            source = expected_path.parent / entry["file"]
            assert digest(source) == entry["sha256"], "FBX changed after export"
            bpy.ops.wm.read_factory_settings(use_empty=True)
            result = bpy.ops.import_scene.fbx(filepath=str(source.resolve()), use_anim=True)
            assert result == {"FINISHED"}, "Blender FBX import failed"
            record = {"file": entry["file"], "sha256": digest(source), "materials": []}
            report["exports"].append(record)
            for mesh in entry["meshes"]:
                material = bpy.data.materials.get(mesh["name"])
                assert material is not None, f"Missing material {mesh['name']}"
                shaders = [node for node in material.node_tree.nodes if node.type == "BSDF_PRINCIPLED"]
                assert len(shaders) == 1, "Unexpected shader count"
                shader = shaders[0]
                alpha_image, alpha_channel = source_image(shader.inputs["Alpha"])
                color_image, color_channel = source_image(shader.inputs["Base Color"])
                texture = mesh["texture"]
                observed = {"name": mesh["name"], "materialAlpha": mesh["materialAlpha"],
                            "alphaChannel": alpha_channel, "alphaImage": None,
                            "scalarAlpha": float(shader.inputs["Alpha"].default_value)}
                matching_objects = [obj for obj in bpy.data.objects if obj.type == "MESH"
                                    and material in list(obj.data.materials)]
                assert matching_objects, "Material is not assigned to any mesh"
                for obj in matching_objects:
                    assert len(obj.data.vertices) == mesh["vertexCount"], "Vertex count changed"
                    if mesh["skin"] is not None:
                        source_skin = mesh["skin"]
                        modifiers = [modifier for modifier in obj.modifiers if modifier.type == "ARMATURE"]
                        assert len(modifiers) == 1, "One SMO skin must remain one connected Blender armature"
                        rig = modifiers[0].object
                        weights_path = expected_path.parent / source_skin["file"]
                        assert digest(weights_path) == source_skin["sha256"], "Expected weights changed"
                        raw_weights = np.frombuffer(weights_path.read_bytes(), dtype="<f4").reshape(-1, 8)
                        joint_names = source_skin["joints"]
                        expected_weights = np.zeros((mesh["vertexCount"], len(joint_names)), dtype=np.float64)
                        for vertex, values in enumerate(raw_weights):
                            for slot in range(4):
                                if values[slot] > 0:
                                    expected_weights[vertex, int(values[slot + 4])] += float(values[slot])
                        observed_weights = np.zeros_like(expected_weights)
                        group_slots = {}
                        for group in obj.vertex_groups:
                            # FBX/Blender make per-mesh palette clone names unique.
                            canonical = group.name if group.name in joint_names else re.sub(r"\.\d{3,}$", "", group.name)
                            if canonical in joint_names:
                                assert group.name in rig.data.bones, "A weighted group lost its bone"
                                group_slots[group.index] = joint_names.index(canonical)
                        for vertex in obj.data.vertices:
                            for group in vertex.groups:
                                if group.weight > 0:
                                    assert group.group in group_slots, "Unexpected weighted bone"
                                    observed_weights[vertex.index, group_slots[group.group]] += group.weight
                        weight_error = float(np.abs(observed_weights - expected_weights).max(initial=0))
                        assert weight_error < 0.000003, f"Lost or changed weights in {obj.name}: {weight_error}"
                        observed["skin"] = {"armature": rig.name, "boneCount": len(rig.data.bones),
                                            "maxWeightError": weight_error, "vertexCount": mesh["vertexCount"]}
                if texture is None:
                    assert color_image is None and alpha_image is None, "Unexpected texture connection"
                    error = abs(observed["scalarAlpha"] - mesh["materialAlpha"])
                    comparisons = 1
                else:
                    pixels_path = expected_path.parent / texture["pixels"]
                    assert digest(pixels_path) == texture["sha256"], "Expected BGRA changed"
                    raw = np.frombuffer(pixels_path.read_bytes(), dtype=np.uint8).reshape(
                        texture["height"], texture["width"], 4)[::-1]
                    assert color_image is not None, "Missing base color image"
                    assert tuple(color_image.size) == (texture["width"], texture["height"])
                    color_values = image_pixels(color_image)
                    rgb = raw[:, :, [2, 1, 0]].astype(np.float64) / 255
                    # Blender exposes byte image buffers as normalized file
                    # values; floating point buffers are scene-linear.
                    if color_image.is_float and color_image.colorspace_settings.name == "sRGB":
                        rgb = np.where(rgb <= 0.04045, rgb / 12.92, ((rgb + 0.055) / 1.055) ** 2.4)
                    rgb_error = float(np.abs(color_values[:, :, :3] - rgb).max())
                    assert rgb_error < 0.000002, f"RGB changed in {mesh['name']}: {rgb_error}"
                    observed["maxRgbError"] = rgb_error
                    observed["baseImageDepth"] = color_image.depth
                    observed["baseImageFloat"] = color_image.is_float
                    if mesh["usesTextureAlpha"] and texture["hasAlpha"]:
                        assert alpha_image is not None, "Texture alpha was not connected"
                        assert tuple(alpha_image.size) == tuple(color_image.size)
                        values = image_pixels(alpha_image)
                        if alpha_channel == "Alpha":
                            values = values[:, :, 3]
                        else:
                            assert alpha_channel == "Color"
                            assert alpha_image.colorspace_settings.name == "Non-Color", "Opacity got color corrected"
                            values = values[:, :, 0]
                        reference = raw[:, :, 3].astype(np.float64) / 255 * mesh["materialAlpha"]
                        error = float(np.abs(values - reference).max())
                        observed["alphaImage"] = {
                            "name": alpha_image.name, "depth": alpha_image.depth,
                            "colorspace": alpha_image.colorspace_settings.name,
                            "min": float(values.min()), "max": float(values.max()),
                        }
                        comparisons = int(values.size)
                    else:
                        assert alpha_image is None, "Opaque material acquired texture alpha"
                        error = abs(observed["scalarAlpha"] - mesh["materialAlpha"])
                        comparisons = 1
                observed["maxAlphaError"] = error
                observed["alphaComparisons"] = comparisons
                record["materials"].append(observed)
                assert error <= expected["alphaTolerance"], f"Opacity changed in {mesh['name']}: {error}"
                report["maxAlphaError"] = max(report["maxAlphaError"], error)
                report["checks"] += comparisons + 1
        report["status"] = "passed"
    except Exception as error:
        report["status"] = "failed"
        report["error"] = repr(error)
        raise
    finally:
        report["elapsedSeconds"] = time.perf_counter() - started
        report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"PASS: Blender independent FBX alpha, {report['checks']} checks; max error {report['maxAlphaError']:.9g}")


if __name__ == "__main__":
    main()
