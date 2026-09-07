# `spOcclusionVolume` (`0x43D24430`)

Полный структурный read-only разбор выполнен по всем экземплярам трёх корпусов
с повторным чтением исходных PC SMO и PS2 PCK. Контракт независимо подтверждён
serializer-кодом `WinxClub.exe` и `SLES_532.19`.

## Назначение и наследование

`spOcclusionVolume` — размещённая геометрия для visibility occlusion. Класс
наследует `spNode`, поэтому локальные Position и optional Rotation лежат в
первой serializer-секции. Собственная секция хранит два обязательных portable
буфера: triangle-list индексы и position-only вершины.

Все 20 объектов каждого корпуса физически принадлежат корневому `spNode`
`Scene Root`, имеют имена `oclusion_mesh*`/`oclusion_wall*` с исходной опечаткой
и не имеют физических детей. Это самостоятельные occluder-объекты, а не
collision mesh и не части `spMeshData`.

## Serializer layout

```text
# section 0: spNode
field 0  Position       Vector3, обязательно
field 1  Rotation       Quaternion XYZW, optional
field 8  IsAnimated     Boolean = false, обязательно
field 0  terminator

# section 1: spOcclusionVolume
field 0  IndexBuffer    portable triangle index buffer, обязательно
field 1  VertexBuffer   portable position vertex buffer, обязательно
field 0  terminator
```

IndexBuffer:

```text
UInt32 primitiveType   = 2       # triangle list
UInt32 primitiveCount  = T       # число треугольников
UInt32 indexFormat     = 0       # UInt16
UInt16 indices[T * 3]

payloadSize = 12 + 6*T
```

VertexBuffer:

```text
UInt32 declaration     = 0       # position-only, stride 12
UInt32 vertexCount     = V
UInt32 flags           = 0
Vector3 positions[V]

payloadSize = 12 + 12*V
```

Это та же portable геометрическая схема, которая внутри combined field
используется `spMeshBV`, но здесь index и vertex buffer разделены на два поля.
Оба executable writer требуют наличия обоих буферов.

## Корпус

| Corpus | Объекты | Ресурсы | Именованные | Rotation | Размер объекта |
|---|---:|---:|---:|---:|---:|
| `pc-working` | 20 | 8 | 20 | 14 | 120..228 |
| `pc-pristine` | 20 | 8 | 20 | 14 | 120..228 |
| `ps2-pristine` | 20 | 8 | 20 | 14 | 120..228 |
| **Всего** | **60** | **24** | **60** | **42** | — |

Ресурсы находятся в `Challenges/battle_01`, `Challenges/race_02`,
`Gardenia01`, `Gardenia03` и `Swamp/BMS_01`, `BMS_02`, `BMS_03`, `BMS_05`.
Пять raw `field_shape` объясняются только optional Rotation и длинами двух
массивов; отдельных serializer-подтипов они не образуют.

| Вершины V | Треугольники T | Объектов на corpus |
|---:|---:|---:|
| 4 | 2 | 16 |
| 5 | 3 | 1 |
| 6 | 4 | 1 |
| 10 | 8 | 2 |

Для всех форм выполняется `T = V - 2`. Все вершины используются, треугольники
не вырождены, winding согласован, mesh связен, каждое граничное ребро встречается
один раз, внутреннее — дважды. Получается один плоский триангулированный диск без
внутренних вершин. Максимальное отклонение от плоскости —
`0.000650151841`, площадь локальной поверхности —
`767985.460..68909502.634`.

## Важный случай выпуклости

Оба executable содержат проверки `mesh is not convex`, `border is not convex`
и сообщение, что mesh должен быть либо замкнутым объёмом, либо плоским. Но
штатный корпус показывает более точную картину:

- 18/20 объектов каждого корпуса строго выпуклые;
- `oclusion_mesh01` в `battle_01.smo` и его точная копия в `Gardenia03.smo`
  являются одним десятиугольником с небольшой вогнутостью;
- средняя вершина цепочки 3–4–5 отклоняется внутрь примерно на `11.832` единицы;
- этот же payload без изменений присутствует на PC и PS2.

Итого строго выпуклы 54/60 экземпляров, а остальные шесть — повтор одного
authored decagon. Прежний вывод, что наличие этих данных автоматически доказывает
их успешную runtime-инициализацию, **отозван в PC checkpoint15**: reader44F400
вызывает Init470FE0 и обрабатываетfalse. Допуск0.001f некоторых shape helpers
уже найден, но его достаточность для этого decagon пока не проверена целиком.
Наличие данных, успешная загрузка и включение в culling — разные утверждения.
Viewer не должен молча «исправлять» форму до convex hull.

Замкнутых объёмов в корпусе нет, хотя обе реализации executable явно допускают
их. Поэтому это подтверждённая возможность формата, но пока не наблюдаемый
вариант данных.

## Свидетельства из executable

PC:

- reader `0x0044F400..0x0044F682`;
- writer `0x0044F690..0x0044F9B3`;
- VertexBuffer pointer `+0xC8`, IndexBuffer pointer `+0xD0`;
- generic IndexBuffer writer `0x0045FC90..0x0045FD69`;
- generic VertexBuffer writer `0x00460400..0x004604C3`.

PS2:

- reader `0x001A00B0..0x001A03F8`;
- writer `0x001A0410..0x001A0630`;
- VertexBuffer pointer `+0xD0`, IndexBuffer pointer `+0xD8`;
- generic IndexBuffer writer `0x00159200..0x00159324`;
- generic VertexBuffer writer `0x0015C4C0..0x0015C5B0`.

Имена собственных полей подтверждены строками
`esfOcclusionVolumeIndexBuffer` и `esfOcclusionVolumeVertexBuffer`. Различаются
только runtime offsets; serialized layout общий.

## PC/PS2

По canonical resource path и ordinal сопоставлены все 20 объектов каждой пары:

- `pc-working` / `pc-pristine`: 20/20 совпадают семантически и побайтно;
- `pc-pristine` / `ps2-pristine`: 20/20 совпадают семантически и побайтно;
- совпадают имена, node transforms, заголовки буферов, индексы и все координаты.

Отдельная platform-ветка decoder не нужна.

## Viewer и исследовательская база

`SmoOcclusionVolumeDecoder` строго читает обе секции, проверяет размеры буферов,
конечность координат, границы индексов, использование всех вершин и отсутствие
вырожденных треугольников. Inspector показывает Position/Rotation/Animated,
число треугольников, UInt16 index format, число вершин и bounds. Геометрия
остаётся read-only.

Analyzer записал:

- 11 field definitions: девять унаследованных `spNode` и два собственных поля;
- 282 annotations присутствующих полей;
- один общий PC/PS2 variant и 60 assignments;
- четыре evidence-записи по двум executable, корпусу и cross-corpus сравнению.

Optional Rotation и разные V/T — параметры одного класса, а не подтипы.

## Воспроизведение

```powershell
dotnet tools/SmoViewer/SmoViewer.Inspect/bin/Release/net8.0/SmoViewer.Inspect.dll `
  research-db analyze-class local-data/results/smo-corpus-v2.sqlite `
  spOcclusionVolume --json

python research/analyze_smo_occlusion_volume.py `
  local-data/results/smo-corpus-v2.sqlite
```

Связанный `spMeshNavigationSet` имеет структурный wire-разбор; это не означает
полную runtime-реконструкцию navigation. [PC checkpoint15](native-pc-occlusion-runtime.md)
добавляет exact1B8 ABI, original class TU, native lifetime/world/weld/planes и
whole Scene consumer на явно подготовленном силуэте. Full protected Init и
фактическая загрузка authored decagon остаются открытыми.
