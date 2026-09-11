"""Read-only observations of the existing local Winx debug build for GPU tests.

Controller offsets follow NativeDebuggerHarness.PollNativeSceneReady. Debug
menu offsets are verified against original 5CBD89/5CBDC0/5CC6A0/5CCF70.
This is our test instrumentation, not reconstructed game logic.
"""
import argparse
import ctypes as c
from ctypes import wintypes as w
import hashlib
import json
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]
DEBUG_SHA256 = 'c27ea9db4228781a12a90ae808807d4af1397a7e40dd8f5fff28f3c87cc62cdb'


class StateReader:
    def __init__(self, pid):
        self.pid = pid
        self.k = c.WinDLL('kernel32', use_last_error=True)
        self.k.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]
        self.k.OpenProcess.restype = w.HANDLE
        self.k.ReadProcessMemory.argtypes = [w.HANDLE, c.c_void_p, c.c_void_p, c.c_size_t, c.POINTER(c.c_size_t)]
        self.k.QueryFullProcessImageNameW.argtypes = [w.HANDLE, w.DWORD, w.LPWSTR, c.POINTER(w.DWORD)]
        self.k.CloseHandle.argtypes = [w.HANDLE]
        self.handle = self.k.OpenProcess(0x1010, False, pid)  # query + VM_READ only
        if not self.handle:
            raise c.WinError(c.get_last_error())
        try:
            path, length = c.create_unicode_buffer(32768), w.DWORD(32768)
            if not self.k.QueryFullProcessImageNameW(self.handle, 0, path, c.byref(length)):
                raise c.WinError(c.get_last_error())
            expected = ROOT / 'local-data/Winx Club/WinxClubDebug.exe'
            if Path(path.value).resolve() != expected or hashlib.sha256(expected.read_bytes()).hexdigest() != DEBUG_SHA256:
                raise RuntimeError('Read-only test requires the documented local debug executable')
            if self.read(0x400000, 2) != b'MZ':
                raise RuntimeError('Unexpected image base')
        except Exception:
            self.close()
            raise

    def close(self):
        if self.handle:
            self.k.CloseHandle(self.handle)
            self.handle = None

    def read(self, address, size):
        if not 0x10000 <= address < 0x7fff0000 or not 0 < size <= 4096:
            raise ValueError('Invalid bounded read')
        buffer, got = c.create_string_buffer(size), c.c_size_t()
        if not self.k.ReadProcessMemory(self.handle, address, buffer, size, c.byref(got)) or got.value != size:
            raise c.WinError(c.get_last_error())
        return buffer.raw

    def u32(self, address):
        return struct.unpack('<I', self.read(address, 4))[0]

    def camera_snapshot(self):
        # Observed PC layouts documented in native-class-sp-camera.md and
        # native-pc-visibility-runtime.md. Diagnostic reads, no game mutation.
        roots = {}
        candidates = set()
        for label, global_address, size in (
                ('engine', 0x755274, 0x54),
                ('cameraManager', 0x75db98, 0x40),
                ('visibilityManager', 0x75e1b0, 0x54)):
            ptr = self.u32(global_address)
            words = list(struct.unpack('<' + 'I'*(size//4), self.read(ptr, size))) if ptr else []
            roots[label] = dict(address=ptr, words=words)
            candidates.update(words)
        # Find camera references in the managers' short pointer containers.
        # Bounds only cover the first 64 entries; reject every non-camera by
        # its exact known original vtable before interpreting any camera fields.
        for pointer in list(candidates):
            try:
                words = struct.unpack('<64I', self.read(pointer, 256))
                candidates.update(words)
            except (OSError, ValueError):
                pass
        cameras = []
        for pointer in sorted(candidates):
            try:
                if self.u32(pointer) not in (0x6ef1e0, 0x6dea20, 0x6dcbc0):
                    continue
                raw = self.read(pointer, 0x238)
                def floats(offset, count):
                    return list(struct.unpack_from('<'+'f'*count, raw, offset))
                cameras.append(dict(address=pointer, vtable=struct.unpack_from('<I',raw)[0],
                    worldPosition=floats(0x74,3), view=floats(0xcc,16), projection=floats(0x10c,16),
                    forward=floats(0x1a0,3), near= floats(0xbc,1)[0], far=floats(0xc0,1)[0],
                    viewAngle=floats(0x188,1)[0], pixelAspect=floats(0x190,1)[0],
                    frustum=[floats(0x1c4+i*16,4) for i in range(6)],
                    viewport=list(struct.unpack_from('<6I',raw,0x170)),
                    viewportActive=raw[0x230], alternateProjection=raw[0x231],
                    viewportHeightOverWidth=floats(0x234,1)[0]))
            except (OSError, ValueError):
                pass
        return dict(roots=roots, cameras=cameras)

    def snapshot(self):
        flow = self.u32(0x755294)
        result = dict(pid=self.pid, controller=flow)
        if flow:
            index = struct.unpack('<i', self.read(flow + 0x1ac, 4))[0]
            if not -1 <= index <= 20:
                raise RuntimeError('Invalid state stack')
            active = self.u32(flow + 0x15c + index * 4) if index >= 0 else 0
            result.update(current=self.u32(flow + 0x1b0), pending=self.u32(flow + 0x1b8),
                          stack=index, active=self.u32(active + 0x10) if active else 0)
            result['stateStack'] = []
            for slot in range(index + 1):
                item = self.u32(flow + 0x15c + slot * 4)
                result['stateStack'].append(self.u32(item + 0x10) if item else 0)
            if any(result[k] > 255 for k in ('current', 'pending', 'active')):
                raise RuntimeError('Implausible state IDs')
        debug = self.u32(0x765bf8)
        result['menus'] = []
        if debug:
            result['debugOpen'] = bool(self.read(debug + 0x3c, 1)[0])
            begin, end = self.u32(debug + 0x20), self.u32(debug + 0x24)
            if end < begin or (end - begin) % 4 or end - begin > 64:
                raise RuntimeError('Invalid debug stack')
            for address in range(begin, end, 4):
                menu = self.u32(address)
                selected, entries, limit = self.u32(menu + 0x20), self.u32(menu + 0x28), self.u32(menu + 0x2c)
                count = (limit - entries) // 4
                if limit < entries or (limit - entries) % 4 or not 0 < count <= 64 or selected >= count:
                    raise RuntimeError('Invalid debug menu entries')
                result['menus'].append(dict(address=menu, selected=selected, count=count, entries=entries))
        return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pid', type=int, required=True)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--cameras', action='store_true')
    args = parser.parse_args()
    reader = StateReader(args.pid)
    try:
        result = reader.snapshot()
        if args.cameras:
            result['cameraAudit'] = reader.camera_snapshot()
        text = json.dumps(result, indent=2)
        if args.output:
            args.output.write_text(text + '\n', encoding='utf-8')
        print(text)
    finally:
        reader.close()
