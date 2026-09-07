#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SAN → VMD: положите один SMO, одну PMD и анимации SAN в папку input.

Запустите run.bat или этот файл. Готовые анимации появятся в папке output.
Нужен Python 3.10 или новее. Blender и дополнительные библиотеки не нужны.

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

from bisect import bisect_right  # Находит, между какими ключами находится время.
from collections import Counter  # Считает повторения имён костей в PMD.
from dataclasses import dataclass  # Создаёт простые записи с именованными полями.
import json  # Сохраняет текстовый отчёт о конвертации.
import math  # Корни, синусы и другие операции для вращений.
from pathlib import Path  # Работа с папками и файлами.
import struct  # Превращает байты файла в числа и обратно.
import sys  # Код завершения программы и настройка Windows-консоли.


# ------------------------- Понятные настройки -------------------------

# __file__ — имя самого скрипта. Поэтому папки ищутся рядом с ним даже при
# запуске из другого рабочего каталога. Путей к игровому проекту здесь нет.
SCRIPT_DIR = Path(__file__).resolve().parent
INPUT_DIR = SCRIPT_DIR / "input"
OUTPUT_DIR = SCRIPT_DIR / "output"
VERSION = "0.2.0-test"
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


class ConversionError(ValueError):
    """Ожидаемая ошибка входного файла, которую можно объяснить пользователю."""


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


def slerp(a, b, amount):
    """Плавный поворот между ключами: amount=0 даёт a, amount=1 даёт b."""
    # q и -q описывают одну позу. Выбираем короткий путь, иначе будет полный оборот.
    dot = sum(x*y for x, y in zip(a, b))
    if dot < 0:
        b, dot = times(b, -1), -dot
    dot = min(1.0, dot)
    if dot > 0.9995:
        # Для почти одинаковых поворотов простая смесь точнее, чем деление
        # на очень маленький синус угла в общей формуле ниже.
        return normalize(add(times(a, 1-amount), times(b, amount)))
    angle = math.acos(dot)
    return normalize(add(times(a, math.sin((1-amount)*angle) / math.sin(angle)),
                         times(b, math.sin(amount*angle) / math.sin(angle))))


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


def floats(payload, count):
    """Прочитать ровно count вещественных чисел, например три координаты сустава."""
    r = Reader(payload)
    values = r.unpack("f" * count)
    r.done()
    if not all(math.isfinite(v) for v in values):
        raise ConversionError("В данных встретились NaN или бесконечность.")
    return values


@dataclass
class Entry:
    """Один объект из каталога игрового файла; меш и кость — разные объекты."""
    name: str  # Имя объекта, например Head.
    kind: int  # Числовой идентификатор класса: кость, анимация, меш и т. д.
    data: memoryview  # Только байты этого объекта, без общего заголовка.


def read_ffps(path):
    """Общая оболочка SMO/SAN. В каталоге хранятся ID, имена и границы объектов."""
    raw = read_bytes(path)
    r = Reader(raw)
    # SMO и SAN имеют одинаковую внешнюю оболочку FFPS. Первые 32 байта
    # сообщают версию, платформу, размеры и место начала данных объектов.
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
        # Каталог похож на оглавление: номер объекта, имя, класс, смещение, длина.
        # Смещение считается от начала области данных, а не от начала файла.
        object_id, name_size = table.unpack("IH")
        name = table.text(name_size)
        kind, offset, extent = table.unpack("III")
        if object_id in entries or extent < 8 or offset + extent > data_size:
            raise ConversionError("Некорректная запись каталога FFPS.")
        body = memoryview(raw)[start + offset:start + offset + extent]
        # У каждого объекта свой маленький заголовок. Проверяем, что оглавление
        # действительно привело к объекту нужного класса, а не к случайным байтам.
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
            # Номера 31 и выше не помещаются в 5 бит: настоящий номер идёт следом.
            kind = r.number("B")
        if code == 0:
            # Для поддержанных concrete spNode/spAnimation здесь заканчивается объект.
            r.done()
            return
        # Короткие поля имеют длину 1/2/4/8 прямо в заголовке. Для остальных
        # длина записана следующим числом шириной 1, 2 или 4 байта.
        if code <= 4:
            size = (0, 1, 2, 4, 8)[code]
        else:
            size = r.number({5: "B", 6: "H", 7: "I"}[code])
        # yield отдаёт одно поле вызывающему циклу и продолжает со следующего.
        yield kind, r.take(size)
    raise ConversionError("Нет завершения секции Sparkplug.")


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


def read_skeleton(path):
    """Из SMO берём concrete spNode и их field 5, а меши/текстуры пропускаем."""
    entries = read_ffps(path)
    nodes = {i: e for i, e in entries.items() if e.kind == 0x695C0F65}
    # Этот номер класса обозначает обычный spNode — узел с положением и поворотом.
    # Сетки, материалы и текстуры остаются за пределами выбранного набора.
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
                # В игровом узле поля 0/1/2 — положение, поворот и масштаб.
                bone.position = floats(payload, 3)
            elif kind == 1:
                bone.rotation = normalize(floats(payload, 4))
            elif kind == 2:
                # Масштаб проверяется позже, только в используемой ветке тела.
                # У лишней кости/маркера он не должен мешать конвертации.
                bone.scale = floats(payload, 3)
            elif kind == 5:
                # Поле 5 — ссылка на ребёнка. Нельзя считать, что соседний
                # объект в файле автоматически является дочерней костью.
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
        # Связи строим после чтения всех узлов: ребёнок мог стоять раньше родителя.
        if bones[child].parent not in (None, parent):
            raise ConversionError(f"У узла {child} несколько родителей.")
        bones[child].parent = parent
    return bones


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


# ----------------------- Кривые анимации SAN --------------------------

@dataclass
class Curve:
    """Одна величина, изменяющаяся во времени: например положение или поворот."""
    representation: int  # 1 — векторные ключи; 3 — скалярные; 4 — кубические.
    times: tuple  # Моменты ключей в секундах, строго по возрастанию.
    values: tuple  # Значения в те же моменты; для cubic также хранятся наклоны.

    def sample(self, time, quaternion=False):
        """Узнать значение между ключами, например положение на секунде 0.25."""
        if not self.times:
            return None  # Пустая кривая сохраняет компоненту исходной позы SMO.
        if time <= self.times[0] or len(self.times) == 1:
            return self.values[0][:1] if self.representation >= 3 else self.values[0]
        if time >= self.times[-1]:
            return self.values[-1][:1] if self.representation >= 3 else self.values[-1]
        # Находим два соседних ключа a и b. Доля u показывает, как далеко
        # мы между ними: 0 — первый ключ, 0.5 — середина, 1 — второй ключ.
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
            # Кубический полином учитывает не только концы, но и наклон кривой.
            # Используется доля u внутри интервала, а не абсолютное время клипа.
            return (a[0] + u*(a[2] + u*(c2 + u*c3)),)
        if quaternion:
            return slerp(a, b, u)
        # Обычное линейное смешивание координат: доля от a плюс доля от b.
        return add(times(a, 1-u), times(b, u))


def read_curve(payload, role):
    """Разобрать ключи одного SAN-канала: role 2=позиция, 3=поворот, 4=масштаб.

    Игра может хранить XYZ вместе или как три отдельные кривые с собственными
    временами ключей. Возвращаем список из одной либо трёх Curve.
    """
    r = Reader(payload)
    curves, axes = [], 1
    while len(curves) < axes:
        representation = r.number("I")
        if representation == 0:
            # Нулевое представление означает отсутствие канала, а не нулевую позу.
            if curves:
                raise ConversionError("Неполная тройка скалярных SAN-кривых.")
            r.done()
            return []
        if representation not in (1, 3, 4) or (role == 3 and representation != 1):
            raise ConversionError(f"Не поддерживается SAN representation {representation}, поле {role}.")
        if not curves and representation >= 3:
            # Скаляр описывает только одну ось: нужно прочитать ещё две.
            axes = 3
        if curves and representation not in (3, 4):
            raise ConversionError("Смешаны векторные и скалярные кривые.")
        count = r.number("I")
        # Сколько float-чисел занимает один ключ. Явные ветки длиннее одной
        # формулы, зато видно, какой размер соответствует какому виду данных.
        if representation == 4:
            stride = 5  # Значение, два наклона, два служебных коэффициента.
        elif representation == 3:
            stride = 1  # Одно число для одной оси.
        elif role == 3:
            stride = 4  # Кватернион XYZW.
        else:
            stride = 3  # Вектор XYZ.
        if count > len(payload) // (4 * (stride+1)):
            raise ConversionError("Некорректное число SAN-ключей.")
        # В SAN сначала идут ВСЕ времена, затем ВСЕ значения, а не пары время/значение.
        key_times = r.unpack("f" * count)
        values = tuple(r.unpack("f" * stride) for _ in range(count))
        if not all(math.isfinite(t) and t >= 0 for t in key_times):
            raise ConversionError("Некорректное время ключа SAN.")
        if any(b <= a for a, b in zip(key_times, key_times[1:])):
            raise ConversionError("SAN-ключи должны идти по возрастанию времени.")
        used_width = 3 if representation == 4 else stride
        # У кубического ключа последние два числа пересчитываются при чтении.
        # Их мусорное содержимое не должно портить проверенные первые три числа.
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
    """Общий доступ к каналу: пустой сохраняет исходную позу, три оси собираются в XYZ."""
    if not curves or not curves[0].times:
        return fallback
    if len(curves) == 3:
        return tuple(c.sample(time)[0] for c in curves)
    return curves[0].sample(time, quaternion)


@dataclass
class Clip:
    """Одна прочитанная анимация SAN."""
    duration: float  # Длительность в секундах.
    tracks: dict  # Имя кости → номер канала → список кривых.
    tags: int  # Число игровых событий, например звуков шагов; в VMD не переносим.
    ignored: list  # Имена ненужных треков для текстового отчёта.


def read_san(path, required):
    """Читаем только движения костей из required, включая их нужных родителей."""
    entries = list(read_ffps(path).values())
    if len(entries) != 1 or entries[0].kind != 0x56EE563A:
        raise ConversionError("Ожидается один spAnimation в SAN.")
    tracks, pending, ignored, names = {}, {}, [], set()
    duration, tags = None, 0
    # Особенность SAN: сначала записаны каналы, а ПОСЛЕ них — имя кости.
    # pending временно хранит байты каналов. Когда узнаем имя, решим, нужны ли они.
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
        # Пустой результат часто означает, что SAN взят от совсем другого скелета.
        raise ConversionError("В SAN нет движения для выбранного скелета.")
    return Clip(duration, tracks, tags, ignored)


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


def source_world(bones, order, clip=None, time=0):
    """Вычислить позу игрового скелета относительно всей сцены.

    Без clip получаем исходную позу SMO. С clip подставляем значения SAN
    на нужной секунде; отсутствующие каналы оставляем как в исходном SMO.
    """
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
            # Локальная позиция — смещение от родителя. Сначала поворачиваем
            # это смещение вместе с родителем, потом прибавляем его положение.
            parent_position, parent_rotation = result[bone.parent]
            position = add(parent_position, rotate(parent_rotation, position))
            rotation = normalize(multiply(parent_rotation, rotation))
        result[name] = (position, rotation)
    return result


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
        self.rest = source_world(source, self.order)
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

    def pose(self, clip, time):
        """Получить положение и поворот каждой выходной кости в один момент времени."""
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
    temporary = path.with_suffix(".vmd.tmp")
    try:
        with temporary.open("wb") as stream:
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
        if temporary.exists():
            temporary.unlink()
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
    source = read_skeleton(skeleton_path)
    model_name, target, iks = read_pmd(model_path)
    rig = Retargeter(source, target, iks, BODY_ONLY, MOTION_SCALE)
    print(f"Скелет: {skeleton_path.name}; модель MMD: {model_path.name}; анимаций: {len(paths)}")
    print("Готовые VMD появятся в output. Одноимённые результаты будут обновлены.")

    # Шаг 4. Обрабатываем по одной анимации, чтобы не хранить весь набор в памяти.
    # Ошибка одного SAN не мешает получить остальные исправные анимации.
    rows = []
    for path in paths:
        destination = output / (path.stem + ".vmd")
        row = {"source": path.name, "output": destination.name}
        try:
            clip = read_san(path, set(rig.order))
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
