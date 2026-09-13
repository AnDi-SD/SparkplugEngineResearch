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
parser.add_argument("--key", choices=["escape", "enter", "up", "down", "left", "right", "space", "f1", "f8"])
parser.add_argument("--trace", action="store_true", help="Press the adapter's F8 trace key immediately before the selected input")
parser.add_argument("--hold", type=float, default=0.15)
parser.add_argument("--key-count", type=int, default=1, help="Repeat the selected key up to 37 times")
parser.add_argument("--key-gap", type=float, default=0.08, help="Release interval between repeated keys")
parser.add_argument("--menu-step", action="store_true", help="Release each arrow when the verified F1 level selection changes")
parser.add_argument("--settle", type=float, default=0,
                    help="Allow up to 15 seconds for rendering/live config to settle before capture")
parser.add_argument("--capture", type=Path)
parser.add_argument("--capture-seconds", type=float, default=0,
                    help="Capture the selected game window twice per second for up to 45 seconds")
parser.add_argument("--client-size", nargs=2, type=int, metavar=("WIDTH", "HEIGHT"),
                    help="Resize only the selected game window for presentation checks")
parser.add_argument("--close", action="store_true")
parser.add_argument("--click", nargs=2, type=int, metavar=("X", "Y"),
                    help="Click inside the verified game client area")
parser.add_argument("--wheel", type=int, default=0, help="Mouse wheel steps, bounded to -12..12")
args = parser.parse_args()
if args.close and args.capture:
    parser.error("capture and close must be separate calls so the screenshot is actually saved")
if args.menu_step and (args.key not in ("up", "down") or args.hold > .5):
    parser.error("menu-step requires up/down with a maximum hold of 0.5 seconds")
if not -12 <= args.wheel <= 12:
    parser.error("wheel must be between -12 and 12")
if not 0.001 <= args.hold <= 3:
    parser.error("hold must be between 0.001 and 3 seconds")
if not 1 <= args.key_count <= 37 or not 0.02 <= args.key_gap <= 2 or (args.hold + args.key_gap) * args.key_count > 20:
    parser.error("key-count/gap must form a bounded sequence of at most 20 seconds")
if not 0 <= args.settle <= 15:
    parser.error("settle must be between 0 and 15 seconds")
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
    game = Path(__file__).resolve().parents[1] / "local-data/Winx Club"
    if Path(path.value).resolve() not in (game / "WinxClub.exe", game / "WinxClubDebug.exe"):
        raise RuntimeError(f"Refusing to control a different executable: {path.value}")
finally:
    k.CloseHandle(handle)

u.GetWindowThreadProcessId.argtypes = [w.HWND, c.POINTER(w.DWORD)]
u.IsWindowVisible.argtypes = [w.HWND]
u.GetWindowRect.argtypes = [w.HWND, c.POINTER(w.RECT)]
u.GetClientRect.argtypes = [w.HWND, c.POINTER(w.RECT)]
u.SetForegroundWindow.argtypes = [w.HWND]
u.GetForegroundWindow.restype = w.HWND
u.ShowWindow.argtypes = [w.HWND, c.c_int]
u.ShowWindowAsync.argtypes = [w.HWND, c.c_int]
u.IsIconic.argtypes = [w.HWND]
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
    # A synchronous restore can block on the game's fullscreen window thread.
    # Leave an already visible, non-minimized window alone.
    if u.IsIconic(hwnd):
        u.ShowWindowAsync(hwnd, 9)
    u.SetForegroundWindow(hwnd)
    time.sleep(0.3)
    if u.GetForegroundWindow() != hwnd:
        # A normal title-bar click can activate the verified window when Windows
        # denies programmatic focus. Do not attach input queues or click through
        # an overlapping application. Fullscreen windows have no such fallback.
        class TitleBarInfo(c.Structure):
            _fields_ = [('size', w.DWORD), ('rect', w.RECT), ('states', w.DWORD * 6)]
        u.GetTitleBarInfo.argtypes = [w.HWND, c.POINTER(TitleBarInfo)]
        u.GetWindowLongW.argtypes = [w.HWND, c.c_int]
        u.SetWindowPos.argtypes = [w.HWND, w.HWND, c.c_int, c.c_int, c.c_int, c.c_int, w.UINT]
        u.WindowFromPoint.argtypes = [w.POINT]
        u.WindowFromPoint.restype = w.HWND
        title = TitleBarInfo()
        title.size = c.sizeof(title)
        has_caption = u.GetWindowLongW(hwnd, -16) & 0x00c00000 == 0x00c00000
        if has_caption and u.GetTitleBarInfo(hwnd, c.byref(title)) and not title.states[0] & 0x18000:
            rect = title.rect
            was_topmost = bool(u.GetWindowLongW(hwnd, -20) & 8)
            if rect.right - rect.left >= 320 and rect.bottom > rect.top:
                try:
                    # Restore the original topmost flag even if validation fails.
                    if not u.SetWindowPos(hwnd, -1, 0, 0, 0, 0, 0x13):
                        raise c.WinError(c.get_last_error())
                    x, y = (2 * rect.left + rect.right) // 3, (rect.top + rect.bottom) // 2
                    hit = u.WindowFromPoint(w.POINT(x, y))
                    hit_pid = w.DWORD()
                    u.GetWindowThreadProcessId(hit, c.byref(hit_pid))
                    if hit == hwnd and hit_pid.value == args.pid:
                        u.SetCursorPos(x, y)
                        u.mouse_event(2, 0, 0, 0, 0)
                        try:
                            time.sleep(0.08)
                        finally:
                            u.mouse_event(4, 0, 0, 0, 0)
                finally:
                    if not was_topmost:
                        u.SetWindowPos(hwnd, -2, 0, 0, 0, 0, 0x13)
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
    if args.click or args.wheel:
        client = w.RECT()
        if not u.GetClientRect(hwnd, c.byref(client)):
            raise c.WinError(c.get_last_error())
        x, y = args.click if args.click else (client.right//2, client.bottom//2)
        if not 0 <= x < client.right or not 0 <= y < client.bottom:
            raise ValueError("Click must be within the selected game client")
        point = w.POINT(x, y)
        u.ClientToScreen.argtypes = [w.HWND, c.POINTER(w.POINT)]
        if not u.ClientToScreen(hwnd, c.byref(point)):
            raise c.WinError(c.get_last_error())
        if u.GetForegroundWindow() != hwnd:
            raise RuntimeError("Winx lost foreground before mouse input")
        u.SetCursorPos(point.x, point.y)
        time.sleep(0.1)
        if args.click:
            u.mouse_event(2, 0, 0, 0, 0)
            try:
                time.sleep(0.08)
            finally:
                u.mouse_event(4, 0, 0, 0, 0)
        if args.wheel:
            u.mouse_event(0x0800, 0, 0, args.wheel*120, 0)
        time.sleep(0.3)
    if args.trace:
        if u.GetForegroundWindow() != hwnd:
            raise RuntimeError("Winx lost foreground before trace input")
        scan = u.MapVirtualKeyW(119, 0)
        u.keybd_event(119, scan, 0, 0)
        try:
            time.sleep(0.08)
        finally:
            u.keybd_event(119, scan, 2, 0)
    if args.key:
        vk = {"escape":27, "enter":13, "up":38, "down":40, "left":37, "right":39, "space":32, "f1":112, "f8":119}[args.key]
        scan = u.MapVirtualKeyW(vk, 0)
        flags = 1 if args.key in ("up", "down", "left", "right") else 0
        for index in range(args.key_count):
            if u.GetForegroundWindow() != hwnd:
                raise RuntimeError("Winx lost foreground; stopping keyboard input")
            if args.menu_step:
                # Poll only the documented selection word in a verified live
                # debug menu. No process writes; input remains ordinary keys.
                from winx_remix_state import StateReader
                reader = StateReader(args.pid)
                try:
                    state = reader.snapshot()
                    if not state.get('debugOpen') or len(state['menus']) != 2 or state['menus'][-1]['count'] != 37:
                        raise RuntimeError('Menu-step requires the verified F1 level selector')
                    menu = state['menus'][-1]
                    deadline = time.monotonic() + args.hold
                    u.keybd_event(vk, scan, flags, 0)
                    try:
                        while time.monotonic() < deadline:
                            if u.GetForegroundWindow() != hwnd:
                                raise RuntimeError('Winx lost foreground during menu-step')
                            if reader.u32(menu['address'] + 0x20) != menu['selected']:
                                break
                            time.sleep(.001)
                    finally:
                        u.keybd_event(vk, scan, flags | 2, 0)
                finally:
                    reader.close()
            else:
                u.keybd_event(vk, scan, flags, 0)
                try:
                    time.sleep(args.hold)
                finally:
                    u.keybd_event(vk, scan, flags | 2, 0)
            if index + 1 < args.key_count:
                time.sleep(args.key_gap)
        time.sleep(0.8)
    if args.capture:
        time.sleep(args.settle)
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
