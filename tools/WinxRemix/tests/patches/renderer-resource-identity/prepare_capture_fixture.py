#!/usr/bin/env python3
"""Extract the pinned renderer's actual capture identity seams for CPU checks.

The recording test supplies renderer/GPU/USD boundary types. This generator
does not rewrite function bodies or claim to compile the complete renderer.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re


def sha(data):
    return hashlib.sha256(data).hexdigest()


def balanced_function(text, marker):
    if text.count(marker) != 1:
        raise ValueError(f"Expected exactly one function marker: {marker}")
    start = text.index(marker)
    brace = text.index("{", start)
    depth = 0
    state = "code"
    escape = False
    index = brace
    while index < len(text):
        char = text[index]
        following = text[index:index + 2]
        if state == "line":
            if char == "\n":
                state = "code"
        elif state == "block":
            if following == "*/":
                state = "code"
                index += 1
        elif state in ("string", "character"):
            if escape:
                escape = False
            elif char == "\\":
                escape = True
            elif char == ('"' if state == "string" else "'"):
                state = "code"
        elif following == "//":
            state = "line"
            index += 1
        elif following == "/*":
            state = "block"
            index += 1
        elif char == '"':
            state = "string"
        elif char == "'":
            state = "character"
        elif char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return start, index + 1
        index += 1
    raise ValueError(f"Unbalanced function: {marker}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-directory", type=Path, required=True)
    parser.add_argument("--output-directory", type=Path, required=True)
    args = parser.parse_args()
    source = args.source_directory.resolve() / "src/dxvk/rtx_render"
    output = args.output_directory.resolve()
    output.mkdir(parents=True, exist_ok=True)
    specs = [
        ("capture_draw_members.inc", "rtx_types.h", [
            "const XXH64_hash_t getHash(const HashRule& rule) const",
            "const XXH64_hash_t getAssetHash(const HashRule& rule) const"]),
        ("capture_instance_members.inc", "rtx_instance_manager.h", [
            "const XXH64_hash_t& getMaterialDataHash() const",
            "const XXH64_hash_t& getCaptureMaterialHash() const",
            "const XXH64_hash_t& getCaptureMeshAssetHash() const"]),
        ("capture_functions.inc", "rtx_game_capturer.cpp", [
            "void GameCapturer::newInstance(",
            "void GameCapturer::captureMaterial("]),
    ]
    records = []
    sources = {}

    def read(name):
        if name not in sources:
            data = (source / name).read_bytes()
            # Keep the raw source digest; spans normalize only Windows line endings.
            sources[name] = (data, data.decode("utf-8").replace("\r\n", "\n"))
        return sources[name][1]

    def write(name, pieces):
        data = "".join(pieces).encode("utf-8")
        target = output / name
        with target.open("xb") as stream:
            stream.write(data)
        return sha(data)

    def piece(file, marker):
        text = read(file)
        first, end = balanced_function(text, marker)
        exact = text[first:end]
        records.append({"source": file, "marker": marker,
                        "firstLine": text.count("\n", 0, first) + 1,
                        "bytes": len(exact.encode("utf-8")),
                        "sha256": sha(exact.encode("utf-8"))})
        return exact + "\n"

    outputs = {}
    for name, file, markers in specs:
        outputs[name] = write(name, [piece(file, marker) for marker in markers])

    seam_specs = [
        ("capture_assign.inc", [
            r"currentInstance\.m_materialDataHash = drawCall\.getMaterialData\(\)\.getHash\(\);",
            r"currentInstance\.m_captureMaterialHash = drawCall\.getGeometryData\(\)\.externalAssetIdentity\.materialHashOr\(currentInstance\.m_materialDataHash\);",
            r"currentInstance\.m_captureMeshAssetHash = drawCall\.getGeometryData\(\)\.externalAssetIdentity\.meshHashOr\(kEmptyHash\);"]),
        ("capture_copy.inc", [
            r"m_materialDataHash = src\.m_materialDataHash;",
            r"m_captureMaterialHash = src\.m_captureMaterialHash;",
            r"m_captureMeshAssetHash = src\.m_captureMeshAssetHash;"]),
    ]
    file = "rtx_instance_manager.cpp"
    text = read(file)
    for name, patterns in seam_specs:
        matches = []
        for pattern in patterns:
            found = list(re.finditer(r"(?m)^[ \t]*" + pattern + r"[ \t]*$", text))
            if len(found) != 1:
                raise ValueError(f"Expected exactly one assignment seam: {pattern}")
            matches.append(found[0])
        if any(text[left.end():right.start()].strip() for left, right in zip(matches, matches[1:])):
            raise ValueError(f"Expected adjacent assignment seams: {name}")
        exact = text[matches[0].start():matches[-1].end()]
        records.append({"source": file, "marker": name,
                        "firstLine": text.count("\n", 0, matches[0].start()) + 1,
                        "bytes": len(exact.encode("utf-8")),
                        "sha256": sha(exact.encode("utf-8"))})
        outputs[name] = write(name, [exact + "\n"])

    manifest = {
        "schemaVersion": 1,
        "qualification": "Actual extracted capture functions and identity seams with recording boundaries; no renderer/GPU/USD execution.",
        "sources": [{"path": name, "bytes": len(value[0]), "sha256": sha(value[0])}
                    for name, value in sorted(sources.items())],
        "spans": records,
        "outputs": outputs,
    }
    with (output / "capture-fixture-extraction.json").open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(manifest, stream, indent=2)
        stream.write("\n")
    print(f"Extracted {len(records)} exact spans into {len(outputs)} fresh includes")


if __name__ == "__main__":
    main()
