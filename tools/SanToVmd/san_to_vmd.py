#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Тестовый SAN → VMD: запустите этот файл, положив SAN рядом с ним.

Python 3.10+, только стандартная библиотека. Результаты: папка vmd/.
Профиль: PC-скелет Bloom → обычный PMD-скелет MMD, ноги в режиме FK.
Модель не экспортируется: SMO/PMD нужны только для исходной позы и имён костей.
Подробности, ограничения и примеры запуска находятся в README.md.
"""

from __future__ import annotations

import argparse
from bisect import bisect_right
from dataclasses import dataclass
import json
import math
from pathlib import Path
import struct
import sys


# ------------------------- Понятные настройки -------------------------

SCRIPT_DIR = Path(__file__).resolve().parent
FPS = 30  # VMD хранит номера кадров, а SAN — время в секундах.
MAX_BYTES = 64 * 1024 * 1024
MAX_SECONDS = 600  # Защита от случайного огромного/повреждённого duration.
ZERO = (0.0, 0.0, 0.0)
IDENTITY = (0.0, 0.0, 0.0, 1.0)  # Quaternion везде в порядке X, Y, Z, W.

# Слева — настоящее японское имя PMD/VMD, справа — источник движения.
# Название UpperArm в игре обманчиво: эта кость находится у локтя.
BODY_MAP = {
    "下半身": "Pelvis",
    "上半身": "Spine_03",  # Мировое вращение включает Spine_01 и Spine_02.
    "首": "Neck", "頭": "Head",
    "左肩": "L_Clavicle", "左腕": "L_Bicep",
    "左ひじ": "L_UpperArm", "左手首": "L_Hand",
    "右肩": "R_Clavicle", "右腕": "R_Bicep",
    "右ひじ": "R_UpperArm", "右手首": "R_Hand",
    "左足": "L_Thigh", "左ひざ": "L_calf", "左足首": "L_Ankle",
    "右足": "R_Thigh", "右ひざ": "R_calf", "右足首": "R_Ankle",
}
FINGERS = {"人指": "Index", "中指": "Middle", "薬指": "Ring", "小指": "Pinky"}
LEG_IK = ("左足ＩＫ", "右足ＩＫ", "左つま先ＩＫ", "右つま先ＩＫ")


class ConversionError(ValueError):
    """Ожидаемая ошибка входного файла, которую можно объяснить пользователю."""


# ---------------------- Немного математики ----------------------------
# Мы используем активные вращения и векторы-столбцы:
# world_rotation = parent_rotation * local_rotation.
# Порядок множителей существенен! Перестановка ломает руки и ноги.

def add(a, b):
    return tuple(x + y for x, y in zip(a, b))


def sub(a, b):
    return tuple(x - y for x, y in zip(a, b))


def times(v, factor):
    return tuple(x * factor for x in v)


def length(v):
    return math.sqrt(sum(x * x for x in v))


def normalize(q):
    size = length(q)
    if not math.isfinite(size) or size < 1e-10:
        raise ConversionError("Нулевой или некорректный quaternion.")
    return times(q, 1.0 / size)


def inverse(q):
    # Все используемые quaternion нормализованы, поэтому обратный = сопряжённый.
    x, y, z, w = q
    return (-x, -y, -z, w)


def multiply(a, b):
    x, y, z, w = a
    X, Y, Z, W = b
    return (w*X + x*W + y*Z - z*Y, w*Y - x*Z + y*W + z*X,
            w*Z + x*Y - y*X + z*W, w*W - x*X - y*Y - z*Z)


def rotate(q, v):
    return multiply(multiply(q, (*v, 0.0)), inverse(q))[:3]


def align_directions(original, desired):
    """Кратчайшее вращение между двумя направлениями кости в исходной позе."""
    if min(length(original), length(desired)) < 1e-8:
        raise ConversionError("Нулевая длина кости при выравнивании скелетов.")
    a, b = times(original, 1/length(original)), times(desired, 1/length(desired))

    def cross(u, v):
        return (u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0])

    dot = max(-1.0, min(1.0, sum(x*y for x, y in zip(a, b))))
    if dot < -0.999999:
        # Для противоположных направлений ось выбирается перпендикулярно кости.
        axis = cross(a, (1, 0, 0) if abs(a[0]) < 0.9 else (0, 1, 0))
        return normalize((*axis, 0))
    return normalize((*cross(a, b), 1+dot))


def slerp(a, b, amount):
    # q и -q описывают одну позу. Выбираем короткий путь, иначе будет полный оборот.
    dot = sum(x*y for x, y in zip(a, b))
    if dot < 0:
        b, dot = times(b, -1), -dot
    dot = min(1.0, dot)
    if dot > 0.9995:
        return normalize(add(times(a, 1-amount), times(b, amount)))
    angle = math.acos(dot)
    return normalize(add(times(a, math.sin((1-amount)*angle) / math.sin(angle)),
                         times(b, math.sin(amount*angle) / math.sin(angle))))


# ------------------- Чтение бинарных данных ---------------------------

class Reader:
    """Каждое чтение ограничено границей своей секции; никаких поисков по байтам."""

    def __init__(self, data):
        self.data = memoryview(data)
        self.offset = 0

    def take(self, size):
        if size < 0 or size > len(self.data) - self.offset:
            raise ConversionError(f"Файл оборван около байта {self.offset}.")
        start = self.offset
        self.offset += size
        return self.data[start:self.offset]

    def unpack(self, fmt):
        return struct.unpack("<" + fmt, self.take(struct.calcsize("<" + fmt)))

    def number(self, fmt):
        return self.unpack(fmt)[0]

    def text(self, size, encoding="latin1"):
        return bytes(self.take(size)).split(b"\0", 1)[0].decode(encoding)

    def done(self):
        if self.offset != len(self.data):
            raise ConversionError("Неожиданные данные в конце секции.")


def read_bytes(path):
    if path.stat().st_size > MAX_BYTES:
        raise ConversionError(f"Файл больше {MAX_BYTES // 1024**2} МиБ: {path.name}")
    return path.read_bytes()


def floats(payload, count):
    r = Reader(payload)
    values = r.unpack("f" * count)
    r.done()
    if not all(math.isfinite(v) for v in values):
        raise ConversionError("В данных встретились NaN или бесконечность.")
    return values


@dataclass
class Entry:
    name: str
    kind: int
    data: memoryview


def read_ffps(path):
    """Общая оболочка SMO/SAN. В каталоге хранятся ID, имена и границы объектов."""
    raw = read_bytes(path)
    r = Reader(raw)
    magic, version, _, size, platform, start, data_size, count = r.unpack("4s7I")
    if magic != b"FFPS" or version != 0x26 or not platform & 3:
        raise ConversionError("Поддерживаются только PC FFPS версии 0x26.")
    if size != len(raw) or start + data_size != size or not 36 <= start <= size:
        raise ConversionError("Размеры FFPS не совпадают с размером файла.")
    table = Reader(r.take(start - 32))
    if count > (start - 36) // 18:
        raise ConversionError("Некорректное число объектов FFPS.")
    entries = {}
    for _ in range(count):
        object_id, name_size = table.unpack("IH")
        name = table.text(name_size)
        kind, offset, extent = table.unpack("III")
        if object_id in entries or extent < 8 or offset + extent > data_size:
            raise ConversionError("Некорректная запись каталога FFPS.")
        body = memoryview(raw)[start + offset:start + offset + extent]
        if bytes(body[:8]) != struct.pack("<I4s", kind, b"SBOO"):
            raise ConversionError(f"Неверная сигнатура объекта {name}.")
        entries[object_id] = Entry(name, kind, body[8:])
    if table.number("I") != 0:
        raise ConversionError("Нет нулевого завершения каталога FFPS.")
    table.done()
    return entries


def fields(data):
    """Поля Sparkplug: младшие 5 бит — номер, старшие 3 бита — способ задания длины."""
    r = Reader(data)
    while r.offset < len(r.data):
        header = r.number("B")
        kind, code = header & 31, header >> 5
        if kind == 31:
            kind = r.number("B")
        if code == 0:
            # Для поддержанных concrete spNode/spAnimation здесь заканчивается объект.
            r.done()
            return
        size = (0, 1, 2, 4, 8)[code] if code <= 4 else r.number({5:"B", 6:"H", 7:"I"}[code])
        yield kind, r.take(size)
    raise ConversionError("Нет завершения секции Sparkplug.")


@dataclass
class Bone:
    name: str
    parent: str | None
    position: tuple
    rotation: tuple = IDENTITY
    kind: int = 0
    scale: tuple = (1.0, 1.0, 1.0)


def read_skeleton(path):
    """Из SMO берём concrete spNode и их field 5, а меши/текстуры пропускаем."""
    entries = read_ffps(path)
    nodes = {i: e for i, e in entries.items() if e.kind == 0x695C0F65}
    bones, links = {}, []
    for object_id, entry in nodes.items():
        if entry.name in bones:
            raise ConversionError(f"Неоднозначное имя узла в SMO: {entry.name}")
        bone = Bone(entry.name, None, ZERO)
        seen = set()
        for kind, payload in fields(entry.data):
            if kind not in range(9):
                raise ConversionError(f"Неизвестное поле spNode: {kind}")
            if kind not in (5, 7) and kind in seen:
                raise ConversionError(f"Повторное поле spNode: {entry.name}/{kind}")
            seen.add(kind)
            if kind == 0:
                bone.position = floats(payload, 3)
            elif kind == 1:
                bone.rotation = normalize(floats(payload, 4))
            elif kind == 2:
                # Масштаб проверяется позже, только в используемой ветке тела.
                # У лишней кости/маркера он не должен мешать конвертации.
                bone.scale = floats(payload, 3)
            elif kind == 5:
                child = Reader(payload)
                child_id = child.number("I")
                if len(payload) != 4:
                    child.take(child.number("I"))
                child.done()
                if child_id not in entries:
                    raise ConversionError("Ссылка на отсутствующий объект SMO.")
                if child_id in nodes:
                    links.append((entry.name, nodes[child_id].name))
        bones[bone.name] = bone
    for parent, child in links:
        if bones[child].parent not in (None, parent):
            raise ConversionError(f"У узла {child} несколько родителей.")
        bones[child].parent = parent
    return bones


def read_pmd(path):
    """PMD задаёт мировые позиции суставов в исходной позе, имена и IK-цепочки."""
    r = Reader(read_bytes(path))
    if bytes(r.take(3)) != b"Pmd" or r.number("f") != 1.0:
        raise ConversionError("Целевая модель должна быть PMD 1.0; PMX пока не поддерживается.")
    model_name = r.text(20, "cp932")
    r.take(256)  # Комментарий автора модели.
    r.take(r.number("I") * 38)  # Вершины.
    r.take(r.number("I") * 2)   # Индексы треугольников.
    r.take(r.number("I") * 70)  # Материалы.
    rows = []
    for _ in range(r.number("H")):
        name = r.text(20, "cp932")
        parent, _, kind, _, x, y, z = r.unpack("HHBH3f")
        if not all(math.isfinite(v) for v in (x, y, z)):
            raise ConversionError("Некорректные позиции костей PMD.")
        rows.append((name, parent, kind, (x, y, z)))
    bones = {}
    for name, parent, kind, position in rows:
        if name in bones or (parent != 65535 and parent >= len(rows)):
            raise ConversionError("Повторные имена или неверные родители PMD.")
        bones[name] = Bone(name, None if parent == 65535 else rows[parent][0], position, kind=kind)
    iks = []
    for _ in range(r.number("H")):
        controller, target, count, _, _ = r.unpack("HHBHf")
        chain = r.unpack("H" * count)
        if any(i >= len(rows) for i in (controller, target, *chain)):
            raise ConversionError("Некорректная IK-цепочка PMD.")
        iks.append(rows[controller][0])
    # Дальше идут морфы/физика: они нужны MMD, а конвертер их не изменяет.
    return model_name, bones, iks


# ----------------------- Кривые анимации SAN --------------------------

@dataclass
class Curve:
    representation: int
    times: tuple
    values: tuple

    def sample(self, time, quaternion=False):
        if not self.times:
            return None  # Пустая кривая сохраняет компоненту исходной позы SMO.
        if time <= self.times[0] or len(self.times) == 1:
            return self.values[0][:1] if self.representation >= 3 else self.values[0]
        if time >= self.times[-1]:
            return self.values[-1][:1] if self.representation >= 3 else self.values[-1]
        i = bisect_right(self.times, time) - 1
        u = (time-self.times[i]) / (self.times[i+1]-self.times[i])
        a, b = self.values[i:i+2]
        if self.representation == 4:
            # SAN хранит value, incoming, outgoing и два служебных coefficient.
            # Пересчитываем коэффициенты, как native preparation; последние
            # служебные значения в файле могут быть неинициализированы.
            delta = b[0] - a[0]
            c2 = 3*delta - (2*a[2] + b[1])
            c3 = a[2] + b[1] - 2*delta
            return (a[0] + u*(a[2] + u*(c2 + u*c3)),)
        return slerp(a, b, u) if quaternion else add(times(a, 1-u), times(b, u))


def read_curve(payload, role):
    r = Reader(payload)
    curves, axes = [], 1
    while len(curves) < axes:
        representation = r.number("I")
        if representation == 0:
            if curves:
                raise ConversionError("Неполная тройка скалярных SAN-кривых.")
            r.done()
            return []
        if representation not in (1, 3, 4) or (role == 3 and representation != 1):
            raise ConversionError(f"Не поддерживается SAN representation {representation}, поле {role}.")
        if not curves and representation >= 3:
            axes = 3
        if curves and representation not in (3, 4):
            raise ConversionError("Смешаны векторные и скалярные кривые.")
        count = r.number("I")
        stride = 5 if representation == 4 else 1 if representation == 3 else 4 if role == 3 else 3
        if count > len(payload) // (4 * (stride+1)):
            raise ConversionError("Некорректное число SAN-ключей.")
        key_times = r.unpack("f" * count)
        values = tuple(r.unpack("f" * stride) for _ in range(count))
        if not all(math.isfinite(t) and t >= 0 for t in key_times):
            raise ConversionError("Некорректное время ключа SAN.")
        if any(b <= a for a, b in zip(key_times, key_times[1:])):
            raise ConversionError("SAN-ключи должны идти по возрастанию времени.")
        used_width = 3 if representation == 4 else stride
        if not all(math.isfinite(v) for row in values for v in row[:used_width]):
            raise ConversionError("Некорректное значение SAN-ключа.")
        if role == 3:
            values = tuple(normalize(q) for q in values)
        if role == 4:
            # Проверяем сами ключи, чтобы даже короткое изменение scale между
            # соседними VMD-кадрами не исчезло незаметно при запекании.
            for row in values:
                components = row[:1] if representation >= 3 else row
                if any(abs(v-1) > 0.001 for v in components):
                    raise ConversionError("Анимация масштаба не представима в VMD.")
                if representation == 4 and any(abs(v) > 0.001 for v in row[1:3]):
                    raise ConversionError("Кубическая анимация масштаба не представима в VMD.")
        curves.append(Curve(representation, key_times, values))
    r.done()
    if len(curves) == 3 and any(bool(c.times) != bool(curves[0].times) for c in curves):
        raise ConversionError("Некоторые оси SAN-кривой отсутствуют.")
    return curves


def sample(curves, time, fallback, quaternion=False):
    if not curves or not curves[0].times:
        return fallback
    if len(curves) == 3:
        return tuple(c.sample(time)[0] for c in curves)
    return curves[0].sample(time, quaternion)


@dataclass
class Clip:
    duration: float
    tracks: dict
    tags: int
    ignored: list


def read_san(path, required):
    entries = list(read_ffps(path).values())
    if len(entries) != 1 or entries[0].kind != 0x56EE563A:
        raise ConversionError("Ожидается один spAnimation в SAN.")
    tracks, pending, ignored, names = {}, {}, [], set()
    duration, tags = None, 0
    for kind, payload in fields(entries[0].data):
        if kind == 0:
            if duration is not None:
                raise ConversionError("Повторная длительность SAN.")
            duration = floats(payload, 1)[0]
        elif kind in (2, 3, 4):
            if kind in pending:
                raise ConversionError("Повторный PRS-канал до имени трека.")
            pending[kind] = payload
        elif kind == 1:
            r = Reader(payload)
            name = r.text(r.number("H"))
            r.done()
            if not name or (name in names and name in required):
                raise ConversionError(f"Пустое/повторное имя трека SAN: {name}")
            names.add(name)
            # Лишние кости разрешены: даже их неподдержанные кривые не влияют
            # на тело. Поддержку PRS проверяем только для используемого графа.
            if name in required:
                try:
                    tracks[name] = {role: read_curve(data, role) for role, data in pending.items()}
                except ConversionError as error:
                    raise ConversionError(f"{name}: {error}") from error
            else:
                ignored.append(name)
            pending = {}
        elif kind == 5:
            tags += 1  # Игровые события вроде SND_FOOTSTEP не являются движением костей.
        elif kind in range(6, 13) or kind == 64:
            if len(payload) != 4:
                raise ConversionError("Неверный размер служебного счётчика SAN.")
        else:
            raise ConversionError(f"Неизвестное поле SAN: {kind}")
    if pending or duration is None or not 0 < duration <= MAX_SECONDS:
        raise ConversionError("Нет корректной длительности или завершения SAN-трека.")
    if not any(c.times for track in tracks.values() for role, cs in track.items()
               if role in (2, 3) for c in cs):
        raise ConversionError("В SAN нет движения для выбранного скелета.")
    return Clip(duration, tracks, tags, ignored)


# --------------------- Перенос позы между скелетами --------------------

def hierarchy(bones, selected):
    """Сначала родители, затем дети. Отброшенные родители не должны ломать позу."""
    order, visited, active = [], set(), set()

    def visit(name):
        if name in active:
            raise ConversionError(f"Цикл в скелете около {name}.")
        if name in visited:
            return
        if name not in bones:
            raise ConversionError(f"Нет нужной кости: {name}")
        active.add(name)
        if bones[name].parent is not None:
            visit(bones[name].parent)
        active.remove(name)
        visited.add(name)
        order.append(name)

    for name in selected:
        visit(name)
    return order


def source_world(bones, order, clip=None, time=0):
    result = {}
    for name in order:
        bone = bones[name]
        channels = clip.tracks.get(name, {}) if clip else {}
        position = sample(channels.get(2), time, bone.position)
        rotation = sample(channels.get(3), time, bone.rotation, quaternion=True)
        scale = sample(channels.get(4), time, bone.scale)
        if any(abs(v-1) > 0.001 for v in scale):
            raise ConversionError(f"Масштабирование {name} нельзя записать в VMD.")
        if bone.parent is not None:
            parent_position, parent_rotation = result[bone.parent]
            position = add(parent_position, rotate(parent_rotation, position))
            rotation = normalize(multiply(parent_rotation, rotation))
        result[name] = (position, rotation)
    return result


def bone_mapping(target, body_only):
    mapping = dict(BODY_MAP)
    # В профиле с двумя суставами позвоночника делим исходную цепочку на две.
    if "上半身2" in target:
        mapping["上半身"] = "Spine_02"
        mapping["上半身2"] = "Spine_03"
    if not body_only:
        for side, prefix in (("左", "L"), ("右", "R")):
            for japanese, english in FINGERS.items():
                for number, digit in enumerate("１２３", 1):
                    name = side + japanese + digit
                    if name in target:
                        mapping[name] = f"{prefix}_{english}_{number:02d}"
            # Стандартная Miku имеет две кости большого пальца. Для первой
            # объединяем движение двух исходных суставов через мировую позу.
            has_base = side + "親指０" in target
            for digit, number in (("０", 1), ("１", 2), ("２", 3)):
                name = side + "親指" + digit
                if name in target and (digit != "０" or has_base):
                    mapping[name] = f"{prefix}_Thumb_{number:02d}"
    return mapping


class Retargeter:
    def __init__(self, source, target, iks, body_only=False, motion_scale=None):
        self.source, self.target = source, target
        missing = (set(BODY_MAP) | {"センター"}) - set(target)
        if missing:
            raise ConversionError("В PMD нет обязательных костей: " + ", ".join(sorted(missing)))
        self.mapping = bone_mapping(target, body_only)
        self.target_order = hierarchy(target, ["センター", *self.mapping])
        for name in self.mapping:
            if target[name].kind not in (0, 1, 4):
                raise ConversionError(f"Необычный тип кости PMD: {name}; этот профиль пока не поддерживается.")
        self.order = hierarchy(source, [*self.mapping.values(), "L_Toe", "R_Toe"])
        self.rest = source_world(source, self.order)
        self.alignment = {name: IDENTITY for name in self.mapping}
        for side in ("左", "右"):
            for start, end in (("肩", "腕"), ("腕", "ひじ"), ("ひじ", "手首"),
                               ("足", "ひざ"), ("ひざ", "足首"), ("手首", "中指１")):
                a, b = side+start, side+end
                if a in self.mapping and b in self.mapping:
                    target_direction = sub(target[b].position, target[a].position)
                    source_direction = sub(self.rest[self.mapping[b]][0], self.rest[self.mapping[a]][0])
                    self.alignment[a] = align_directions(target_direction, source_direction)
            # Пальцы сохраняют форму исходной кисти Miku и получают ту же
            # поправку базиса, что кисть; их собственные движения идут из SAN.
            for name in self.mapping:
                if name.startswith(side) and "指" in name:
                    self.alignment[name] = self.alignment[side+"手首"]
        # Y вверх и X в сторону левой руки совпадают у проверенных PC Bloom
        # и Miku PMD. Здесь читается исходный SMO, не отражённый экспортный GLB.
        # Никакого дополнительного отражения Z поэтому не делаем.
        floor = min(self.rest[name][0][1] for name in ("L_Toe", "R_Toe"))
        source_height = self.rest["Pelvis"][0][1] - floor
        target_floor = min(b.position[1] for b in target.values()
                           if b.name in ("左つま先", "右つま先", "左足首", "右足首"))
        target_height = target["下半身"].position[1] - target_floor
        if source_height <= 0 or target_height <= 0:
            raise ConversionError("Не удалось определить высоту таза над стопами.")
        self.scale = motion_scale if motion_scale is not None else target_height/source_height
        if not math.isfinite(self.scale) or self.scale <= 0:
            raise ConversionError("Масштаб перемещения должен быть положительным числом.")
        self.disabled_ik = [name for name in LEG_IK if name in iks]
        unexpected = [name for name in self.target_order
                      if name not in self.mapping and name != "センター"]
        # Дополнительный обычный родитель остаётся в исходной позе. Нет попытки
        # угадать вращение twist/IK/grant-костей по одному похожему имени.
        self.extra_parents = unexpected
        if any(target[name].kind not in (0, 1) for name in unexpected):
            raise ConversionError("Дополнительный родитель PMD использует неподдерживаемый тип кости.")

    def pose(self, clip, time):
        animated = source_world(self.source, self.order, clip, time)
        # Важное отличие от копирования SAN-ключей: убираем исходную ориентацию
        # кости и получаем её ПОЛНОЕ мировое изменение от bind/rest pose.
        # Одной разницы вращений недостаточно: Bloom и Miku держат руки под
        # разным углом в rest pose. Поправка alignment сначала приводит направление
        # конечности Miku к Bloom, а затем применяется движение из SAN.
        # Стопы сохраняют исходную ориентацию целевой модели: форма обуви различается.
        world_changes = {
            target: normalize(multiply(multiply(animated[source][1], inverse(self.rest[source][1])),
                                       self.alignment[target]))
            for target, source in self.mapping.items()
        }
        local, target_world = {}, {}
        for name in self.target_order:
            parent = self.target[name].parent
            parent_rotation = target_world.get(parent, IDENTITY)
            if name in world_changes:
                desired = world_changes[name]
                local[name] = (ZERO, normalize(multiply(inverse(parent_rotation), desired)))
            else:
                # Непереносимый родитель не добавляет своего вращения.
                desired = parent_rotation
            target_world[name] = desired
        # Центр MMD не расположен в тазу. Передаём ему только СМЕЩЕНИЕ таза
        # относительно исходного SMO; остальная поза использует длины PMD.
        # Не вычитаем кадр 0: в нём уже может быть присед, который надо сохранить.
        shift = times(sub(animated["Pelvis"][0], self.rest["Pelvis"][0]), self.scale)
        local["センター"] = (shift, IDENTITY)
        return local


# --------------------------- Запись VMD -------------------------------

def encoded_name(name, size):
    data = name.encode("cp932")
    if len(data) > size:
        raise ConversionError(f"Имя {name} не помещается в {size} байт VMD.")
    return data.ljust(size, b"\0")


def linear_interpolation():
    # Четыре Bezier-кривые: X/Y/Z и вращение. Контрольные точки (20,20),
    # (107,107) лежат на диагонали и задают линейное движение. VMD содержит
    # перекрывающиеся копии этих параметров, поэтому это не просто 64 нуля.
    base = bytes([20]*8 + [107]*8)
    return b"".join(base[i:] + bytes(i) for i in range(4))


def frame_count(duration):
    # Float32(1.2)*30 чуть больше 36: не добавляем лишний кадр из-за округления.
    return math.ceil(duration * FPS - 1e-5) + 1


def write_vmd(path, clip, retargeter, model_name):
    count = frame_count(clip.duration)
    names = ["センター", *retargeter.mapping]
    encoded = {name: encoded_name(name, 15) for name in names}
    model = encoded_name(model_name, 20)
    interpolation = linear_interpolation()
    previous = {}
    # Временный файл позволяет не оставлять наполовину записанный VMD при ошибке.
    # Готовый результат заменяется только после успешного расчёта всех кадров.
    temporary = path.with_suffix(".vmd.tmp")
    try:
        with temporary.open("wb") as stream:
            stream.write(b"Vocaloid Motion Data 0002".ljust(30, b"\0") + model)
            stream.write(struct.pack("<I", count * len(names)))
            for frame in range(count):
                pose = retargeter.pose(clip, min(frame / FPS, clip.duration))
                for name in names:
                    position, rotation = pose[name]
                    if not all(math.isfinite(v) for v in (*position, *rotation)):
                        raise ConversionError("При переносе получилась некорректная поза.")
                    if name in previous and sum(a*b for a, b in zip(previous[name], rotation)) < 0:
                        rotation = times(rotation, -1)
                    previous[name] = rotation
                    stream.write(encoded[name])
                    stream.write(struct.pack("<I3f4f", frame, *position, *rotation))
                    stream.write(interpolation)
            # Морфы, камера, свет, тень отсутствуют; затем один model/IK-кадр.
            stream.write(struct.pack("<5I", 0, 0, 0, 0, 1))
            stream.write(struct.pack("<IBI", 0, 1, len(retargeter.disabled_ik)))
            for name in retargeter.disabled_ik:
                stream.write(encoded_name(name, 20) + b"\0")
        temporary.replace(path)
    finally:
        if temporary.exists():
            temporary.unlink()
    return count, len(names)


# ------------------- Запуск: SAN рядом со скриптом ---------------------

def reference_path(explicit, filename, repository_relative, extension):
    if explicit:
        path = Path(explicit).resolve()
        if not path.is_file():
            raise ConversionError(f"Файл не найден: {path}")
        return path
    preferred = SCRIPT_DIR / filename
    if preferred.is_file():
        return preferred
    adjacent = sorted(p for p in SCRIPT_DIR.iterdir() if p.is_file() and p.suffix.lower() == extension)
    if len(adjacent) == 1:
        return adjacent[0]
    if len(adjacent) > 1:
        raise ConversionError(f"Рядом несколько {extension}: укажите нужный через параметр запуска.")
    # Удобство при работе в репозитории. Перенесённый .py не зависит от этой папки:
    # достаточно положить оба эталонных файла непосредственно рядом с ним.
    repository = SCRIPT_DIR.parent.parent
    fallback = repository / repository_relative
    if fallback.is_file():
        return fallback
    raise ConversionError(f"Положите {filename} рядом со скриптом или укажите путь параметром.")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input-dir", type=Path, default=SCRIPT_DIR, help="Папка SAN; по умолчанию рядом с .py")
    parser.add_argument("--output-dir", type=Path, help="Папка VMD; по умолчанию vmd внутри папки SAN")
    parser.add_argument("--skeleton", help="Эталонный SMO со скелетом Bloom")
    parser.add_argument("--model", help="Целевая модель PMD")
    parser.add_argument("--body-only", action="store_true", help="Не переносить пальцы")
    parser.add_argument("--motion-scale", type=float, help="Масштаб перемещений вместо оценки по высоте таза")
    parser.add_argument("--overwrite", action="store_true", help="Заменить существующие одноимённые VMD")
    args = parser.parse_args(argv)
    directory = args.input_dir.resolve()
    output = args.output_dir.resolve() if args.output_dir else directory / "vmd"
    try:
        if not directory.is_dir():
            raise ConversionError(f"Папка SAN не существует: {directory}")
        paths = sorted(p for p in directory.iterdir() if p.is_file() and p.suffix.lower() == ".san")
        if not paths:
            print(f"SAN-файлы не найдены. Положите их в папку:\n{directory}")
            return 0
        skeleton_path = reference_path(args.skeleton, "bloom_jeans.smo", "local-data/bloom_jeans.smo", ".smo")
        model_path = reference_path(args.model, "Miku_Hatsune.pmd",
                                    "local-data/mmd/MikuMikuDanceE_v932/UserFile/Model/Miku_Hatsune.pmd", ".pmd")
        source = read_skeleton(skeleton_path)
        model_name, target, iks = read_pmd(model_path)
        retargeter = Retargeter(source, target, iks, args.body_only, args.motion_scale)
        output.mkdir(parents=True, exist_ok=True)
        print(f"SAN: {len(paths)}; скелет: {skeleton_path.name}; модель: {model_path.name}")
        print(f"Масштаб перемещений: {retargeter.scale:.6f}; ноги FK; IK ног отключается в VMD.")
        rows = []
        for path in paths:
            destination = output / (path.stem + ".vmd")
            row = {"source": str(path), "output": str(destination)}
            if destination.exists() and not args.overwrite:
                row["status"] = "skipped"
                print(f"ПРОПУСК {path.name}: VMD уже существует (для замены --overwrite).")
            else:
                try:
                    clip = read_san(path, set(retargeter.order))
                    frames, bones = write_vmd(destination, clip, retargeter, model_name)
                    row.update(status="ok", duration=clip.duration, frames=frames, bones=bones,
                               ignored_tracks=clip.ignored, ignored_tags=clip.tags,
                               missing_tracks=sorted(set(retargeter.order)-set(clip.tracks)))
                    print(f"ГОТОВО {path.name} -> {destination.name}: {frames} кадров, {bones} костей")
                except (OSError, ValueError, struct.error, OverflowError) as error:
                    row.update(status="error", error=str(error))
                    print(f"ОШИБКА {path.name}: {error}")
            rows.append(row)
        report = {"version": "0.1.0-test", "skeleton": str(skeleton_path), "model": str(model_path),
                  "motion_scale": retargeter.scale, "disabled_ik": retargeter.disabled_ik,
                  "extra_target_parents": retargeter.extra_parents, "files": rows}
        (output / "conversion_report.json").write_text(
            json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        errors = sum(row["status"] == "error" for row in rows)
        completed = sum(row["status"] == "ok" for row in rows)
        print(f"Готово: {completed}; ошибок: {errors}. Результаты и отчёт: {output}")
        return 1 if errors else 0
    except (OSError, ValueError, struct.error, OverflowError) as error:
        print(f"ОШИБКА: {error}")
        return 1


if __name__ == "__main__":
    # Старые Windows-консоли не умеют печатать японские имена. Файлы всё равно
    # сохраняются в правильных кодировках, а неподдержанные символы экрана заменяются.
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(errors="replace")
    raise SystemExit(main())
