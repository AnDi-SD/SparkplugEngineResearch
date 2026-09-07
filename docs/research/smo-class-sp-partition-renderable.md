# `spPartitionRenderable` (`0x94BBCA2A`)

Статус: **завершён полный структурный/read-only разбор наблюдаемого PC/PS2
layout**. Проверены все уникальные экземпляры в `pc-working`, `pc-pristine` и
`ps2-pristine`, оба executable и все физические связи в object directory.

## Назначение

`spPartitionRenderable` — лист spatial-partition дерева, который группирует
одну или несколько моделей. Сам объект не хранит transform и не является
размещением сцены: его родитель всегда `spPartitionNode`, а каждая модель уже
содержит собственное renderable-поддерево. Поэтому размеры экземпляров от 228
до 559 239 байт отражают размеры вложенных `spModel`, а не сотни собственных
полей или подвидов `spPartitionRenderable`.

Во всех трёх корпусах найден только один структурный вариант:

```text
SBOO spPartitionRenderable
field 1: UInt32 DebugColor (ARGB)
repeat 1..67 times:
    field 0: UInt32 objectId
             UInt32 inlineSerializedSize
             inline SBOO spModel
field 0: empty section terminator
```

Relationship payload имеет обычный Sparkplug inline-layout: размер после
первых восьми байт равен `inlineSerializedSize`, затем идут type hash и `SBOO`
целевого объекта. Во всех 19 989 случаях ID ненулевой, target однозначно
разрешается в `spModel`, совпадает с физическим child текущего
`spPartitionRenderable` и хранится inline. ID-only, sized-reference, null и
targets других классов не обнаружены.

## Корпуса

| Корпус | Объекты | SMO | Физические вхождения | Модели | Моделей на объект | Размер объекта |
|---|---:|---:|---:|---:|---:|---:|
| `pc-working` | 2 324 | 29 | 2 324 | 6 541 | 1..67 | 228..546 711 |
| `pc-pristine` | 2 324 | 29 | 2 324 | 6 541 | 1..67 | 228..546 711 |
| `ps2-pristine` | 2 560 | 30 | 2 696 | 6 907 | 1..67 | 354..559 239 |
| **Всего** | **7 208** | **88** | **7 344** | **19 989** | **1..67** | — |

Все 7 208 объектов безымянны. Все они имеют физического родителя
`spPartitionNode`. Дополнительные 236 уникальных PS2-объектов находятся в
PS2-only `Gardenia03.smo`; увеличенное число физических вхождений PS2 связано
с повтором одинаковых SMO в разных PCK, а не с дополнительными уникальными
формами.

Наиболее часты один, два и три `spModel`. Для каждой PC-копии это соответственно
843, 668 и 362 объекта; на PS2 — 978, 745 и 383. Максимальные 67 моделей
встречаются по одному разу в каждом корпусе. Cardinality меняется, но порядок
полей и encoding остаются одинаковыми.

## Поля

### Field 1 — `DebugColor`

PC и PS2 executable независимо называют поле
`eprsfPartitionRenderableDebugColor`. Это обязательный `UInt32` ARGB; в файле
little-endian слово представлено байтами BGRA. Viewer показывает значение как
`#AARRGGBB` и отдельно каналы A/R/G/B.

В обоих форматах наблюдаются одни и те же 19 точных значений. Первые восемь
образуют часто используемую синюю шкалу, остальные являются редкими
акцентными цветами:

| ARGB | PC pristine | PS2 pristine |
|---|---:|---:|
| `0x8F48488F` | 224 | 255 |
| `0x9F50509F` | 245 | 278 |
| `0xAF5858AF` | 289 | 313 |
| `0xBF6060BF` | 319 | 344 |
| `0xCF6868CF` | 227 | 258 |
| `0xDF7070DF` | 257 | 295 |
| `0xEF7878EF` | 307 | 332 |
| `0xFF8080FF` | 357 | 386 |
| остальные 11 значений | 99 | 99 |

Название `DebugColor` доказано бинарниками. Конкретное runtime-назначение
отдельных цветов пока не проверено контролируемой мутацией, поэтому редактор
поля не включён.

### Field 0 — `Renderable`

Executable называют повторяемое поле
`eprsfPartitionRenderableRenderable`. Writer получает элементы через
`pPartitionRenderable->GetRenderable( i )` и записывает их после цвета.
Наблюдаемый диапазон — 1..67 отношений на объект, всего 19 989. Пустого списка
нет: нулевой field 0 в конце является terminator serializer-секции, а не
отношением.

## PC/PS2 сравнение

Две PC-копии совпадают побайтно для всех 2 324 объектов во всех 29 общих
ресурсах. Для 29 общих PC/PS2 ресурсов совпадает число
`spPartitionRenderable`; по same-path/ordinal сопоставлены 2 324 объекта:

- `DebugColor` совпадает у 2 324 из 2 324;
- число renderables совпадает у 2 323 из 2 324;
- полная упорядоченная последовательность имён `spModel` совпадает у 2 323 из
  2 324.

Единственное отличие — `Levels/Alfea/Alfea03.smo`, ordinal 1. После 17 общих
моделей PS2 добавляет `light_ray-000` и `detach ray-000`. Это различие состава
уровня, а не платформенный вариант serializer: encoding и layout остаются
общими.

## Свидетельства executable

PC `WinxClub.exe`:

- reader: VA `0x0044ECF0..0x0044EF33`;
- writer: VA `0x0044EF40..0x0044F2BB`;
- `DebugColor` загружается в runtime offset `+0x84`;
- строки `eprsfPartitionRenderableRenderable`,
  `eprsfPartitionRenderableDebugColor`, `GetRenderable(i)` и
  `GetDebugColor().uColor` подтверждают номер, порядок и источник обоих полей.

PS2 `SLES_532.19`:

- reader: VA `0x001A2030..0x001A2220`;
- writer: VA `0x001A22E0..0x001A24D0`;
- `DebugColor` находится по runtime offset `+0x80`;
- присутствуют те же enum/debug strings и тот же порядок сериализации.

Различие runtime offsets относится к памяти платформ, а не к SMO layout.

## Реализация и база

`SmoPartitionRenderableDecoder` строго требует цвет первым, хотя бы одну
inline-модель, пустой terminator последним и совпадение каждого target с
физическим child `spModel`. Inspector восстановленных полей показывает ARGB и
каждое разрешённое relationship.

В schema v2 записаны:

- определения `partition_renderable.debug_color` и
  `partition_renderable.renderable`;
- 27 197 декодированных direct fields;
- один общий PC/PS2 variant и 7 208 подтверждённых назначений;
- четыре evidence rows: PC executable, PS2 executable, полный corpus и
  cross-corpus comparison.

Воспроизведение:

```powershell
python research/analyze_smo_partition_renderable.py `
  local-data/results/smo-corpus-v2.sqlite

SmoViewer.Inspect research-db analyze-class <db> spPartitionRenderable
```

Первый скрипт остаётся независимым read-only аудитом БД. Второй заново читает
исходные directory/PCK bytes, запускает строгий decoder и идемпотентно обновляет
семантические записи.

## Открытые границы

- Runtime-роль `DebugColor` и безопасность его изменения ещё не проверены в
  игре.
- Изменение количества inline-моделей требует catalog-safe перестройки всего
  вложенного поддерева; writer намеренно не включён.
- `spPartitionNode`, octree/BSP и parent spatial graph после первоначального
  отчёта структурно разобраны по SMO; это не полнота executable runtime.

## PC runtime — 2026-09-06

[Native checkpoint](native-pc-partition-runtime.md): base factory4CD950 реально
возвращает `spPCPartitionRenderable9CBB56A2`, exact8C, support+10 и shared identity
matrices760058 после обязательного CRT6D38C0. Actual Model refs/bounds и blank
own clone, Scene88, parent direct-delete подтверждены. Draw ignores matrix
failure, но stops on first Model failure — не контракт RenderNode. Полный
portable class/Visibility/loader остаётся открытым; GPU/игра не запускались.
