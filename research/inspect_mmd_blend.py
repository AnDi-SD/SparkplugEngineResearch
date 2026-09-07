"""Read texts and skeletons from the supplied .blend without running its scripts.

Run with Blender's --background --factory-startup --disable-autoexec flags,
then --python this_script.py -- input.blend output_directory.
This is an inspection helper, not a SAN converter. The source is never saved.
"""

import hashlib
import json
from pathlib import Path
import sys

import bpy


source, destination = map(Path, sys.argv[sys.argv.index("--") + 1:])
destination.mkdir(parents=True, exist_ok=True)

# Loading datablocks as a library avoids opening the supplied UI/session and
# avoids loading all meshes/textures. Text datablocks are data, not executed code.
with bpy.data.libraries.load(str(source.resolve()), link=False) as (available, chosen):
    inventory = {
        kind: list(getattr(available, kind))
        for kind in ("texts", "armatures", "actions", "objects", "scenes")
    }
    chosen.texts = available.texts
    chosen.armatures = available.armatures
    chosen.actions = available.actions

report = {
    "source": str(source),
    "sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
    "blender_version": bpy.app.version_string,
    "inventory": inventory,
    "texts": [],
    "armatures": [],
    "actions": [],
}
for index, block in enumerate(chosen.texts):
    # Use our own filenames: a text datablock name may contain a directory path.
    target = destination / f"embedded_text_{index:02d}.txt"
    contents = block.as_string()
    target.write_text(contents, encoding="utf-8")
    report["texts"].append({
        "name": block.name, "file": target.name,
        "lines": len(contents.splitlines()), "characters": len(contents),
    })
for armature in chosen.armatures:
    report["armatures"].append({
        "name": armature.name,
        "bones": [{
            "name": bone.name,
            "parent": bone.parent.name if bone.parent else None,
            "head": list(bone.head_local), "tail": list(bone.tail_local),
            "matrix": [list(row) for row in bone.matrix_local],
            "deform": bone.use_deform,
        } for bone in armature.bones],
    })
for action in chosen.actions:
    report["actions"].append({
        "name": action.name, "frame_range": list(action.frame_range),
    })
(destination / "blend_inventory.json").write_text(
    json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
print(json.dumps({
    "texts": report["texts"],
    "armatures": [{"name": a["name"], "bones": len(a["bones"])}
                  for a in report["armatures"]],
    "actions": report["actions"],
}, ensure_ascii=True))
