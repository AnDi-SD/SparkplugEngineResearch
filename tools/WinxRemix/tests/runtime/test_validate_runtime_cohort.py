"""Own finite cohort fixtures; no compiler, DLL loading, game or network."""
import copy
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import struct
import tempfile
import unittest
from types import SimpleNamespace

SCRIPT = Path(__file__).resolve().parents[2] / "scripts/runtime/validate_runtime_cohort.py"
SPEC = importlib.util.spec_from_file_location("validate_runtime_cohort", SCRIPT)
cohort = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(cohort)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def pe(machine="x86"):
    data = bytearray(512)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 60, 64)
    data[64:68] = b"PE\0\0"
    struct.pack_into("<H", data, 68, cohort.MACHINES[machine][0])
    struct.pack_into("<H", data, 84, 224 if machine == "x86" else 240)
    struct.pack_into("<H", data, 88, cohort.MACHINES[machine][1])
    return bytes(data)


class RuntimeCohortTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.data = {"schema": 1, "name": "own-finite-fixture", "backend": "system",
                     "apiVersion": [0, 6, 4], "files": []}
        self.path = self.root / "cohort.json"
        self.source = self.root / "snapshot/tools/WinxRemix"
        self.source.mkdir(parents=True)
        self.add("game", "game", "game.exe", pe(), machine="x86")
        self.add("adapter", "adapter", "d3d9.dll", pe(), machine="x86")
        self.add("translator", "d3d8to9", "d3d8to9.dll", pe(), machine="x86")
        self.add("driver", "launchDriver", "driver.ps1", b"# own fixture, not executed\n")
        self.add("config", "config", "rtx.conf", b"fixture = True\n")
        self.add("entry", "source", "snapshot/tools/WinxRemix/probe.cpp", b'#include "local.h"\n')
        self.add("local", "source", "snapshot/tools/WinxRemix/local.h", b"// own header\n")
        self.add("exports", "source", "snapshot/tools/WinxRemix/probe.def", b"EXPORTS\n")
        api = b"#define REMIXAPI_VERSION_MAJOR 0\n#define REMIXAPI_VERSION_MINOR 6\n#define REMIXAPI_VERSION_PATCH 4\n"
        self.add("api", "apiHeader", "deps/remix_c.h", api)
        xxhash, light = b"// own xxhash stand-in\n", b"// own converter stand-in\n"
        self.add("xxhash", "generatedHeader", "deps/third-party/xxhash.h", xxhash)
        self.add("light", "generatedHeader", "deps/third-party/remix_light_conversion.h", light)
        pin = "a" * 40
        self.lock = {"schema": 1, "remix": {"revision": pin, "inputsLfSha256": {
            "public/include/remix/remix_c.h": digest(api)}}, "xxhash": {"sha256": digest(xxhash)},
            "generatedLightLfSha256": digest(light)}
        self.add_json("lock", "dependencyLock", "dependencies.json", self.lock)
        self.add_json("deps", "generatedDependencies", "deps/dependencies.json", {
            "remixRevision": pin, "lockSha256": self.row("lock")["sha256"],
            "generatedLightLfSha256": digest(light), "xxhashSha256": digest(xxhash)})
        self.add("cl", "compiler", "tools/cl.exe", pe("x64"), machine="x64")
        self.add("link", "linker", "tools/link.exe", pe("x64"), machine="x64")
        self.add("env", "environment", "tools/vcvarsall.bat", b"@rem fixture only\n")
        self.add("commands", "buildCommand", "commands.cmd", b"@rem not executed\n")
        self.add("includes", "includeLog", "includes.txt", ("Note: including file: " +
                 str(self.root / "snapshot/tools/WinxRemix/local.h") + "\n").encode())
        self.chain = {"schema": 1, "target": "x86", "recordedAtBuild": True,
                      "adapterSha256": self.row("adapter")["sha256"], "sourceSha256": self.row("entry")["sha256"],
                      "remixRevision": pin, "compilerFile": "cl", "linkerFile": "link", "environmentFile": "env",
                      "commandFile": "commands", "includeLogFile": "includes", "environment": {
                          "VCToolsVersion": "fixture", "VCToolsInstallDir": str(self.root / "tools"),
                          "WindowsSDKVersion": "fixture", "WindowsSdkDir": str(self.root / "deps"),
                          "UniversalCRTSdkDir": str(self.root / "deps")}}
        self.add_json("chain", "toolchain", "toolchain.json", self.chain)
        self.built = {"schema": 1, "platform": "x86", "sourceDirectory": str(self.source),
                      "sourceSha256": self.row("entry")["sha256"], "exportsSourcePath": str(self.source / "probe.def"),
                      "exportsFromSourceSnapshot": True, "exportsSha256": self.row("exports")["sha256"],
                      "dllSha256": self.row("adapter")["sha256"]}
        self.add_json("built", "adapterBuild", "build-manifest.json", self.built)
        self.data["build"] = {"sourceRoot": str(self.source), "entryFile": "entry", "exportsFile": "exports",
                              "manifestFile": "built", "toolchainFile": "chain", "includeRoots": [str(self.source), str(self.root / "deps")]}

    def row(self, ident):
        return next(x for x in self.data["files"] if x["id"] == ident)

    def add(self, ident, role, relative, body, **fields):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(body)
        self.data["files"] = [x for x in self.data["files"] if x["id"] != ident]
        self.data["files"].append(dict(id=ident, role=role, path=relative, sha256=digest(body), bytes=len(body), **fields))

    def add_json(self, ident, role, relative, value):
        self.add(ident, role, relative, (json.dumps(value) + "\n").encode())

    def run_validation(self):
        self.path.write_text(json.dumps(self.data), encoding="utf-8")
        return cohort.validate(self.path, self.root)

    def reject(self, text):
        with self.assertRaisesRegex(cohort.CohortError, text):
            self.run_validation()

    def test_finite_system_cohort(self):
        result = self.run_validation()
        self.assertEqual(result["status"], "COHORT_BYTES_AND_CONTRACT_MATCH")
        self.assertEqual(result["quotedSourceFiles"], 2)
        self.assertEqual(result["apiVersion"], [0, 6, 4])

    def test_immutable_inputs_and_no_outputs(self):
        before = {p: p.read_bytes() for p in self.root.rglob("*") if p.is_file()}
        self.run_validation()
        self.assertEqual(before, {p: p.read_bytes() for p in before})
        self.assertEqual(set(before) | {self.path}, {p for p in self.root.rglob("*") if p.is_file()})

    def test_mutated_binary(self):
        (self.root / "d3d9.dll").write_bytes(b"x" * 512)
        self.reject("SHA256 mismatch")

    def test_missing_role(self):
        self.data["files"] = [r for r in self.data["files"] if r["id"] != "driver"]
        self.reject("launchDriver required")

    def test_duplicate_and_case_alias(self):
        self.data["files"].append(copy.deepcopy(self.row("adapter")))
        self.reject("Duplicate id")
        self.data["files"][-1]["id"] = "alias"
        self.reject("Duplicate/case-insensitive")

    def test_bad_schema_and_boolean_size(self):
        self.data["schema"] = True
        self.reject("schema")
        self.data["schema"] = 1
        self.row("adapter")["bytes"] = True
        self.reject("byte bound")

    def test_hash_malformed(self):
        self.row("adapter")["sha256"] = "bad"
        self.reject("Invalid SHA256")

    def test_budget_before_big_file_read(self):
        self.row("adapter")["bytes"] = cohort.MAX_FILE + 1
        self.reject("byte bound")

    def test_relative_escape(self):
        self.row("adapter")["path"] = "../outside.dll"
        self.reject("escapes")

    def test_pe_wrong_machine(self):
        self.add("adapter", "adapter", "d3d9.dll", pe("x64"), machine="x86")
        self.reject("PE architecture mismatch")

    def test_pe_missing_signature_and_header_bound(self):
        body = bytearray(pe())
        body[:2] = b"NO"
        self.add("adapter", "adapter", "d3d9.dll", bytes(body), machine="x86")
        self.reject("DOS header")
        body = bytearray(pe())
        struct.pack_into("<I", body, 60, 0xffffffff)
        self.add("adapter", "adapter", "d3d9.dll", bytes(body), machine="x86")
        self.reject("PE header offset")
        body = bytearray(pe())
        struct.pack_into("<H", body, 84, 0xffff)
        self.add("adapter", "adapter", "d3d9.dll", bytes(body), machine="x86")
        self.reject("Truncated PE optional header")

    def test_unknown_historical_toolchain(self):
        self.chain["recordedAtBuild"] = False
        self.add_json("chain", "toolchain", "toolchain.json", self.chain)
        self.reject("historical unknown")

    def test_build_output_and_snapshot_exports(self):
        self.built["dllSha256"] = "0" * 64
        self.add_json("built", "adapterBuild", "build-manifest.json", self.built)
        self.reject("dllSha256 mismatch")
        self.built["exportsFromSourceSnapshot"] = False
        self.add_json("built", "adapterBuild", "build-manifest.json", self.built)
        self.reject("frozen x86 exports")

    def test_uninventoried_literal_source(self):
        self.data["files"] = [r for r in self.data["files"] if r["id"] != "local"]
        self.reject("Quoted include omitted")

    def test_uninventoried_compiler_include(self):
        self.add("includes", "includeLog", "includes.txt", ("Note: including file: " + str(self.root / "config.ini") + "\n").encode())
        (self.root / "config.ini").write_text("x", encoding="ascii")
        self.reject("Compiler include omitted")

    def test_localized_or_missing_include_evidence(self):
        self.add("includes", "includeLog", "includes.txt", b"No English includes\n")
        self.reject("showIncludes evidence")

    def test_declared_russian_prefix_and_exact_utf16_transcoding(self):
        prefix = "Примечание: включение файла:"
        text = prefix + " " + str(self.source / "local.h") + "\r\n"
        self.add("rawlog", "includeLogRaw", "build.log", text.encode("utf-16"))
        self.add("includes", "includeLog", "includes.txt", text.encode("utf-8"))
        self.chain.update(includeLogPrefix=prefix, includeLogRawFile="rawlog", includeLogRawEncoding="utf-16")
        self.add_json("chain", "toolchain", "toolchain.json", self.chain)
        self.assertEqual(self.run_validation()["recordedCompilerIncludes"], 1)
        self.add("includes", "includeLog", "includes.txt", text.replace(prefix, cohort.SHOW_INCLUDE).encode("utf-8"))
        self.reject("transcoding changed raw text")

    def test_arbitrary_include_prefix_is_not_compiler_evidence(self):
        self.chain["includeLogPrefix"] = "arbitrary:"
        self.add_json("chain", "toolchain", "toolchain.json", self.chain)
        self.reject("Unsupported compiler include prefix")

    def test_exact_cp866_raw_transcode_and_tamper(self):
        prefix = "Примечание: включение файла:"
        text = prefix + " " + str(self.source / "local.h") + "\r\n"
        self.add("rawlog", "includeLogRaw", "build.log", text.encode("cp866"))
        self.add("includes", "includeLog", "includes.txt", text.encode("utf-8-sig"))
        self.chain.update(includeLogPrefix=prefix, includeLogRawFile="rawlog", includeLogRawEncoding="cp866")
        self.add_json("chain", "toolchain", "toolchain.json", self.chain)
        self.assertEqual(self.run_validation()["recordedCompilerIncludes"], 1)
        self.add("includes", "includeLog", "includes.txt", (text + "changed\n").encode("utf-8"))
        self.reject("transcoding changed raw text")

    def test_dependency_pin_and_api_version(self):
        self.chain["remixRevision"] = "b" * 40
        self.add_json("chain", "toolchain", "toolchain.json", self.chain)
        self.reject("Remix pin mismatch")
        self.chain["remixRevision"] = "a" * 40
        self.add_json("chain", "toolchain", "toolchain.json", self.chain)
        self.data["apiVersion"] = [0, 6, 5]
        self.reject("API version mismatch")

    def test_generated_dependency_lock(self):
        self.add_json("deps", "generatedDependencies", "deps/dependencies.json", {"remixRevision": "a" * 40, "lockSha256": "0" * 64})
        self.reject("Generated dependency lock mismatch")

    def test_system_does_not_silently_become_remix(self):
        self.add("client", "client", "client.dll", pe(), machine="x86")
        self.reject("silently include")

    def add_remix(self):
        self.data["backend"] = "remix"
        for role, arch in (("client", "x86"), ("server", "x64"), ("renderer", "x64")):
            self.add(role, role, role + ".dll", pe(arch), machine=arch)
        self.add("patch", "patch", "patch.diff", b"own fixture patch\n")
        self.add_json("patchManifest", "patchManifest", "patch.json", {"pin": "a" * 40})
        self.add_json("provenance", "runtimeProvenance", "runtime.json", {"remixRevision": "a" * 40,
            "binaries": {role: self.row(role)["sha256"] for role in ("client", "server", "renderer")},
            "patchManifestFiles": ["patchManifest"], "patchFiles": ["patch"]})

    def test_remix_requires_complete_triple_and_patch_provenance(self):
        self.data["backend"] = "remix"
        self.reject("runtimeProvenance required")
        self.add_remix()
        self.assertEqual(self.run_validation()["backend"], "remix")
        self.add_json("patchManifest", "patchManifest", "patch.json", {"pin": "b" * 40})
        self.reject("Patch base mismatch")

    def test_patch_expected_bytes_not_just_common_revision(self):
        self.add_remix()
        self.add_json("patchManifest", "patchManifest", "patch.json", {
            "pin": "a" * 40, "patchSha256": "0" * 64})
        self.reject("Patch manifest byte hash omitted")

    def test_external_header_restricted_to_recorded_sdk_roots(self):
        with tempfile.TemporaryDirectory() as folder:
            external = Path(folder).resolve() / "outside.h"
            external.write_bytes(b"own external fixture\n")
            self.data["files"].append(dict(id="outside", role="sdkHeader", path=str(external),
                bytes=external.stat().st_size, sha256=digest(external.read_bytes())))
            self.reject("Header outside recorded SDK")

    def test_duplicate_json_keys_rejected(self):
        self.path.write_text('{"schema":1,"schema":1}', encoding="utf-8")
        with self.assertRaisesRegex(cohort.CohortError, "Duplicate JSON"):
            cohort.validate(self.path, self.root)

    def test_nonobject_json_and_nested_build_rejected(self):
        self.path.write_text("[]", encoding="utf-8")
        with self.assertRaisesRegex(cohort.CohortError, "JSON object required"):
            cohort.validate(self.path, self.root)
        self.data["build"] = []
        self.reject("Build object required")

    def test_growth_cannot_bypass_text_read_bound(self):
        class GrowingInput:
            def stat(self):
                return SimpleNamespace(st_size=0)

            def open(self, _mode):
                return io.BytesIO(b"x" * (cohort.MAX_JSON + 2))

        with self.assertRaisesRegex(cohort.CohortError, "grew beyond bound"):
            cohort.read_limited(GrowingInput())


if __name__ == "__main__":
    unittest.main()
