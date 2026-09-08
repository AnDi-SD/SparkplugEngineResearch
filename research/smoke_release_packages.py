#!/usr/bin/env python3
"""Start local Windows packages on a private desktop with bounded owned processes.

Copies packages before launch so application logs cannot alter release contents.
Requires the installed .NET 8 Desktop Runtime; never accepts installer dialogs.
This checks startup and a responsive main window, not interactive workflows.
"""
from __future__ import annotations

import argparse
import ctypes as C
from ctypes import wintypes as W
import json
import os
from pathlib import Path
import shutil
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
K = C.WinDLL("kernel32", use_last_error=True)
U = C.WinDLL("user32", use_last_error=True)
SIZE = C.c_size_t


class Startup(C.Structure):
    _fields_ = [("cb", W.DWORD), ("reserved", W.LPWSTR), ("desktop", W.LPWSTR), ("title", W.LPWSTR),
                ("x", W.DWORD), ("y", W.DWORD), ("xSize", W.DWORD), ("ySize", W.DWORD),
                ("xChars", W.DWORD), ("yChars", W.DWORD), ("fill", W.DWORD), ("flags", W.DWORD),
                ("show", W.WORD), ("reservedSize", W.WORD), ("reserved2", C.c_void_p),
                ("stdin", W.HANDLE), ("stdout", W.HANDLE), ("stderr", W.HANDLE)]


class ProcessInfo(C.Structure):
    _fields_ = [("process", W.HANDLE), ("thread", W.HANDLE), ("pid", W.DWORD), ("tid", W.DWORD)]


class BasicLimit(C.Structure):
    _fields_ = [("processTime", C.c_int64), ("jobTime", C.c_int64), ("flags", W.DWORD),
                ("minWorkingSet", SIZE), ("maxWorkingSet", SIZE), ("activeProcesses", W.DWORD),
                ("affinity", SIZE), ("priority", W.DWORD), ("scheduling", W.DWORD)]


class ExtendedLimit(C.Structure):
    _fields_ = [("basic", BasicLimit), ("io", C.c_uint64 * 6), ("processMemory", SIZE),
                ("jobMemory", SIZE), ("peakProcessMemory", SIZE), ("peakJobMemory", SIZE)]


CALLBACK = C.WINFUNCTYPE(W.BOOL, W.HWND, W.LPARAM)


def api(library, name, result, *args):
    fn = getattr(library, name)
    fn.restype, fn.argtypes = result, args
    return fn


create_desktop = api(U, "CreateDesktopW", W.HANDLE, W.LPCWSTR, W.LPCWSTR, C.c_void_p, W.DWORD, W.DWORD, C.c_void_p)
close_desktop = api(U, "CloseDesktop", W.BOOL, W.HANDLE)
enum_windows = api(U, "EnumDesktopWindows", W.BOOL, W.HANDLE, CALLBACK, W.LPARAM)
window_pid = api(U, "GetWindowThreadProcessId", W.DWORD, W.HWND, C.POINTER(W.DWORD))
window_text = api(U, "GetWindowTextW", C.c_int, W.HWND, W.LPWSTR, C.c_int)
window_class = api(U, "GetClassNameW", C.c_int, W.HWND, W.LPWSTR, C.c_int)
send_timeout = api(U, "SendMessageTimeoutW", SIZE, W.HWND, W.UINT, W.WPARAM, W.LPARAM, W.UINT, W.UINT, C.POINTER(SIZE))
post_message = api(U, "PostMessageW", W.BOOL, W.HWND, W.UINT, W.WPARAM, W.LPARAM)
create_job = api(K, "CreateJobObjectW", W.HANDLE, C.c_void_p, W.LPCWSTR)
set_job = api(K, "SetInformationJobObject", W.BOOL, W.HANDLE, C.c_int, C.c_void_p, W.DWORD)
query_job = api(K, "QueryInformationJobObject", W.BOOL, W.HANDLE, C.c_int, C.c_void_p, W.DWORD, C.c_void_p)
assign_job = api(K, "AssignProcessToJobObject", W.BOOL, W.HANDLE, W.HANDLE)
terminate_job = api(K, "TerminateJobObject", W.BOOL, W.HANDLE, W.UINT)
create_process = api(K, "CreateProcessW", W.BOOL, W.LPCWSTR, W.LPWSTR, C.c_void_p, C.c_void_p, W.BOOL, W.DWORD, C.c_void_p, W.LPCWSTR, C.POINTER(Startup), C.POINTER(ProcessInfo))
resume = api(K, "ResumeThread", W.DWORD, W.HANDLE)
terminate_process = api(K, "TerminateProcess", W.BOOL, W.HANDLE, W.UINT)
exit_code = api(K, "GetExitCodeProcess", W.BOOL, W.HANDLE, C.POINTER(W.DWORD))
open_process = api(K, "OpenProcess", W.HANDLE, W.DWORD, W.BOOL, W.DWORD)
image_name = api(K, "QueryFullProcessImageNameW", W.BOOL, W.HANDLE, W.DWORD, W.LPWSTR, C.POINTER(W.DWORD))
close_handle = api(K, "CloseHandle", W.BOOL, W.HANDLE)


def check(ok):
    if not ok:
        raise C.WinError(C.get_last_error())
    return ok


def processes(job):
    buffer = C.create_string_buffer(4096)
    check(query_job(job, 3, buffer, len(buffer), None))
    count = C.cast(buffer, C.POINTER(W.DWORD))[1]
    if count > 16:
        raise RuntimeError("Unexpected process count")
    ids = (SIZE * count).from_buffer(buffer, 8)
    rows = []
    for pid in ids:
        handle = open_process(0x1000, False, pid)
        if handle:
            try:
                path, length = C.create_unicode_buffer(32768), W.DWORD(32768)
                if image_name(handle, 0, path, C.byref(length)):
                    rows.append({"pid": pid, "path": path.value})
            finally:
                close_handle(handle)
    return rows


def windows(desktop):
    rows = []

    @CALLBACK
    def collect(hwnd, _):
        title, kind, pid = C.create_unicode_buffer(2048), C.create_unicode_buffer(256), W.DWORD()
        window_text(hwnd, title, len(title))
        window_class(hwnd, kind, len(kind))
        window_pid(hwnd, C.byref(pid))
        if title.value:
            answer = SIZE()
            responsive = bool(send_timeout(hwnd, 0, 0, 0, 2, 500, C.byref(answer)))
            rows.append({"hwnd": int(hwnd), "pid": pid.value, "title": title.value,
                         "class": kind.value, "responsive": responsive})
        return True

    C.set_last_error(0)
    enumerated = enum_windows(desktop, collect, 0)
    if not enumerated and C.get_last_error():
        raise C.WinError(C.get_last_error())
    return rows


def smoke(executable: Path, payload: Path, title: str, state: Path, bootstrap: bool):
    state.mkdir(parents=True, exist_ok=False)
    desktop_name = "SparkplugSmoke_" + uuid.uuid4().hex
    desktop = check(create_desktop(desktop_name, None, None, 0, 0x01FF, None))
    job, info = None, ProcessInfo()
    started = time.perf_counter()
    result = {"entryPoint": str(executable), "payload": str(payload), "bootstrap": bootstrap, "expectedTitle": title}
    try:
        job = check(create_job(None, None))
        limits = ExtendedLimit()
        limits.basic.flags = 0x2000 | 0x200 | 0x8  # kill on close, aggregate commit, process count
        limits.basic.activeProcesses = 4
        limits.jobMemory = 512 * 1024 * 1024
        check(set_job(job, 9, C.byref(limits), C.sizeof(limits)))
        startup = Startup(cb=C.sizeof(Startup), desktop=desktop_name, flags=1, show=0)
        env = dict(os.environ)
        for key in ("TEMP", "TMP", "LOCALAPPDATA", "APPDATA", "DOTNET_BUNDLE_EXTRACT_BASE_DIR"):
            directory = state / key.lower()
            directory.mkdir()
            env[key] = str(directory)
        env["DOTNET_CLI_TELEMETRY_OPTOUT"] = "1"
        environment = C.create_unicode_buffer("\0".join(k + "=" + v for k, v in sorted(env.items(), key=lambda pair: pair[0].upper())) + "\0\0")
        command = C.create_unicode_buffer('"' + str(executable) + '"')
        check(create_process(str(executable), command, None, None, False, 0x4 | 0x400,
                             environment, str(executable.parent), C.byref(startup), C.byref(info)))
        check(assign_job(job, info.process))
        if resume(info.thread) == 0xFFFFFFFF:
            raise C.WinError(C.get_last_error())
        main = None
        while time.perf_counter() - started < 25:
            time.sleep(0.2)
            rows = processes(job)
            candidates = {p["pid"] for p in rows if Path(p["path"]).resolve() == payload}
            observed = windows(desktop)
            main = next((w for w in observed if w["pid"] in candidates and w["title"] == title and w["responsive"]), None)
            if main and time.perf_counter() - started >= 3:
                break
            if not rows:
                break
        status = W.DWORD()
        check(exit_code(info.process, C.byref(status)))
        result.update({"processes": processes(job), "windows": windows(desktop),
                       "launcherExitCodeAtProbe": status.value, "mainWindowReady": main is not None})
        if not main or (bootstrap and status.value != 0):
            result["status"] = "failed"
        else:
            result["status"] = "passed"
            post_message(main["hwnd"], 0x10, 0, 0)  # close only this owned main window
            stop = time.perf_counter() + 3
            while processes(job) and time.perf_counter() < stop:
                time.sleep(0.1)
        check(query_job(job, 9, C.byref(limits), C.sizeof(limits), None))
        result["peakJobCommitBytes"] = limits.peakJobMemory
        result["forcedCleanupRequired"] = bool(processes(job))
    except Exception as exc:
        result.update({"status": "failed", "error": str(exc)})
    finally:
        if info.process:
            status = W.DWORD()
            if exit_code(info.process, C.byref(status)) and status.value == 259:
                terminate_process(info.process, 1)
        if job:
            terminate_job(job, 1)
            close_handle(job)
        for handle in (info.thread, info.process):
            if handle:
                close_handle(handle)
        close_desktop(desktop)
    result["elapsedSeconds"] = time.perf_counter() - started
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("packages", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    runtime = Path(os.environ["ProgramFiles"]) / "dotnet/shared/Microsoft.WindowsDesktop.App"
    if not any(p.name.startswith("8.") for p in runtime.iterdir()):
        raise RuntimeError("Installed .NET 8 Desktop Runtime required; no installer is run by this probe")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    titles = {"SmoViewer": "SMO Viewer", "SmoLVLcreator": "SmoLVLcreator", "SmoExporter": "Sparkplug SMO Exporter",
              "SmoImporter": "Sparkplug SMO Importer", "WinxHairPatcher": "Winx Club \u2014 Bloom Hair Patcher", "SMOTextureTool": "SMO Texture Tool"}
    manifest = json.loads((ROOT / "release/release-manifest.json").read_text(encoding="utf-8-sig"))
    results = []
    for product in manifest["products"]:
        candidates = [p for p in args.packages.resolve().iterdir() if p.is_dir() and p.name.startswith(product["id"] + "-")]
        if len(candidates) != 1:
            raise RuntimeError(f"Expected one package for {product['id']}")
        package = output / candidates[0].name
        shutil.copytree(candidates[0], package)
        cases = [(product, package / product["executable"], package / "app" / product["executable"], True)]
        cases += [(tool, package / "tools" / tool["id"] / tool["executable"], package / "tools" / tool["id"] / tool["executable"], False) for tool in product.get("tools", [])]
        for app, entry, payload, bootstrap in cases:
            label = product["id"] + ("-root" if bootstrap else "-" + app["id"])
            result = smoke(entry, payload, titles[app["id"]], output / "state" / label, bootstrap)
            result["case"] = label
            results.append(result)
            print(json.dumps(result, ensure_ascii=True), flush=True)
            (output / "report.json").write_text(json.dumps({"status": "passed" if all(r["status"] == "passed" for r in results) else "failed", "cases": results,
                "limits": {"secondsPerLaunch": 25, "closeSeconds": 3, "jobCommitBytes": 512 * 1024 * 1024, "activeProcesses": 4},
                "limitations": ["Private desktop startup/WM_NULL only; not rendering or user-operation coverage.", "Existing .NET runtime required; installer path is not exercised.", "KnownFolder settings may be read; no settings commands or game assets are supplied."]}, indent=2) + "\n", encoding="utf-8")
    return int(any(r["status"] != "passed" for r in results))


if __name__ == "__main__":
    raise SystemExit(main())
