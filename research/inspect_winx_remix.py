#!/usr/bin/env python3
"""Read-only Winx/Remix inventory; no game launch, injection, or configuration edits."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
from datetime import datetime, timezone
from pathlib import Path

from inspect_renderers import PC_GRAPHICS_BODIES
from inspect_serializer_manager import PC_SHA256, image_slice, read_pe, sha256


def fingerprint(path: Path) -> dict:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return {"bytes": path.stat().st_size, "sha256": digest.hexdigest().upper()}


def inspect(game: Path, checksums: Path | None) -> dict:
    data = (game / "WinxClub.exe").read_bytes()
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    machine = struct.unpack_from("<H", data, pe + 4)[0]
    magic = struct.unpack_from("<H", data, pe + 24)[0]
    if machine != 0x14C or magic != 0x10B:
        raise ValueError("The existing Winx evidence reader requires x86 PE32")
    base, sections = read_pe(data)
    endpoints = []
    for name, (address, size, expected) in PC_GRAPHICS_BODIES.items():
        actual = sha256(image_slice(data, sections, address - base, size))
        endpoints.append({"name": name, "address": f"0x{address:08X}",
                          "bytes": size, "sha256": actual,
                          "matchesOriginalEvidence": actual == expected})

    official = {}
    if checksums is not None:
        for line in checksums.read_text(encoding="utf-8-sig").splitlines():
            fields = line.split()
            if len(fields) == 12 and re.fullmatch(r"[0-9A-Fa-f]{64}", fields[2]):
                package_path = fields[-1].replace("\\", "/")
                if package_path.startswith("remix/"):
                    official[package_path[6:]] = fields[2].upper()
        if not official:
            raise ValueError("No runtime SHA256 records in the supplied release checksum list")

    files = {}
    selected = ["WinxClub.exe", "d3d9.dll", ".trex/d3d9.dll",
                ".trex/NvRemixBridge.exe", "NvRemixLauncher32.exe",
                "rtx.conf", "user.conf", "winx.ini", ".trex/bridge.conf",
                "rtx-remix/logs/remix-dxvk.log", "rtx-remix/logs/bridge32.log",
                "rtx-remix/logs/bridge64.log", "GameStateLog.txt"]
    selected += [p.relative_to(game).as_posix() for p in sorted((game / "Shaders").glob("*")) if p.is_file()]
    for relative in selected:
        path = game / relative
        if path.is_file():
            files[relative] = fingerprint(path)
            if relative in official:
                files[relative]["officialReleaseSha256"] = official[relative]
                files[relative]["matchesOfficialRelease"] = files[relative]["sha256"] == official[relative]

    configs = {}
    invalid_lines = []
    for name in ("rtx.conf", "user.conf", ".trex/bridge.conf"):
        path = game / name
        if not path.is_file():
            continue
        content = path.read_text(encoding="utf-8-sig")
        configs[name] = content
        for index, line in enumerate(content.splitlines(), 1):
            stripped = line.strip()
            if stripped and not stripped.startswith(("#", "[", ";", "//")) and "=" not in stripped:
                invalid_lines.append({"file": name, "line": index, "text": line})

    log_path = game / "rtx-remix/logs/remix-dxvk.log"
    log = log_path.read_text(encoding="utf-8-sig") if log_path.is_file() else ""
    relevant = [{"line": index, "text": line} for index, line in enumerate(log.splitlines(), 1)
                if re.search(r"DXVK_Remix:|not detecting a valid camera|rejected an invalid camera|Did not find app config", line)]
    return {
        "schemaVersion": 1,
        "observedAtUtc": datetime.now(timezone.utc).isoformat(),
        "gameDirectory": str(game.resolve()),
        "method": "File inspection and comparison with existing original-code evidence; no live frame capture",
        "gameMachine": "x86 PE32",
        "gameMatchesPristineResearchExe": files["WinxClub.exe"]["sha256"] == PC_SHA256,
        "pristineResearchExeSha256": PC_SHA256,
        "rendererEndpoints": endpoints,
        "files": files,
        "configs": configs,
        "configLinesMissingEquals": invalid_lines,
        "logObservations": relevant,
        "officialChecksumList": fingerprint(checksums) if checksums else None,
        "limitations": ["A matching function body does not validate the whole modified EXE or live state.",
                        "Camera warnings do not identify a draw, projection values, or the cause of the black screen.",
                        "Only selected runtime files are compared with official checksums.",
                        "Input files must be idle; this inspector does not take an atomic filesystem snapshot."]
    }


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", type=Path, default=root / "local-data/Winx Club")
    parser.add_argument("--release-checksums", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = inspect(args.game, args.release_checksums)
    encoded = json.dumps(result, ensure_ascii=False, indent=2) + "\n"
    if args.output:
        args.output.write_text(encoded, encoding="utf-8")
        print(json.dumps({"output": str(args.output),
                          "rendererEndpointsMatched": sum(x["matchesOriginalEvidence"] for x in result["rendererEndpoints"]),
                          "rendererEndpointsChecked": len(result["rendererEndpoints"]),
                          "runtimeFilesMatched": sum(x.get("matchesOfficialRelease", False) for x in result["files"].values()),
                          "runtimeFilesChecked": sum("matchesOfficialRelease" in x for x in result["files"].values()),
                          "configLinesMissingEquals": result["configLinesMissingEquals"]}, ensure_ascii=False))
    else:
        print(encoded, end="")


if __name__ == "__main__":
    main()
