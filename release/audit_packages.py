#!/usr/bin/env python3
"""Audit canonical local packages without opening game assets or starting apps."""
from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
from pathlib import Path
import re
from urllib.parse import unquote, urlsplit
import zipfile

ROOT = Path(__file__).resolve().parents[1]
LINK = re.compile(r"!?\[[^\]\n]*\]\(\s*(<[^>\n]+>|[^\s)]+)(?:\s+[^)\n]*)?\)")
REFERENCE = re.compile(r"^\s{0,3}\[[^\]\n]+\]:\s*(<[^>\n]+>|\S+)")
FENCE = re.compile(r"^\s{0,3}(`{3,}|~{3,})")
NATIVE = ("SmoFbxBridge.exe", "libfbxsdk.dll", "FBX_SDK_License.rtf")


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest().upper()


def file_version(path: Path) -> str:
    version = ctypes.WinDLL("version", use_last_error=True)
    version.GetFileVersionInfoSizeW.argtypes = (wintypes.LPCWSTR, ctypes.POINTER(wintypes.DWORD))
    version.GetFileVersionInfoSizeW.restype = wintypes.DWORD
    version.GetFileVersionInfoW.argtypes = (wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p)
    version.GetFileVersionInfoW.restype = wintypes.BOOL
    version.VerQueryValueW.argtypes = (ctypes.c_void_p, wintypes.LPCWSTR, ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(wintypes.UINT))
    version.VerQueryValueW.restype = wintypes.BOOL
    size = version.GetFileVersionInfoSizeW(str(path), None)
    if not size:
        raise ctypes.WinError(ctypes.get_last_error())
    buffer = ctypes.create_string_buffer(size)
    if not version.GetFileVersionInfoW(str(path), 0, size, buffer):
        raise ctypes.WinError(ctypes.get_last_error())
    value, length = ctypes.c_void_p(), wintypes.UINT()
    if not version.VerQueryValueW(buffer, "\\", ctypes.byref(value), ctypes.byref(length)) or length.value < 52:
        raise ValueError(f"Missing fixed file version: {path}")
    words = ctypes.cast(value, ctypes.POINTER(wintypes.DWORD))
    if words[0] != 0xFEEF04BD:
        raise ValueError(f"Invalid fixed file version: {path}")
    return ".".join(map(str, (words[2] >> 16, words[2] & 65535, words[3] >> 16, words[3] & 65535)))


def audit(output: Path, archives: bool, products: list[str] | None = None) -> dict:
    manifest = json.loads((ROOT / "release/release-manifest.json").read_text(encoding="utf-8-sig"))
    errors, packages, payloads = [], [], {}
    markdown_count = link_count = 0

    def require(condition: bool, message: str):
        if not condition:
            errors.append(message)

    for product in manifest["products"]:
        if products and product['id'] not in products:
            continue
        candidates = [p for p in output.iterdir() if p.is_dir() and p.name.startswith(product["id"] + "-")]
        if len(candidates) != 1:
            errors.append(f"{product['id']}: expected one package, got {len(candidates)}")
            continue
        package = candidates[0]
        metadata = json.loads((package / "release.json").read_text(encoding="utf-8-sig"))
        release_version = metadata["version"]
        suffix = "-suite" if product.get("suite") else ""
        require(package.name == f"{product['id']}-{release_version}{suffix}-{manifest['runtimeIdentifier']}", f"{package.name}: unexpected package name")
        require(metadata["product"] == product["id"] and metadata["runtimeIdentifier"] == manifest["runtimeIdentifier"], f"{package.name}: metadata product/runtime mismatch")
        require(metadata["entryPoint"].replace("\\", "/") == "app/" + product["executable"], f"{package.name}: wrong entry point")
        expected = {"release.json", product["executable"], "app/" + product["executable"]}
        applications = [(product, package, "app/" + product["executable"])]
        applications += [(tool, package / "tools" / tool["id"], "tools/" + tool["id"] + "/" + tool["executable"]) for tool in product.get("tools", [])]
        for app, directory, executable in applications:
            expected.add(executable)
            for companion in app.get('companionFiles', []):
                require(bool(re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*\.dll', companion)), 'Invalid companion filename')
                expected.add((Path(executable).parent / companion).as_posix())
            payload = package / executable
            fingerprint = digest(payload)
            if app["id"] in payloads:
                require(fingerprint == payloads[app["id"]]["sha256"], f"{package.name}: suite/standalone payload differs for {app['id']}")
            else:
                payloads[app["id"]] = {"sha256": fingerprint, "fileVersion": file_version(payload)}
            for document in app.get("documents", []):
                source = ROOT / (document if isinstance(document, str) else document["source"])
                name = source.name if isinstance(document, str) else document["name"]
                destination = directory / "docs" / name
                expected.add(destination.relative_to(package).as_posix())
                require(destination.is_file() and digest(destination) == digest(source), f"{package.name}: document mismatch {destination.relative_to(package)}")
        if product.get("nativeFbx"):
            for name in NATIVE:
                expected.add("native/" + name)
                require(digest(package / "native" / name) == digest(ROOT / "tools/FbxBridge.Native/build/bin/Release" / name), f"{package.name}: native mismatch {name}")
        actual = {p.relative_to(package).as_posix() for p in package.rglob("*") if p.is_file()}
        require(actual == expected, f"{package.name}: missing={sorted(expected-actual)}, extra={sorted(actual-expected)}")
        expected_version = release_version.split("-", 1)[0].split("+", 1)[0]
        expected_version += ".0" * (4 - len(expected_version.split(".")))
        require(file_version(package / product["executable"]) == expected_version, f"{package.name}: launcher version mismatch")
        require(payloads[product["id"]]["fileVersion"] == expected_version, f"{package.name}: application version mismatch")
        files = [{"path": name, "size": (package / name).stat().st_size, "sha256": digest(package / name)} for name in sorted(actual)]
        hashes = [row["sha256"] for row in files]
        require(len(hashes) == len(set(hashes)), f"{package.name}: duplicate file content")
        for path in sorted(package.rglob("*.md")):
            markdown_count += 1
            fence = None
            for number, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), 1):
                marker = FENCE.match(line)
                if marker:
                    delimiter = marker.group(1)
                    if fence is None:
                        fence = delimiter
                    elif delimiter[0] == fence[0] and len(delimiter) >= len(fence):
                        fence = None
                    continue
                if fence:
                    continue
                matches = list(LINK.finditer(line))
                reference = REFERENCE.match(line)
                if reference:
                    matches.append(reference)
                for match in matches:
                    target = match.group(1).strip("<>")
                    parts = urlsplit(target)
                    if parts.scheme or parts.netloc or not parts.path:
                        continue
                    link_count += 1
                    resolved = (path.parent / unquote(parts.path).replace("\\", "/")).resolve()
                    require(resolved.is_relative_to(package) and resolved.exists(), f"{package.name}: broken/escaping link {path.relative_to(package)}:{number} -> {target}")
        row = {"package": package.name, "version": release_version, "files": files, "totalBytes": sum(f["size"] for f in files)}
        if archives:
            archive = output / (package.name + ".zip")
            with zipfile.ZipFile(archive) as zipped:
                entries = [e for e in zipped.infolist() if not e.is_dir()]
                require(len(entries) == len(actual), f"{archive.name}: unexpected entry count")
                require({e.filename.replace("\\", "/") for e in entries} == {package.name + "/" + p for p in actual}, f"{archive.name}: unexpected archive paths")
                inventory = {package.name + "/" + f["path"]: f for f in files}
                for entry in entries:
                    expected_file = inventory.get(entry.filename.replace("\\", "/"))
                    if expected_file:
                        with zipped.open(entry) as stream:
                            zipped_hash = hashlib.file_digest(stream, "sha256").hexdigest().upper()
                        require(zipped_hash == expected_file["sha256"], f"{archive.name}: payload mismatch {entry.filename}")
            row["archive"] = {"path": archive.name, "size": archive.stat().st_size, "sha256": digest(archive)}
        packages.append(row)
    return {"schemaVersion": 1, "status": "failed" if errors else "passed", "packages": packages, "payloads": payloads,
            "markdownFiles": markdown_count, "relativeLinks": link_count, "errors": errors,
            "limitations": ["Checks package contents and Windows file versions, not execution.", "Remote URLs and heading anchors are not checked.", "Links use a bounded Markdown pattern, not a full parser."]}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--archives", action="store_true")
    parser.add_argument("--product", action="append", choices=[p['id'] for p in json.loads((ROOT/'release/release-manifest.json').read_text(encoding='utf-8-sig'))['products']])
    parser.add_argument("--report", required=True, type=Path)
    args = parser.parse_args()
    report = audit(args.directory.resolve(), args.archives, args.product)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({k: v for k, v in report.items() if k not in {"packages", "payloads"}}))
    return int(bool(report["errors"]))


if __name__ == "__main__":
    raise SystemExit(main())
