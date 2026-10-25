"""Extract the applied production external-draw function for a recording CPU fixture.

No renderer code is rewritten or executed here. The fixture supplies recording
stand-ins for its renderer/GPU collaborators; it includes the exact extracted
function rather than maintaining a second implementation of its routing policy.
"""

import argparse
import hashlib
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    source_path = args.source.resolve(strict=True)
    source = source_path.read_bytes()
    text = source.decode("utf-8").replace("\r\n", "\n")
    marker = "  void SceneManager::submitExternalDraw(Rc<DxvkContext> ctx, ExternalDrawState&& state) {\n"
    following = "\n  void SceneManager::destroyExternalMesh("
    if text.count(marker) != 1:
        raise ValueError("Expected exactly one production external-draw function")
    start = text.index(marker)
    end = text.index(following, start)
    extracted = text[start:end].rstrip() + "\n"
    if not extracted.endswith("  }\n") or extracted.count("SceneManager::submitExternalDraw(") != 1:
        raise ValueError("Unexpected production external-draw function boundary")
    args.output.mkdir(parents=True, exist_ok=False)
    include = args.output / "external_draw.inc"
    include.write_bytes(extracted.encode("utf-8"))
    manifest = {
        "schemaVersion": 1,
        "scope": "exact production submitExternalDraw body with recording CPU collaborators",
        "rendererExecuted": False,
        "gpuExecuted": False,
        "source": str(source_path),
        "sourceSha256": hashlib.sha256(source).hexdigest(),
        "sourceFirstLine": text[:start].count("\n") + 1,
        "extractedFile": include.name,
        "extractedSha256": hashlib.sha256(include.read_bytes()).hexdigest(),
    }
    (args.output / "extraction.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(manifest))


if __name__ == "__main__":
    main()
