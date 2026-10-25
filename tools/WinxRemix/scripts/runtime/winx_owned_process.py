"""Shared Windows ABI wrappers for bounded owned-process fixtures.

The caller must prove process/job ownership before mutation or termination.
No game, desktop or child process is created by importing this module.
"""
import ctypes as C
from ctypes import wintypes as W

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


class MemoryStatus(C.Structure):
    _fields_ = [("length", W.DWORD), ("load", W.DWORD)] + [
        (name, C.c_uint64) for name in ("totalPhysical", "availablePhysical", "totalCommit",
                                      "availableCommit", "totalVirtual", "availableVirtual", "unused")]


global_memory = api(K, "GlobalMemoryStatusEx", W.BOOL, C.POINTER(MemoryStatus))


def memory_status():
    value = MemoryStatus(length=C.sizeof(MemoryStatus))
    check(global_memory(C.byref(value)))
    return {name: int(getattr(value, name)) for name, _ in MemoryStatus._fields_ if name != "length"}
