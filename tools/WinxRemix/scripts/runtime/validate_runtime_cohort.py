"""Read-only byte/contract validation of an explicitly inventoried PC runtime.

This is our launch preparation tool, not recovered game code. It never builds,
loads a DLL, launches a process or certifies executed runtime compatibility.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct


MAX_JSON = 4 * 1024 * 1024
MAX_FILES = 4096
MAX_FILE = 512 * 1024 * 1024
MAX_TOTAL = 768 * 1024 * 1024
HASH = re.compile(r"^[0-9a-fA-F]{64}$")
INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"\r\n]+)"', re.MULTILINE)
SHOW_INCLUDE = "Note: including file:"
INCLUDE_PREFIXES = (SHOW_INCLUDE, "Примечание: включение файла:")
MACHINES = {"x86": (0x14c, 0x10b), "x64": (0x8664, 0x20b)}
ROLES = {"game", "adapter", "d3d8to9", "client", "server", "renderer", "config", "launchDriver",
         "dependencyLock", "generatedDependencies", "apiHeader", "adapterBuild", "toolchain",
         "source", "generatedHeader", "sdkHeader", "compiler", "linker", "environment",
         "buildCommand", "includeLog", "includeLogRaw", "runtimeProvenance", "patchManifest", "patch"}
EXTERNAL = {"compiler", "linker", "environment", "sdkHeader"}


class CohortError(ValueError):
    pass


def require(condition, reason):
    if not condition:
        raise CohortError(reason)


def pairs(values):
    result = {}
    for key, value in values:
        require(key not in result, f"Duplicate JSON key: {key}")
        result[key] = value
    return result


def read_limited(path):
    require(path.stat().st_size <= MAX_JSON, f"Text exceeds bound: {path}")
    with path.open("rb") as stream:
        body = stream.read(MAX_JSON + 1)
    require(len(body) <= MAX_JSON, f"Text grew beyond bound: {path}")
    return body


def load_json(path):
    value = json.loads(read_limited(path).decode("utf-8-sig"), object_pairs_hook=pairs)
    require(isinstance(value, dict), f"JSON object required: {path}")
    return value


def inside(path, root):
    return path == root or root in path.parents


def key(path):
    # Windows runtime paths are case-insensitive even in portable fixture tests.
    return str(path).replace("\\", "/").casefold()


def locate(root, value):
    require(isinstance(value, str) and value and "\x00" not in value, "Invalid file path")
    path = Path(value)
    if not path.is_absolute():
        require(".." not in path.parts, f"Relative path escapes declared root: {value}")
        path = root / path
    return path.resolve(strict=True)


def hash_file(path, expected_size):
    with path.open("rb") as stream:
        descriptor = stream.fileno()
        stat = os.fstat(descriptor)
        require(stat.st_size == expected_size, f"Size mismatch: {path}")
        digest = hashlib.sha256()
        remaining = expected_size
        while remaining:
            block = stream.read(min(1024 * 1024, remaining))
            require(block, f"File truncated during read: {path}")
            digest.update(block)
            remaining -= len(block)
        require(not stream.read(1), f"File grew during read: {path}")
        after = os.fstat(descriptor)
        require((stat.st_size, stat.st_mtime_ns, stat.st_ino) ==
                (after.st_size, after.st_mtime_ns, after.st_ino), f"File changed during read: {path}")
        return digest.hexdigest()


def pe_machine(path, expected):
    require(expected in MACHINES, f"Unknown PE architecture: {expected}")
    with path.open("rb") as stream:
        dos = stream.read(64)
        require(len(dos) == 64 and dos[:2] == b"MZ", f"Missing DOS header: {path}")
        offset = struct.unpack_from("<I", dos, 60)[0]
        require(64 <= offset <= 1024 * 1024, f"Unbounded PE header offset: {path}")
        stream.seek(offset)
        header = stream.read(26)
    require(len(header) == 26 and header[:4] == b"PE\0\0", f"Missing PE header: {path}")
    optional_bytes = struct.unpack_from("<H", header, 20)[0]
    require(optional_bytes >= 2 and offset + 24 + optional_bytes <= path.stat().st_size,
            f"Truncated PE optional header: {path}")
    require((struct.unpack_from("<H", header, 4)[0], struct.unpack_from("<H", header, 24)[0]) ==
            MACHINES[expected], f"PE architecture mismatch ({expected}): {path}")


def validate(manifest, root):
    """Validate declared inputs against current bytes; raise on the first mismatch."""
    root = Path(root).resolve(strict=True)
    manifest = locate(root, str(manifest))
    require(inside(manifest, root), "Manifest must be inside workspace")
    data = load_json(manifest)
    require(data.get("schema") == 1 and type(data.get("schema")) is int, "Unsupported cohort schema")
    require(isinstance(data.get("name"), str) and re.fullmatch(r"[A-Za-z0-9_-]{1,100}", data["name"]), "Invalid cohort name")
    backend = data.get("backend")
    require(backend in ("system", "remix"), "Explicit backend required")
    rows = data.get("files")
    require(isinstance(rows, list) and 0 < len(rows) <= MAX_FILES, "Invalid file inventory bound")
    files, roles, paths = {}, {}, {}
    total = 0
    for row in rows:
        require(isinstance(row, dict), "File row must be an object")
        ident, role = row.get("id"), row.get("role")
        require(isinstance(ident, str) and re.fullmatch(r"[A-Za-z0-9_.-]{1,100}", ident), "Invalid file id")
        require(ident not in files and role in ROLES, f"Duplicate id or unknown role: {ident}")
        size = row.get("bytes")
        require(type(size) is int and 0 <= size <= MAX_FILE, f"Invalid byte bound: {ident}")
        require(isinstance(row.get("sha256"), str) and HASH.fullmatch(row["sha256"]), f"Invalid SHA256: {ident}")
        total += size
        require(total <= MAX_TOTAL, "Cohort exceeds total read bound")
        path = locate(root, row.get("path"))
        require(path.is_file() and (inside(path, root) or role in EXTERNAL), f"File escapes workspace: {ident}")
        require(key(path) not in paths, f"Duplicate/case-insensitive file path: {ident}")
        files[ident] = {**row, "resolved": path}
        paths[key(path)] = ident
        roles.setdefault(role, []).append(ident)

    def one(role):
        require(len(roles.get(role, [])) == 1, f"Exactly one {role} required")
        return files[roles[role][0]]

    def ref(ident, role):
        require(ident in files and files[ident]["role"] == role, f"Invalid {role} file reference: {ident}")
        return files[ident]

    def document(row):
        return load_json(row["resolved"])

    for role in ("game", "adapter", "d3d8to9", "launchDriver", "dependencyLock", "generatedDependencies",
                 "apiHeader", "adapterBuild", "toolchain"):
        one(role)
    require(roles.get("config"), "At least one exact configuration required")
    binary_roles = {"game": "x86", "adapter": "x86", "d3d8to9": "x86"}
    if backend == "remix":
        binary_roles.update(client="x86", server="x64", renderer="x64")
        one("runtimeProvenance")
        require(roles.get("patchManifest") and roles.get("patch"), "Remix patch provenance required")
    else:
        require(not any(roles.get(role) for role in ("client", "server", "renderer", "runtimeProvenance")),
                "System cohort must not silently include a Remix runtime")
    for role, machine in binary_roles.items():
        require(one(role).get("machine") == machine, f"Explicit {machine} required for {role}")

    build = data.get("build", {})
    require(isinstance(build, dict), "Build object required")
    source_root = locate(root, build.get("sourceRoot"))
    require(inside(source_root, root) and source_root.is_dir(), "Source snapshot must be inside workspace")
    entry, exports = ref(build.get("entryFile"), "source"), ref(build.get("exportsFile"), "source")
    require(inside(entry["resolved"], source_root) and inside(exports["resolved"], source_root), "Build inputs escape snapshot")
    require(ref(build.get("manifestFile"), "adapterBuild") is one("adapterBuild"), "Adapter build mismatch")
    require(ref(build.get("toolchainFile"), "toolchain") is one("toolchain"), "Toolchain evidence mismatch")
    chain = document(one("toolchain"))
    require(chain.get("schema") == 1 and chain.get("target") == "x86" and chain.get("recordedAtBuild") is True,
            "Fresh recorded x86 toolchain evidence required; historical unknown is not qualified")
    environment = chain.get("environment", {})
    require(isinstance(environment, dict), "Toolchain environment object required")
    for field in ("VCToolsVersion", "VCToolsInstallDir", "WindowsSDKVersion", "WindowsSdkDir", "UniversalCRTSdkDir"):
        require(isinstance(environment.get(field), str) and environment[field].strip(), f"Missing toolchain {field}")
    allowed_sdk = [Path(environment[field]).resolve(strict=True) for field in
                   ("VCToolsInstallDir", "WindowsSdkDir", "UniversalCRTSdkDir")]
    for field, role in (("compilerFile", "compiler"), ("linkerFile", "linker"), ("environmentFile", "environment"),
                        ("commandFile", "buildCommand"), ("includeLogFile", "includeLog")):
        ref(chain.get(field), role)
    for row in files.values():
        path, role = row["resolved"], row["role"]
        if not inside(path, root):
            if role == "sdkHeader":
                require(any(inside(path, folder) for folder in allowed_sdk), f"Header outside recorded SDK: {path}")
            else:
                expected = {"compiler": "cl.exe", "linker": "link.exe", "environment": "vcvarsall.bat"}[role]
                require(path.name.casefold() == expected, f"Unexpected external tool: {path}")
                require(row is ref(chain[{"compiler": "compilerFile", "linker": "linkerFile", "environment": "environmentFile"}[role]], role),
                        "Unreferenced external tool")
        require(hash_file(path, row["bytes"]) == row["sha256"].lower(), f"SHA256 mismatch: {row['id']}")
        if row.get("machine"):
            pe_machine(path, row["machine"])

    lock, generated = document(one("dependencyLock")), document(one("generatedDependencies"))
    pin = lock["remix"]["revision"]
    require(re.fullmatch(r"[0-9a-f]{40}", pin), "Invalid Remix revision")
    require(generated.get("remixRevision") == pin and chain.get("remixRevision") == pin, "Remix pin mismatch")
    require(generated.get("lockSha256") == one("dependencyLock")["sha256"].lower(), "Generated dependency lock mismatch")
    require(generated.get("generatedLightLfSha256") == lock["generatedLightLfSha256"] and
            generated.get("xxhashSha256") == lock["xxhash"]["sha256"], "Generated dependency pin mismatch")
    header = read_limited(one("apiHeader")["resolved"]).replace(b"\r\n", b"\n")
    require(hashlib.sha256(header).hexdigest() == lock["remix"]["inputsLfSha256"]["public/include/remix/remix_c.h"], "SDK header pin mismatch")
    version = []
    for part in ("MAJOR", "MINOR", "PATCH"):
        match = re.search(rb"^#define REMIXAPI_VERSION_" + part.encode() + rb"\s+(\d+)\s*$", header, re.MULTILINE)
        require(match is not None, f"Missing API version {part}")
        version.append(int(match[1]))
    require(isinstance(data.get("apiVersion"), list) and
            all(type(value) is int for value in data["apiVersion"]) and data["apiVersion"] == version,
            "API version mismatch")
    generated_paths = {files[i]["resolved"].name: files[i] for i in roles.get("generatedHeader", [])}
    require(len(generated_paths) == len(roles.get("generatedHeader", [])), "Ambiguous generated header names")
    for name, expected, lf in (("xxhash.h", lock["xxhash"]["sha256"], False),
                               ("remix_light_conversion.h", lock["generatedLightLfSha256"], True)):
        require(name in generated_paths, f"Missing generated header: {name}")
        body = read_limited(generated_paths[name]["resolved"])
        require(hashlib.sha256(body.replace(b"\r\n", b"\n") if lf else body).hexdigest() == expected,
                f"Generated header mismatch: {name}")

    built = document(one("adapterBuild"))
    require(built.get("platform") == "x86" and built.get("exportsFromSourceSnapshot") is True, "Build must use frozen x86 exports")
    require(locate(root, built.get("sourceDirectory")) == source_root and
            locate(root, built.get("exportsSourcePath")) == exports["resolved"], "Build source paths mismatch")
    for field, row in (("sourceSha256", entry), ("exportsSha256", exports), ("dllSha256", one("adapter"))):
        require(str(built.get(field, "")).lower() == row["sha256"].lower(), f"Adapter build {field} mismatch")
    require(chain.get("adapterSha256") == one("adapter")["sha256"].lower() and
            chain.get("sourceSha256") == entry["sha256"].lower(), "Toolchain receipt belongs to another build")

    include_roots = [locate(root, p) for p in build.get("includeRoots", [])]
    require(include_roots and all(p.is_dir() and inside(p, root) for p in include_roots), "Invalid include roots")
    pending, seen = [entry["resolved"]], set()
    while pending:
        path = pending.pop()
        if key(path) in seen:
            continue
        seen.add(key(path))
        require(key(path) in paths, f"Source omitted from inventory: {path}")
        for name in INCLUDE.findall(read_limited(path).decode("utf-8-sig")):
            choices = [path.parent / name] + [folder / name for folder in include_roots]
            found = next((p.resolve() for p in choices if p.is_file()), None)
            require(found is not None and key(found) in paths, f"Quoted include omitted/unresolved: {name} from {path}")
            require(files[paths[key(found)]]["role"] in ("source", "generatedHeader", "apiHeader", "sdkHeader"), "Include has wrong inventory role")
            pending.append(found)
    log = ref(chain["includeLogFile"], "includeLog")["resolved"]
    log_text = read_limited(log).decode("utf-8-sig")
    prefix = chain.get("includeLogPrefix", SHOW_INCLUDE)
    require(prefix in INCLUDE_PREFIXES, "Unsupported compiler include prefix")
    if "includeLogRawFile" in chain:
        raw = ref(chain["includeLogRawFile"], "includeLogRaw")["resolved"]
        encoding = chain.get("includeLogRawEncoding")
        require(encoding in ("utf-16", "utf-16-le", "utf-8-sig", "cp866"), "Explicit supported raw log encoding required")
        require(read_limited(raw).decode(encoding) == log_text, "Include log transcoding changed raw text")
    includes = 0
    for line in log_text.splitlines():
        if line.lstrip().startswith(prefix):
            path = locate(root, line.lstrip()[len(prefix):].strip())
            require(key(path) in paths and files[paths[key(path)]]["role"] in
                    ("source", "generatedHeader", "apiHeader", "sdkHeader"), f"Compiler include omitted: {path}")
            includes += 1
    require(includes > 0, "Declared /showIncludes evidence required")

    if backend == "remix":
        provenance = document(one("runtimeProvenance"))
        require(provenance.get("remixRevision") == pin, "Runtime provenance revision mismatch")
        require(isinstance(provenance.get("binaries"), dict), "Runtime binaries object required")
        for role in ("client", "server", "renderer"):
            require(provenance.get("binaries", {}).get(role) == one(role)["sha256"].lower(), f"Runtime provenance {role} mismatch")
        require(set(provenance.get("patchManifestFiles", [])) == set(roles["patchManifest"]), "Patch inventory mismatch")
        for ident in provenance["patchManifestFiles"]:
            patch_manifest = document(ref(ident, "patchManifest"))
            require(patch_manifest.get("pin", patch_manifest.get("baseRevision")) == pin, "Patch base mismatch")
            if patch_manifest.get("patchSha256"):
                require(any(files[p]["sha256"].lower() == patch_manifest["patchSha256"].lower()
                            for p in roles["patch"]), "Patch manifest byte hash omitted")
        require(set(provenance.get("patchFiles", [])) == set(roles["patch"]), "Patch bytes inventory mismatch")

    return {"schema": 1, "status": "COHORT_BYTES_AND_CONTRACT_MATCH", "name": data["name"], "backend": backend,
            "manifestSha256": hashlib.sha256(read_limited(manifest)).hexdigest(), "verifiedFiles": len(files),
            "verifiedBytes": total, "quotedSourceFiles": len(seen), "recordedCompilerIncludes": includes,
            "remixRevision": pin, "apiVersion": version,
            "qualification": "Sequential read-only input validation; not an atomic snapshot, executed compatibility, compiler execution proof or complete SDK build closure. Revalidate immediately before launch."}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[4])
    args = parser.parse_args(argv)
    try:
        result = validate(args.manifest, args.root)
    except (OSError, ValueError, KeyError, TypeError, AttributeError) as error:
        print(json.dumps({"schema": 1, "status": "REJECTED", "reason": str(error)}, ensure_ascii=False))
        return 1
    print(json.dumps(result, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
