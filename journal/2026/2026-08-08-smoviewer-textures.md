# 2026-08-08 — текстуры, UV и цвета вершин в SmoViewer

## Вопрос

Нужно восстановить текстурированный предпросмотр PC-файлов SMO без глобального
поиска сигнатур и без проверки всего игрового корпуса. Основными эталонами стали
файлы из корня `local-data` и несколько моделей из
`local-data/pc-pristine/Media/Characters`.

## Корпус и исполняемый файл

- pristine PC-корпус: `local-data/pc-pristine`;
- рабочие точечные образцы: файлы `.smo` в корне `local-data`;
- готовая реализация движка для будущей сверки: `local-data/WinxClub.exe`;
- исполняемый файл на этом этапе не изменялся и для texture/UV decode не
  потребовался: необходимые offsets подтвердились непосредственно корпусом;
- массовый scan всех SMO не выполнялся. Использовались отдельные файлы и
  воспроизводимые выборки по 10 файлов.

## Подтверждённый texture decode

`spTextureData` имеет class ID `0x78EA082B`. Поддержаны три PC-варианта:

| Format | Layout | Width | Height | Pixels |
|---:|---|---:|---:|---:|
| `0x32E3` | ABGR | `+0x24` | `+0x28` | `+0x3C` |
| `0x43E3` | ABGR | `+0x24` | `+0x28` | `+0x3C` |
| `0x29E3` | BGRA | `+0x28` | `+0x30` | `+0x34` |

Декодер ограничивает чтение точным interval записи каталога, проверяет размеры
вложенных data blocks и нормализует результат в BGRA32.

## Подтверждённые vertex layouts

| Format | Serialized stride | Runtime stride (наблюдался) | Diffuse ARGB | UV0 | Пример |
|---:|---:|---:|---:|---:|---|
| `0x0900` | 24 | 24 | `+12` | `+16` | `Bloom_body.smo` |
| `0x093E` | 44 | 56 | `+32` | `+36` | `fish.smo`, `butterfly.smo` |
| `0x0940` | 36 | 36 | `+24` | `+28` | `loading.smo`, колесо Droid |
| `0x097E` | 56 | 68 | не применяется | `+48` | skinned character meshes |
| `0x197E` | 64 | 76 | не применяется | `+48` | skinned Bloom meshes |

У `0x197E` после UV0 присутствует второй восьмибайтовый UV-канал по `+56`, но
viewer пока использует только UV0. Значения по `+44` у `0x097E/0x197E`
наблюдались как ARGB-подобные, однако их семантика для рендера не подтверждена;
модуляция ими отключена.

## Texture binding

Первоначальное правило «один sibling mesh + одна texture» было недостаточным.
Character-файлы сериализуют материал только рядом с первой mesh-частью, а
остальные `spSkin`-части того же `spRenderNode` переиспользуют его.

Текущий порядок разрешения:

1. точная interval-связь owner → material subtree → texture;
2. переиспользование основной (наибольшей) texture внутри того же render node;
3. переиспользование для нумерованного семейства render nodes, например
   `Goopeye01/02/03`;
4. основной atlas модели выбирается как наибольшая подтверждённая texture и
   применяется к отдельным частям вроде `Legs` или `Object01`.

Выбор первой texture был ошибочным: у `Grizelda` и `Griffin` первой шла маленькая
текстура глаз. Выбор наибольшего atlas исправил руки, юбку и остальные части.

## Подтверждённые bindings на эталонах

- `bloom_school.smo`: 6 meshes; 5 × `bloom_gilet`, 1 × `bloomeye`;
- `bloom_silk.smo`: 5 × `bloom_lotus`, 1 × `bloomeye`;
- `Darcy.smo`: 6 × `darcy`;
- `Goopmonster.smo`: 6 × `gooptex`, 3 × `gooptex2`;
- `Generic.smo`: 7 × `spe_body`, 1 × `spe_g_ey`, 1 × `spe_g_he`;
- `Grizelda.smo`: 6 × `grizelda`, 1 × `grizel_e`, 1 × `grizel_g`;
- `Griffin.smo`: 6 × `griffin`, 1 × `griffi_e`;
- `fish.smo`: 1 × `fish`;
- `butterfly.smo`: 1 × `butter_g`;
- старый `Bloom_body.smo`: 6 × `bloom_jeans`, 2 × `bloomeye`.

## Primitive type 2

`Droid.smo` подтвердил, что primitive type `2` — triangle list. В E1 поле после
primitive type хранит количество треугольников, а размер индексов равен:

```text
triangleCount * 3 * sizeof(UInt16)
```

Наблюдения:

- 44 треугольника → 264 байта;
- 536 треугольников → 3216 байт.

После поддержки type `2` в `Droid.smo` декодируются 4/4 meshes вместо 2/4.
`trail_mesh` является служебной геометрией эффекта меча. Назначение ему `xj5`
создавало большой растянутый прямоугольник, поэтому обычный viewer его пропускает.

## Vertex diffuse modulation

Для `butterfly.smo` подтверждены меняющиеся vertex diffuse colors формата
`0x093E`. Реализован preview-путь: цвета интерполируются по UV-треугольникам во
временную tint-карту, после чего исходная texture умножается на tint один раз.
Это диагностический rasterized preview; при пересекающихся UV единственно точный
результат требует настоящего per-vertex GPU-рендера.

## Обнаруженная регрессия

Первая реализация tint preview применялась ко всем layouts с полем diffuse,
включая `0x0940`. Это привело к двум симптомам:

- колесо `Droid.smo` стало чёрным;
- `Alfea01.smo` и `Alfea02.smo` визуально зависали при открытии, потому что CPU
  растеризовал vertex colors для большого числа level meshes и крупных textures.

Следующее исправление должно ограничить текущий tint preview подтверждённым
форматом `0x093E`. Цвета `0x0940` требуют отдельного, производительного render
path или явного opt-in после проверки material semantics.

## Что не закрыто

- transform hierarchy узлов и skin pose; у Droid колесо может быть не закреплено
  к основной модели, хотя geometry/texture decode уже корректен;
- точная семантика material states, UV transforms и vertex diffuse для всех FVF;
- второй UV-канал `0x197E`;
- прозрачные effect materials и корректный рендер `trail_mesh`;
- GPU-путь для точной интерполяции vertex colors без rasterized atlas preview.
# Дополнение: материалы составных уровней

Уровни нельзя оценивать только по числу texture bindings. В `Alfea02.smo` находятся
87 встроенных `spTextureData`, которые дают 82 renderable textured meshes, но большая
часть оставшихся объектов по формату является не текстурированной, а окрашенной через
`spMaterialData`. Голубой цвет на них был диагностическим fallback самого viewer.

В `spMaterialData` поле type `2` с payload 20 байт соответствует `esfMaterialColor`:
четыре packed ARGB значения (ambient, diffuse, specular, emissive) и material power.
Diffuse читается из второго DWORD. Материал сопоставляется с mesh по подтверждённому
общему owner interval и тому же порядку material/mesh, который применяется для явных
texture bindings. Если texture существует, она имеет приоритет над сплошным цветом.

Результаты на контрольных уровнях:

| Файл | Textured meshes | Meshes с material color |
|---|---:|---:|
| `Alfea02.smo` | 82 | 680 |
| `Alfea01.smo` | 61 | 501 |
| `BMS_05.smo` | 15 | 69 |
| `cloud01_02.smo` | 48 | 256 |
| `Alfea_night_01.smo` | 50 | 538 |

Для `Alfea02` это заменяет голубой fallback настоящим материалом почти на всей сцене,
не распространяя случайный texture atlas на несвязанные предметы.

## Дополнение: vertex diffuse в level meshes

Серый material diffuse не является окончательным цветом уровня: в `Alfea02.smo` 700
из 702 мешей содержат vertex diffuse, отличный от сплошного белого. Он используется
как локальный цвет/освещение и должен модулировать материал.

Подтверждены дополнительные layout:

| Format | Stride | Diffuse ARGB | UV0 |
|---:|---:|---:|---:|
| `0x0100` | 16 | `+12` | нет |
| `0x1940` | 44 | `+24` | `+28` |

`0x1940` совместим с началом `0x0940` и добавляет данные после первого UV-канала.
Vertex-color modulation теперь разрешена для `0x0900`, `0x093E`, `0x0940` и `0x1940`.
Если у mesh есть настоящая текстура, diffuse умножается на её RGB. Если texture object
отсутствует, цвет интерполируется в компактную 64×64 служебную карту по UV0. Это сохраняет
внутримешевые градиенты, которые невозможно представить одним WPF `DiffuseMaterial`, и
ограничивает память на сотнях level meshes.
