"""Thin ctypes ownership/array adapter to the shared reconstructed C++ core.

No SAN decoding, interpolation, quaternion or hierarchy mathematics lives here.
The DLL beside this module is the deployable location; repository builds are
also accepted for development. No alternate animation implementation exists.
"""
from __future__ import annotations
import ctypes as C
from dataclasses import dataclass
from pathlib import Path
import threading
import weakref


class NativeError(ValueError):
    pass


class Node(C.Structure):
    _fields_ = [('parent', C.c_int32), ('position', C.c_float*3),
                ('rotation', C.c_float*4), ('scale', C.c_float*3), ('billboard', C.c_uint32)]


class Pose(C.Structure):
    _fields_ = [('position', C.c_float*3), ('rotation', C.c_float*4),
                ('scale', C.c_float*3), ('valid_roles', C.c_uint32)]


class TrackInfo(C.Structure):
    _fields_ = [('position_keys', C.c_uint32), ('rotation_keys', C.c_uint32), ('scale_keys', C.c_uint32)]


class ChannelInfo(C.Structure):
    _fields_ = [('source_keys', C.c_uint32), ('axes', C.c_uint32),
                ('unique_times', C.c_uint32), ('representations', C.c_uint32*3)]


class AxisInfo(C.Structure):
    _fields_ = [('representation', C.c_uint32), ('keys', C.c_uint32),
                ('stride', C.c_uint32), ('values', C.c_uint32)]


_library = None
_load_lock = threading.Lock()


def library():
    global _library
    with _load_lock:
        if _library is not None:
            return _library
        if C.sizeof(C.c_void_p) != 8 or not hasattr(C, 'WinDLL'):
            raise NativeError('Общему ядру нужен 64-битный Python на Windows.')
        folder = Path(__file__).resolve().parent
        candidates = [folder/'SparkplugViewerNative.dll',
                      folder.parents[1]/'artifacts/native/viewer/Release/SparkplugViewerNative.dll']
        path = next((p for p in candidates if p.is_file()), None)
        if path is None:
            raise NativeError('Нет SparkplugViewerNative.dll. Соберите общее native-ядро или положите DLL рядом с конвертером.')
        lib = C.CDLL(str(path))
        u, f, h = C.c_uint32, C.c_float, C.c_void_p
        signatures = {
            'spv_abi_version': (u, []), 'spv_last_error': (C.c_char_p, []),
            'spv_clip_load': (h, [C.POINTER(C.c_uint8), u]), 'spv_clip_destroy': (None, [h]),
            'spv_clip_info': (C.c_int, [h, C.POINTER(f), C.POINTER(u)]),
            'spv_clip_tag_count': (C.c_int, [h, C.POINTER(u)]),
            'spv_clip_track': (C.c_int, [h, u, C.POINTER(C.c_char), u, C.POINTER(TrackInfo)]),
            'spv_clip_sample': (C.c_int, [h, u, f, C.POINTER(Pose)]),
            'spv_clip_channel': (C.c_int, [h, u, u, C.POINTER(ChannelInfo)]),
            'spv_clip_times': (C.c_int, [h, u, u, C.POINTER(f), u]),
            'spv_clip_axis_info': (C.c_int, [h, u, u, u, C.POINTER(AxisInfo)]),
            'spv_clip_axis_values': (C.c_int, [h, u, u, u, C.POINTER(f), u]),
            'spv_scene_create': (h, [C.POINTER(Node), u]), 'spv_scene_destroy': (None, [h]),
            'spv_scene_bind': (C.c_int, [h, h, C.POINTER(C.c_int32), u]),
            'spv_scene_pose': (C.c_int, [h, f, C.POINTER(Pose), u]),
        }
        try:
            for name, (result, arguments) in signatures.items():
                fn = getattr(lib, name);fn.restype = result;fn.argtypes = arguments
        except AttributeError as error:
            raise NativeError('DLL устарела: нужна сборка с общим SAN API конвертера.') from error
        if lib.spv_abi_version() != 2:
            raise NativeError('Неподдерживаемая версия ABI SparkplugViewerNative.dll.')
        if (C.sizeof(Node), C.sizeof(Pose), C.sizeof(ChannelInfo), C.sizeof(AxisInfo)) != (48,44,24,16):
            raise NativeError('Неверный размер структуры native-интерфейса.')
        _library = lib
        return lib


def check(result):
    if not result:
        raise NativeError(library().spv_last_error().decode('utf-8', 'replace'))
    return result


class Owned:
    def _initialize(self, handle, destroy):
        self._lock = threading.RLock()
        self._handle = check(handle)
        self._finalizer = weakref.finalize(self, destroy, handle)

    def _get(self):
        if not self._handle:
            raise NativeError('Объект общего ядра уже закрыт.')
        return self._handle

    def close(self):
        with self._lock:
            self._handle = None
            self._finalizer()

    def __enter__(self):
        self._get();return self

    def __exit__(self, *args):
        self.close()


@dataclass(frozen=True)
class Track:
    ordinal: int
    name: str
    channels: dict


class Channel:
    def __init__(self, owner, ordinal, role):
        self.owner, self.ordinal, self.role = owner, ordinal, role
        lib = library(); info = ChannelInfo()
        check(lib.spv_clip_channel(owner._get(), ordinal, role, C.byref(info)))
        self.source_keys, self.axes = info.source_keys, info.axes
        self.representations = tuple(info.representations)
        values = (C.c_float*info.unique_times)()
        check(lib.spv_clip_times(owner._get(), ordinal, role, values, len(values)))
        self.times = tuple(values)

    def sample(self, time):
        pose = self.owner.sample(self.ordinal, time)
        return (tuple(pose.position), tuple(pose.rotation), tuple(pose.scale))[self.role] if pose.valid_roles & (1 << self.role) else None

    def prepared_axes(self):
        """Actual engine arrays, exposed for target-format representability checks."""
        with self.owner._lock:
            handle = self.owner._get();lib = library();result = []
            for axis in range(3):
                info = AxisInfo();check(lib.spv_clip_axis_info(handle, self.ordinal, self.role, axis, C.byref(info)))
                values = (C.c_float*info.values)()
                check(lib.spv_clip_axis_values(handle, self.ordinal, self.role, axis, values, len(values)))
                result.append((info, tuple(values)))
            return result


class Animation(Owned):
    def __init__(self, data):
        if not 0 < len(data) <= 64*1024*1024:
            raise NativeError('SAN превышает допустимый размер.')
        lib = library();buffer = (C.c_uint8*len(data)).from_buffer_copy(data)
        self._initialize(lib.spv_clip_load(buffer, len(buffer)), lib.spv_clip_destroy)
        try:
            duration, count, tags = C.c_float(), C.c_uint32(), C.c_uint32()
            check(lib.spv_clip_info(self._get(), C.byref(duration), C.byref(count)))
            check(lib.spv_clip_tag_count(self._get(), C.byref(tags)))
            self.duration, self.tags = duration.value, tags.value
            self.tracks = []
            for ordinal in range(count.value):
                name, info = C.create_string_buffer(65536), TrackInfo()
                check(lib.spv_clip_track(self._get(), ordinal, name, len(name), C.byref(info)))
                channels = {role: Channel(self, ordinal, role) for role in range(3)}
                self.tracks.append(Track(ordinal, name.value.decode('latin-1'), channels))
        except BaseException:
            self.close();raise

    def sample(self, ordinal, time):
        with self._lock:
            pose = Pose();check(library().spv_clip_sample(self._get(), ordinal, time, C.byref(pose)))
            return pose


class Scene(Owned):
    def __init__(self, bones, order):
        self.names = tuple(order)
        indices = {name: i for i, name in enumerate(order)}
        if len(indices) != len(order):
            raise NativeError('Повторный узел в порядке скелета.')
        nodes = (Node*len(order))()
        for i, name in enumerate(order):
            bone = bones[name]
            if bone.parent is not None and bone.parent not in indices:
                raise NativeError('В выбранном скелете отсутствует родитель.')
            nodes[i] = Node(indices.get(bone.parent, -1), (C.c_float*3)(*bone.position),
                            (C.c_float*4)(*bone.rotation), (C.c_float*3)(*bone.scale), getattr(bone, 'billboard', 0))
        lib = library();self._initialize(lib.spv_scene_create(nodes, len(nodes)), lib.spv_scene_destroy)
        self.bound = None
        self._poses = (Pose*len(order))()

    def bind(self, animation, roles):
        with self._lock, animation._lock:
            indices = (C.c_int32*len(roles))(*roles)
            check(library().spv_scene_bind(self._get(), animation._get(), indices, len(indices)))
            self.bound = animation

    def sample(self, time):
        with self._lock:
            check(library().spv_scene_pose(self._get(), time, self._poses, len(self._poses)))
            return [(tuple(v.position), tuple(v.rotation), tuple(v.scale)) for v in self._poses]
