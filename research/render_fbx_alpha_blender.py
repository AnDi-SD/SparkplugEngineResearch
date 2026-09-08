"""Small GPU material render of the independently imported FBX alpha fixture."""
import json
from pathlib import Path
import sys
import time

import bpy
from mathutils import Vector

source, destination = map(Path, sys.argv[sys.argv.index("--") + 1:])
destination.mkdir(parents=True, exist_ok=True)
started = time.perf_counter()
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(source.resolve()))
scene = bpy.context.scene
scene.render.engine = "BLENDER_EEVEE_NEXT"
scene.render.resolution_x = 1024
scene.render.resolution_y = 88
scene.render.resolution_percentage = 100
scene.render.film_transparent = True
scene.render.image_settings.file_format = "PNG"
scene.render.image_settings.color_mode = "RGBA"
scene.render.filepath = str((destination / "alpha-ramp.png").resolve())
if hasattr(scene.eevee, "taa_render_samples"):
    scene.eevee.taa_render_samples = 128
world = bpy.data.worlds.new("White environment")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (1, 1, 1, 1)
scene.world = world
camera_data = bpy.data.cameras.new("Alpha validation camera")
camera_data.type = "ORTHO"
camera_data.ortho_scale = 256
camera_data.clip_end = 1000
camera = bpy.data.objects.new("Alpha validation camera", camera_data)
scene.collection.objects.link(camera)
camera.location = (128, -300, 10.5)
camera.rotation_euler = (Vector((128, 0, 10.5)) - camera.location).to_track_quat("-Z", "Y").to_euler()
scene.camera = camera
sun_data = bpy.data.lights.new("Validation light", "SUN")
sun_data.energy = 2
sun = bpy.data.objects.new("Validation light", sun_data)
scene.collection.objects.link(sun)
sun.rotation_euler = camera.rotation_euler
bpy.ops.render.render(write_still=True)
report = {"status": "rendered", "engine": scene.render.engine,
          "blenderVersion": bpy.app.version_string, "file": "alpha-ramp.png",
          "resolution": [1024, 88], "elapsedSeconds": time.perf_counter() - started}
try:
    import gpu
    report["gpuRenderer"] = gpu.platform.renderer_get()
    report["gpuVendor"] = gpu.platform.vendor_get()
except Exception as error:
    report["gpuQuery"] = repr(error)
(destination / "render-report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
print(json.dumps(report))
