"""Control and capture only the explicitly selected local Winx Club process."""
import argparse
import ctypes as c
from ctypes import wintypes as w
import json
from pathlib import Path
import time

from PIL import ImageGrab

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--pid", required=True, type=int)
parser.add_argument("--key", choices=["escape", "enter", "up", "down", "left", "right", "space", "f8"])
parser.add_argument("--hold", type=float, default=0.15)
parser.add_argument("--capture", type=Path)
parser.add_argument("--capture-seconds", type=float, default=0,
                    help="Capture the selected game window twice per second for up to 45 seconds")
parser.add_argument("--client-size", nargs=2, type=int, metavar=("WIDTH", "HEIGHT"),
                    help="Resize only the selected game window for presentation checks")
parser.add_argument("--close", action="store_true")
args = parser.parse_args()
if not 0.01 <= args.hold <= 3:
    parser.error("hold must be between 0.01 and 3 seconds")
if not 0 <= args.capture_seconds <= 45 or (args.capture_seconds and not args.capture):
    parser.error("capture-seconds requires capture and must be between 0 and 45")
if args.client_size and any(not 320 <= v <= 3840 for v in args.client_size):
    parser.error("client-size dimensions must be between 320 and 3840")

u, k = c.WinDLL("user32", use_last_error=True), c.WinDLL("kernel32", use_last_error=True)
u.SetProcessDPIAware()
k.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]
k.OpenProcess.restype = w.HANDLE
k.QueryFullProcessImageNameW.argtypes = [w.HANDLE, w.DWORD, w.LPWSTR, c.POINTER(w.DWORD)]
k.CloseHandle.argtypes = [w.HANDLE]
handle = k.OpenProcess(0x1000, False, args.pid)
if not handle:
    raise c.WinError(c.get_last_error())
try:
    path, size = c.create_unicode_buffer(32768), w.DWORD(32768)
    if not k.QueryFullProcessImageNameW(handle, 0, path, c.byref(size)):
        raise c.WinError(c.get_last_error())
    expected = Path(__file__).resolve().parents[1] / "local-data/Winx Club/WinxClub.exe"
    if Path(path.value).resolve() != expected:
        raise RuntimeError(f"Refusing to control a different executable: {path.value}")
finally:
    k.CloseHandle(handle)

u.GetWindowThreadProcessId.argtypes = [w.HWND, c.POINTER(w.DWORD)]
u.IsWindowVisible.argtypes = [w.HWND]
u.GetWindowRect.argtypes = [w.HWND, c.POINTER(w.RECT)]
u.SetForegroundWindow.argtypes = [w.HWND]
u.GetForegroundWindow.restype = w.HWND
u.ShowWindow.argtypes = [w.HWND, c.c_int]
u.PostMessageW.argtypes = [w.HWND, w.UINT, w.WPARAM, w.LPARAM]
windows = []
callback_type = c.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM)
@callback_type
def visit(hwnd, param):
    pid = w.DWORD()
    u.GetWindowThreadProcessId(hwnd, c.byref(pid))
    if pid.value == args.pid and u.IsWindowVisible(hwnd):
        rect = w.RECT()
        if u.GetWindowRect(hwnd, c.byref(rect)):
            windows.append(((rect.right-rect.left)*(rect.bottom-rect.top), hwnd))
    return True
u.EnumWindows.argtypes = [callback_type, w.LPARAM]
u.EnumWindows(visit, 0)
if not windows:
    raise RuntimeError("No visible window for the selected Winx process")
_, hwnd = max(windows)
u.GetLastActivePopup.argtypes = [w.HWND]
u.GetLastActivePopup.restype = w.HWND
popup = u.GetLastActivePopup(hwnd)
popup_pid = w.DWORD()
u.GetWindowThreadProcessId(popup, c.byref(popup_pid))
if popup_pid.value == args.pid and u.IsWindowVisible(popup):
    hwnd = popup
if args.close:
    for _, owned_window in windows:
        u.PostMessageW(owned_window, 0x10, 0, 0)
else:
    u.ShowWindow(hwnd, 9)
    u.SetForegroundWindow(hwnd)
    time.sleep(0.3)
    if u.GetForegroundWindow() != hwnd:
        raise RuntimeError("Winx did not gain foreground; refusing keyboard input/capture")
    if args.client_size:
        u.GetClientRect.argtypes = [w.HWND, c.POINTER(w.RECT)]
        u.SetWindowPos.argtypes = [w.HWND, w.HWND, c.c_int, c.c_int, c.c_int, c.c_int, w.UINT]
        outer, client = w.RECT(), w.RECT()
        if not u.GetWindowRect(hwnd, c.byref(outer)) or not u.GetClientRect(hwnd, c.byref(client)):
            raise c.WinError(c.get_last_error())
        width = args.client_size[0] + outer.right - outer.left - client.right
        height = args.client_size[1] + outer.bottom - outer.top - client.bottom
        if not u.SetWindowPos(hwnd, None, 0, 0, width, height, 0x16):
            raise c.WinError(c.get_last_error())
        time.sleep(0.8)
    if args.key:
        vk = {"escape":27, "enter":13, "up":38, "down":40, "left":37, "right":39, "space":32, "f8":119}[args.key]
        scan = u.MapVirtualKeyW(vk, 0)
        flags = 1 if args.key in ("up", "down", "left", "right") else 0
        u.keybd_event(vk, scan, flags, 0)
        try:
            time.sleep(args.hold)
        finally:
            u.keybd_event(vk, scan, flags | 2, 0)
        time.sleep(0.8)
    if args.capture:
        args.capture.parent.mkdir(parents=True, exist_ok=True)
        started, index = time.monotonic(), 0
        while True:
            if u.GetForegroundWindow() != hwnd:
                raise RuntimeError("Winx lost foreground; stopping window capture")
            rect = w.RECT()
            if not u.GetWindowRect(hwnd, c.byref(rect)):
                raise c.WinError(c.get_last_error())
            target = args.capture if index == 0 else args.capture.with_stem(f"{args.capture.stem}-{index:03d}")
            ImageGrab.grab(bbox=(rect.left, rect.top, rect.right, rect.bottom), all_screens=True).save(target)
            index += 1
            if time.monotonic() - started >= args.capture_seconds:
                break
            time.sleep(0.5)
print(json.dumps({"pid": args.pid, "hwnd": hwnd, "key": args.key, "capture": str(args.capture) if args.capture else None, "close": args.close}))
