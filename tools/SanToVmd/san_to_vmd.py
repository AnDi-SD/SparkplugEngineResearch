#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SAN → VMD: положите один SMO, одну PMD и анимации SAN в папку input.

Запустите run.bat или этот файл. Готовые анимации появятся в папке output.
Нужен 64-битный Python 3.10+ на Windows и общее ядро SparkplugViewerNative.dll.

Как читать этот скрипт, если вы только начинаете изучать Python:
1. Начните с main() в самом конце: это короткий список действий программы.
2. convert_files() находит файлы и обрабатывает анимации по одной.
3. read_skeleton(), read_pmd(), read_san() читают три разных формата.
4. Retargeter.pose() переносит позу игрового персонажа на модель MMD.
5. write_vmd() сохраняет результат в понятном MMD формате.

Слова, которые встретятся ниже:
«кость» — сустав скелета; «родитель» — кость, за которой он движется;
«исходная поза» — положение скелета до применения анимации;
«ключ» — записанное положение/вращение в конкретный момент;
«трек» — все ключи одной кости; «кривая» — изменение одной величины во времени.
Меши, текстуры, волосы и одежду мы не переносим: используем только скелеты.
"""

from __future__ import annotations

from collections import Counter  # Считает повторения имён костей в PMD.
from dataclasses import dataclass  # Создаёт простые записи с именованными полями.
from contextlib import closing
import json  # Сохраняет текстовый отчёт о конвертации.
import math  # Корни, синусы и другие операции для вращений.
from pathlib import Path  # Работа с папками и файлами.
import struct  # Превращает байты файла в числа и обратно.
import sys  # Код завершения программы и настройка Windows-консоли.
import tempfile  # Создаёт отдельный временный файл для каждой операции записи.
import sparkplug_native as native  # Только владение C++-объектами и вызовы общего ядра.


# ------------------------- Понятные настройки -------------------------

# __file__ — имя самого скрипта. Поэтому папки ищутся рядом с ним даже при
# запуске из другого рабочего каталога. Путей к игровому проекту здесь нет.
SCRIPT_DIR = Path(__file__).resolve().parent
INPUT_DIR = SCRIPT_DIR / "input"
OUTPUT_DIR = SCRIPT_DIR / "output"
VERSION = "0.4.0-dev"
FPS = 30  # VMD хранит номера кадров, а SAN — время в секундах.
MAX_BYTES = 64 * 1024 * 1024  # Не загружаем случайный гигантский файл в память.
MAX_SECONDS = 600  # Защита от случайного огромного/повреждённого duration.
ZERO = (0.0, 0.0, 0.0)  # Нулевое смещение: кость остаётся на месте.
IDENTITY = (0.0, 0.0, 0.0, 1.0)  # Quaternion везде в порядке X, Y, Z, W.
# Quaternion (кватернион) — четыре числа, описывающие поворот в пространстве.
# IDENTITY означает «не поворачивать». Это НЕ четыре угла в градусах.

# Обычному пользователю менять настройки не нужно. Для экспериментов:
BODY_ONLY = False  # True отключает перенос пальцев, оставляя тело.
MOTION_SCALE = None  # None: подобрать масштаб ходьбы по росту; число: задать вручную.

# Слева — настоящее японское имя PMD/VMD, справа — источник движения.
# Название UpperArm в игре обманчиво: эта кость находится у локтя.
BODY_MAP = {
    "下半身": "Pelvis",  # Таз / нижняя половина туловища.
    "上半身": "Spine_03",  # Мировое вращение включает Spine_01 и Spine_02.
    "首": "Neck", "頭": "Head",  # Шея и голова.
    "左肩": "L_Clavicle", "左腕": "L_Bicep",  # Левая ключица и плечо.
    "左ひじ": "L_UpperArm", "左手首": "L_Hand",  # Левый локоть и кисть.
    "右肩": "R_Clavicle", "右腕": "R_Bicep",
    "右ひじ": "R_UpperArm", "右手首": "R_Hand",
    "左足": "L_Thigh", "左ひざ": "L_calf", "左足首": "L_Ankle",  # Бедро, колено, стопа.
    "右足": "R_Thigh", "右ひざ": "R_calf", "右足首": "R_Ankle",
}
# Указательный, средний, безымянный и мизинец. Большой палец обработан отдельно:
# в игровом скелете у него три сустава, а в обычной Miku — два.
FINGERS = {"人指": "Index", "中指": "Middle", "薬指": "Ring", "小指": "Pinky"}
# IK — способ, когда MMD сам сгибает ногу, чтобы дотянуть стопу до заданной точки.
# Мы уже переносим повороты суставов (FK), поэтому IK ног нужно выключить.
LEG_IK = ("左足ＩＫ", "右足ＩＫ", "左つま先ＩＫ", "右つま先ＩＫ")


ConversionError = native.NativeError


# ---------------------- Немного математики ----------------------------
# Мы используем активные вращения и векторы-столбцы:
# world_rotation = parent_rotation * local_rotation.
# Порядок множителей существенен! Перестановка ломает руки и ноги.

def add(a, b):
    """Сложить координаты: (1, 2, 3) + (4, 5, 6) = (5, 7, 9)."""
    # zip берёт числа попарно, tuple собирает ответ в неизменяемый список.
    return tuple(x + y for x, y in zip(a, b))


def sub(a, b):
    """Вычесть координаты. Например, конец минус начало даёт направление кости."""
    return tuple(x - y for x, y in zip(a, b))


def times(v, factor):
    """Умножить каждое число на factor: так мы меняем длину или масштаб вектора."""
    return tuple(x * factor for x in v)


def length(v):
    """Длина вектора по теореме Пифагора, обобщённой на несколько координат."""
    return math.sqrt(sum(x * x for x in v))


def normalize(q):
    """Сделать длину кватерниона равной 1, чтобы он задавал корректный поворот."""
    size = length(q)
    if not math.isfinite(size) or size < 1e-10:
        raise ConversionError("Нулевой или некорректный quaternion.")
    return times(q, 1.0 / size)


def inverse(q):
    """Поворот, отменяющий q: нужен для перехода от общей позы к позе относительно родителя."""
    # Все используемые quaternion нормализованы, поэтому обратный = сопряжённый.
    x, y, z, w = q
    return (-x, -y, -z, w)


def multiply(a, b):
    """Составить два поворота: сначала применяется b, затем a.

    Ниже стандартная формула умножения кватернионов. Большие буквы — компоненты
    второго поворота. Перемножение здесь не такое, как умножение четырёх чисел
    по отдельности; менять порядок a и b нельзя.
    """
    x, y, z, w = a
    X, Y, Z, W = b
    return (w*X + x*W + y*Z - z*Y, w*Y - x*Z + y*W + z*X,
            w*Z + x*Y - y*X + z*W, w*W - x*X - y*Y - z*Z)


def rotate(q, v):
    """Повернуть трёхмерное направление v, не меняя его длины."""
    # Временно добавляем четвёртую компоненту 0, применяем q*v*q^-1,
    # затем [:3] оставляет только три координаты результата.
    return multiply(multiply(q, (*v, 0.0)), inverse(q))[:3]


def align_directions(original, desired):
    """Кратчайшее вращение между двумя направлениями кости в исходной позе."""
    if min(length(original), length(desired)) < 1e-8:
        raise ConversionError("Нулевая длина кости при выравнивании скелетов.")
    # Нас интересует направление, а не размер руки: приводим обе длины к 1.
    a, b = times(original, 1/length(original)), times(desired, 1/length(desired))

    def cross(u, v):
        # Векторное произведение даёт ось, перпендикулярную обоим направлениям.
        return (u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0])

    # Скалярное произведение равно косинусу угла. Ограничиваем его из-за
    # небольших погрешностей чисел с плавающей точкой.
    dot = max(-1.0, min(1.0, sum(x*y for x, y in zip(a, b))))
    if dot < -0.999999:
        # Для противоположных направлений ось выбирается перпендикулярно кости.
        axis = cross(a, (1, 0, 0) if abs(a[0]) < 0.9 else (0, 1, 0))
        return normalize((*axis, 0))
    return normalize((*cross(a, b), 1+dot))


# ------------------- Чтение бинарных данных ---------------------------

class Reader:
    """Помощник для чтения файла слева направо, как закладка в книге.

    data — байты, offset — место, где продолжится следующее чтение.
    Форматы бинарные: текстовый поиск имён не восстанавливает их структуру.
    Поэтому каждая функция ниже забирает ровно известное количество байтов.
    """

    def __init__(self, data):
        # memoryview позволяет смотреть кусок исходных байтов без лишней копии.
        self.data = memoryview(data)
        self.offset = 0

    def take(self, size):
        """Забрать size байтов и сдвинуть закладку; за конец секции выходить нельзя."""
        if size < 0 or size > len(self.data) - self.offset:
            raise ConversionError(f"Файл оборван около байта {self.offset}.")
        start = self.offset
        self.offset += size
        return self.data[start:self.offset]

    def unpack(self, fmt):
        """Прочитать числа по описанию: I=4-байтовое целое, H=2, B=1, f=float."""
        # '<' — порядок байтов little endian, принятый в PC-файлах игры и MMD.
        # calcsize считает нужный размер, unpack превращает эти байты в числа.
        return struct.unpack("<" + fmt, self.take(struct.calcsize("<" + fmt)))

    def number(self, fmt):
        """Короткая запись для одного числа вместо набора из одного элемента."""
        return self.unpack(fmt)[0]

    def text(self, size, encoding="latin1"):
        """Прочитать строку фиксированного размера; первый нулевой байт завершает имя."""
        return bytes(self.take(size)).split(b"\0", 1)[0].decode(encoding)

    def done(self):
        """Убедиться, что структура прочитана целиком, без необъяснённого хвоста."""
        if self.offset != len(self.data):
            raise ConversionError("Неожиданные данные в конце секции.")


def read_bytes(path):
    """Прочитать небольшой файл целиком, предварительно проверив размер."""
    if path.stat().st_size > MAX_BYTES:
        raise ConversionError(f"Файл больше {MAX_BYTES // 1024**2} МиБ: {path.name}")
    return path.read_bytes()


@dataclass
class Bone:
    """Нужная нам часть описания кости, без вершин и физических настроек.

    В SMO position/rotation заданы относительно родителя.
    В PMD position — положение в исходной модели, rotation по умолчанию единичен.
    parent=None означает, что кость находится в корне скелета.
    """
    name: str
    parent: str | None
    position: tuple
    rotation: tuple = IDENTITY
    kind: int = 0
    scale: tuple = (1.0, 1.0, 1.0)
    object_id: int = 0
    orientation: tuple | None = None
    billboard: int = 0


class Skeleton(dict):
    """Имена для профиля MMD и владелец настоящего графа ресурсов Sparkplug."""
    def __init__(self, graph=None):
        super().__init__()
        self.native_graph = graph

    def close(self):
        if self.native_graph is not None:
            self.native_graph.close()


def read_skeleton(path):
    """Общий loader читает SMO целиком; профиль использует реальные spNode."""
    graph = native.Graph(read_bytes(path))
    try:
        bones = Skeleton(graph)
        nodes = [(name, info, node) for name, info, node in graph.objects if node is not None]
        names = {info.id: name for name, info, node in nodes}
        for name, info, node in nodes:
            if name in bones:
                raise ConversionError(f"Неоднозначное имя узла в SMO: {name}")
            bones[name] = Bone(name, names[node.parent] if node.parent else None,
                               tuple(node.position), tuple(node.rotation), scale=tuple(node.scale),
                               object_id=info.id, orientation=tuple(node.orientation),
                               billboard=(node.flags >> 20) & 3)
        return bones
    except BaseException:
        graph.close()
        raise


def read_pmd(path):
    """Читаем скелет целевой модели, не изменяя её PMD-файл.

    PMD хранит позиции суставов относительно всей модели, а родителей — как
    номера в списке костей. Сначала читаем список, затем переводим номера в имена.
    """
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
        parent, tail, kind, influence, x, y, z = r.unpack("HHBH3f")
        # tail задаёт конец кости, influence — зависимое вращение некоторых
        # служебных костей. Нам нужны только сустав, родитель и тип.
        if not all(math.isfinite(v) for v in (x, y, z)):
            raise ConversionError("Некорректные позиции костей PMD.")
        rows.append((name, parent, kind, (x, y, z)))
    # В комплектной MEIKO два конца кости называются одинаково. В самом PMD
    # это допустимо: их различают номера. Даём таким концам разные ВНУТРЕННИЕ
    # ключи словаря. В VMD эти ключи не попадут — концы костей не анимируются.
    # Повторные имена управляемых костей всё ещё неоднозначны для VMD.
    counts = Counter(row[0] for row in rows)
    parents = {row[1] for row in rows}
    names = []
    for index, (name, parent, kind, position) in enumerate(rows):
        if not name or (parent != 65535 and parent >= len(rows)):
            raise ConversionError("Пустое имя или неверный родитель кости PMD.")
        if counts[name] > 1:
            if kind != 7 or index in parents:
                raise ConversionError(f"Повторное имя управляемой кости PMD: {name}")
            names.append(f"{name} [PMD #{index}]")
        else:
            names.append(name)
    if len(set(names)) != len(names):
        raise ConversionError("Не удалось различить служебные имена PMD.")
    bones = {}
    for index, (name, parent, kind, position) in enumerate(rows):
        parent_name = None if parent == 65535 else names[parent]
        bones[names[index]] = Bone(name, parent_name, position, kind=kind)
    iks = []
    for _ in range(r.number("H")):
        controller, target, count, _, _ = r.unpack("HHBHf")
        chain = r.unpack("H" * count)
        if any(i >= len(rows) for i in (controller, target, *chain)):
            raise ConversionError("Некорректная IK-цепочка PMD.")
        iks.append(names[controller])
    # Дальше идут морфы/физика: они нужны MMD, а конвертер их не изменяет.
    return model_name, bones, iks


# ----------------------- Общее ядро анимации SAN ----------------------

@dataclass
class Clip:
    """Метаданные конвертации и владелец реального spAnimation в C++."""
    duration: float
    tracks: dict  # Имя → PRS role 2/3/4 → thin native Channel.
    tags: int
    ignored: list
    native: native.Animation | None = None

    def close(self):
        if self.native is not None:
            self.native.close()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()


def validate_vmd_scale(channel):
    """Ограничение выходного VMD: масштаб не представим этим форматом.

    Значения и подготовленные коэффициенты получены из spAnimTrack. Здесь
    нет разбора SAN или вычисления кривой; проверяется пригодность экспорта.
    """
    for info, values in channel.prepared_axes():
        width = 1 if info.representation >= 3 else 3
        for index in range(info.keys):
            row = values[index*info.stride:(index+1)*info.stride]
            if any(abs(v-1) > 0.001 for v in row[:width]):
                raise ConversionError("Анимация масштаба не представима в VMD.")
            if info.representation in (2, 4) and any(abs(v) > 0.001 for v in row[width:3*width]):
                raise ConversionError("Кубическая анимация масштаба не представима в VMD.")


def read_san(path, required):
    """Общее C++-ядро читает SAN; конвертер выбирает нужные имена и роли."""
    animation = native.Animation(read_bytes(path))
    try:
        if not 0 < animation.duration <= MAX_SECONDS:
            raise ConversionError("Нет корректной длительности SAN.")
        tracks, ignored = {}, []
        for track in animation.tracks:
            if track.name not in required:
                ignored.append(track.name)
                continue
            chosen = tracks.setdefault(track.name, {})
            # Та же прикладная политика, что у Viewer: первое совпадение каждой
            # PRS-роли; одно имя может иметь разные роли в отдельных треках.
            for role, channel in track.channels.items():
                if channel.source_keys and role+2 not in chosen:
                    if role == 2:
                        validate_vmd_scale(channel)
                    chosen[role+2] = channel
        if not any(role in (2, 3) for channels in tracks.values() for role in channels):
            raise ConversionError("В SAN нет движения для выбранного скелета.")
        return Clip(animation.duration, tracks, animation.tags, ignored, animation)
    except BaseException:
        animation.close()
        raise


# --------------------- Перенос позы между скелетами --------------------

def hierarchy(bones, selected):
    """Составить порядок расчёта: сначала родители, затем их дети.

    Чтобы вычислить положение кисти, сначала нужно вычислить локоть и плечо.
    Поэтому добавляем предков, даже если их нет в таблице BODY_MAP.
    Лишние боковые ветви — например волосы — при этом в список не попадают.
    """
    order, visited, active = [], set(), set()

    def visit(name):
        # active — кости, к которым мы ещё не закончили подниматься по родителям.
        # Повтор в active означает замкнутую цепочку, которую вычислить нельзя.
        # visited — уже добавленные кости, чтобы не считать общий корень дважды.
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


def scene_world(scene, order, clip=None, time=0):
    """spNodeController и spNode вычисляют PRS; здесь только DTO для MMD."""
    if clip is not None:
        if clip.native is None:
            raise ConversionError("Клип не содержит объекта общего SAN-ядра.")
        if scene.bound is not clip.native:
            roles = []
            for name in order:
                channels = clip.tracks.get(name, {})
                roles.extend(channels[role].ordinal if role in channels else -1 for role in (2, 3, 4))
            scene.bind(clip.native, roles)
    result = {}
    for name, (position, rotation, scale) in zip(order, scene.sample(time)):
        if any(abs(v-1) > 0.001 for v in scale):
            raise ConversionError(f"Масштабирование {name} нельзя записать в VMD.")
        result[name] = (position, rotation)
    return result


def source_world(bones, order, clip=None, time=0):
    """Однократный вызов общего ядра; Retargeter повторно использует одну сцену."""
    with native.Scene(bones, order) as scene:
        return scene_world(scene, order, clip, time)


def bone_mapping(target, body_only):
    """Составить соответствие MMD → игра: обязательное тело и доступные пальцы."""
    mapping = dict(BODY_MAP)
    # В профиле с двумя суставами позвоночника делим исходную цепочку на две.
    if "上半身2" in target:
        mapping["上半身"] = "Spine_02"
        mapping["上半身2"] = "Spine_03"
    if not body_only:
        for side, prefix in (("左", "L"), ("右", "R")):
            # 左/右 — левая/правая сторона. Японские цифры ниже полноширинные:
            # замена на обычные 1/2/3 изменит имя кости и MMD её не найдёт.
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
    """Переносчик поз между двумя скелетами.

    __init__ один раз подготавливает соответствия и исходные позы.
    pose вызывается для каждого кадра и возвращает локальные движения костей MMD.
    """
    def __init__(self, source, target, iks, body_only=False, motion_scale=None):
        # 1. Проверяем, что целевая модель действительно имеет человеческое тело.
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
        self.native_scene = native.Scene(source, self.order)
        try:
            self.rest = scene_world(self.native_scene, self.order)
            # 2. Ищем исходный наклон рук и ног. У игры и MMD он может отличаться:
            # просто одинаковые углы суставов ещё не дают одинаковые направления рук.
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
            # 3. Если высота таза в игре 100 единиц, а в MMD 13, перемещение на
            # 10 игровых единиц должно стать перемещением на 1.3 единицы MMD.
            if source_height <= 0 or target_height <= 0:
                raise ConversionError("Не удалось определить высоту таза над стопами.")
            self.scale = motion_scale if motion_scale is not None else target_height/source_height
            if not math.isfinite(self.scale) or self.scale <= 0:
                raise ConversionError("Масштаб перемещения должен быть положительным числом.")
            self.disabled_ik = [name for name in LEG_IK if name in iks]
            unexpected = [name for name in self.target_order
                          if name not in self.mapping and name != "センター"]
            # В Luka и Miku Ver2 между плечом и локтем стоят кости скручивания,
            # type 8. Они могут крутиться только вокруг своей оси. Нулевой поворот
            # допустим для любой оси: оставляем их нейтральными, а движение переносим
            # обычными костями рук. Длину цепочки и наследование родителей сохраняем.
            # Это простой перенос позы, без распределения скручивания по руке.
            self.extra_parents = unexpected
            if any(target[name].kind not in (0, 1, 8) for name in unexpected):
                raise ConversionError("Дополнительный родитель PMD использует неподдерживаемый тип кости.")
            # Записываем нейтральные ключи явно, чтобы прежнее вращение служебной
            # кости в сцене MMD не добавилось к новой анимации.
            self.neutral_bones = unexpected
        except BaseException:
            self.native_scene.close()
            raise


    def close(self):
        self.native_scene.close()

    def pose(self, clip, time):
        """Получить положение и поворот каждой выходной кости в один момент времени."""
        animated = scene_world(self.native_scene, self.order, clip, time)
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
                # VMD ждёт вращение относительно родителя. Отменяем уже учтённый
                # поворот родителя, иначе плечо повернёт локоть второй раз.
                local[name] = (ZERO, normalize(multiply(inverse(parent_rotation), desired)))
            else:
                # Нейтральная кость следует за родителем, но сама не поворачивается.
                desired = parent_rotation
                local[name] = (ZERO, IDENTITY)
            target_world[name] = desired
        # Центр MMD не расположен в тазу. Передаём ему только СМЕЩЕНИЕ таза
        # относительно исходного SMO; остальная поза использует длины PMD.
        # Не вычитаем кадр 0: в нём уже может быть присед, который надо сохранить.
        shift = times(sub(animated["Pelvis"][0], self.rest["Pelvis"][0]), self.scale)
        local["センター"] = (shift, IDENTITY)
        return local


# --------------------------- Запись VMD -------------------------------

def encoded_name(name, size):
    """Поместить японское имя в строку VMD фиксированной длины в БАЙТАХ."""
    # Японская буква в CP932 обычно занимает два байта, поэтому len(name)
    # не годится для проверки размера. Оставшееся место дополняем нулями.
    data = name.encode("cp932")
    if len(data) > size:
        raise ConversionError(f"Имя {name} не помещается в {size} байт VMD.")
    return data.ljust(size, b"\0")


def linear_interpolation():
    """Параметры перехода между двумя соседними кадрами VMD."""
    # Четыре Bezier-кривые: X/Y/Z и вращение. Контрольные точки (20,20),
    # (107,107) лежат на диагонали и задают линейное движение. VMD содержит
    # перекрывающиеся копии этих параметров, поэтому это не просто 64 нуля.
    base = bytes([20]*8 + [107]*8)
    return b"".join(base[i:] + bytes(i) for i in range(4))


def frame_count(duration):
    """Сколько кадров записать, включая кадр 0 и конечную позу."""
    # Float32(1.2)*30 чуть больше 36: не добавляем лишний кадр из-за округления.
    return math.ceil(duration * FPS - 1e-5) + 1


def write_vmd(path, clip, retargeter, model_name):
    """Запечь движение при 30 кадрах/с и записать бинарный VMD.

    «Запечь» означает вычислить готовую позу на каждом кадре. MMD не потребуется
    разбираться в игровых кривых SAN — он получит свои обычные ключи.
    """
    count = frame_count(clip.duration)
    names = ["センター", *retargeter.mapping, *retargeter.neutral_bones]
    encoded = {name: encoded_name(name, 15) for name in names}
    model = encoded_name(model_name, 20)
    interpolation = linear_interpolation()
    previous = {}
    # Временный файл позволяет не оставлять наполовину записанный VMD при ошибке.
    # Готовый результат заменяется только после успешного расчёта всех кадров.
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode="wb", dir=path.parent,
                prefix=path.name + ".", suffix=".tmp", delete=False) as stream:
            temporary = Path(stream.name)
            # Заголовок: 30 байтов сигнатуры формата + 20 байтов имени модели.
            stream.write(b"Vocaloid Motion Data 0002".ljust(30, b"\0") + model)
            stream.write(struct.pack("<I", count * len(names)))
            # Число ключей = число кадров × число записываемых костей.
            # Каждый ключ занимает 111 байтов: имя 15, кадр 4, позиция 12,
            # поворот 16 и параметры перехода к следующему ключу 64.
            for frame in range(count):
                pose = retargeter.pose(clip, min(frame / FPS, clip.duration))
                for name in names:
                    position, rotation = pose[name]
                    if not all(math.isfinite(v) for v in (*position, *rotation)):
                        raise ConversionError("При переносе получилась некорректная поза.")
                    if name in previous and sum(a*b for a, b in zip(previous[name], rotation)) < 0:
                        # q и -q дают один поворот, но смена знака между ключами
                        # может заставить чужой проигрыватель выбрать длинный путь.
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
        # Убираем только собственный временный файл. При ошибке старый готовый
        # VMD остаётся на месте: замена выше происходит после успешной записи.
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    return count, len(names)


# ------------------- Запуск: input → output ---------------------------

def find_files(directory, extension):
    """Список файлов одного типа прямо в input, без обхода вложенных папок."""
    # lower() позволяет одинаково воспринимать .san, .SAN и .San.
    # sorted() делает порядок обработки предсказуемым: по имени файла.
    return sorted(path for path in directory.iterdir()
                  if path.is_file() and path.suffix.lower() == extension)


def single_file(directory, extension, description):
    """Выбираем единственный SMO/PMD. Если выбор неоднозначен, объясняем ошибку."""
    paths = find_files(directory, extension)
    if not paths:
        raise ConversionError(f"Положите в input один {extension.upper()}: {description}.")
    if len(paths) > 1:
        names = ", ".join(path.name for path in paths)
        raise ConversionError(f"В input несколько {extension.upper()}: {names}. Оставьте только один.")
    return paths[0]


def convert_files(directory, output):
    """Полный рабочий цикл. Аргументы-папки нужны также для изолированных тестов."""
    # Шаг 1. Создаём папки, если пользователь их случайно удалил.
    directory.mkdir(parents=True, exist_ok=True)
    output.mkdir(parents=True, exist_ok=True)

    # Шаг 2. Весь комплект выбирается только из input. Никаких запасных Bloom
    # или Miku из проекта: отсутствие нужного файла должно быть заметно сразу.
    skeleton_path = single_file(directory, ".smo", "исходная игровая модель, например Icy.smo")
    model_path = single_file(directory, ".pmd", "модель MMD, на которой будет проигрываться движение")
    paths = find_files(directory, ".san")
    if not paths:
        raise ConversionError("Положите в input хотя бы один SAN с анимацией выбранного SMO.")

    # Шаг 3. Скелеты читаем один раз: они одинаковы для всех SAN этого запуска.
    with closing(read_skeleton(skeleton_path)) as source:
        model_name, target, iks = read_pmd(model_path)
        with closing(Retargeter(source, target, iks, BODY_ONLY, MOTION_SCALE)) as rig:
            print(f"Скелет: {skeleton_path.name}; модель MMD: {model_path.name}; анимаций: {len(paths)}")
            print("Готовые VMD появятся в output. Одноимённые результаты будут обновлены.")

            # Шаг 4. Обрабатываем по одной анимации, чтобы не хранить весь набор в памяти.
            # Ошибка одного SAN не мешает получить остальные исправные анимации.
            rows = []
            for path in paths:
                destination = output / (path.stem + ".vmd")
                row = {"source": path.name, "output": destination.name}
                try:
                    with read_san(path, set(rig.order)) as clip:
                        frames, bones = write_vmd(destination, clip, rig, model_name)
                        row.update(status="ok", duration=clip.duration, frames=frames, bones=bones,
                                   ignored_tracks=clip.ignored, ignored_tags=clip.tags,
                                   missing_tracks=sorted(set(rig.order)-set(clip.tracks)))
                    print(f"ГОТОВО {path.name} → {destination.name}: {frames} кадров")
                except (OSError, ValueError, struct.error, OverflowError) as error:
                    row.update(status="error", error=str(error))
                    print(f"ОШИБКА {path.name}: {error}")
                rows.append(row)

            # Шаг 5. Сохраняем читаемый отчёт. Там видно, какие файлы удались,
            # какие дополнительные кости были отброшены и чего не хватило в SAN.
            report = {"version": VERSION, "skeleton": skeleton_path.name, "model": model_path.name,
                      "motion_scale": rig.scale, "disabled_ik": rig.disabled_ik,
                      "neutral_target_bones": rig.neutral_bones, "files": rows}
            (output / "conversion_report.json").write_text(
                json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
            errors = sum(row["status"] == "error" for row in rows)
            print(f"Готово: {len(rows)-errors}; ошибок: {errors}. Результаты: {output}")
            return 1 if errors else 0  # Ноль означает успешное завершение программы.


def main():
    """Точка входа: запускаем конвертацию и показываем понятные ошибки входов."""
    try:
        # Старые параметры путей больше не нужны. Их нельзя молча проигнорировать:
        # иначе человек решит, что преобразовал файлы из указанной им папки.
        if len(sys.argv) > 1:
            raise ConversionError("Параметры запуска не нужны. Положите SMO, PMD и SAN в input и запустите run.bat.")
        return convert_files(INPUT_DIR, OUTPUT_DIR)
    except (OSError, ValueError, struct.error, OverflowError) as error:
        print(f"ОШИБКА: {error}")
        return 1


if __name__ == "__main__":
    # Этот блок выполняется при запуске файла, но не при импорте из тестов.
    # Старые Windows-консоли не умеют печатать японские имена. Файлы всё равно
    # сохраняются в правильных кодировках, а неподдержанные символы экрана заменяются.
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(errors="replace")
    raise SystemExit(main())
