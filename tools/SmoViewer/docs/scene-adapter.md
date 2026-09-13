# Сценовый адаптер SmoViewer

## Прямой GPU-preview уровней

Исследованный DX material path связывает `ColorOperation = 3` с
`D3DTOP_MODULATE`, а FVF `0x940` содержит `D3DFVF_DIFFUSE`. Поэтому достоверный
основной путь не должен сначала растрировать vertex colors обратно в texture:
игра передаёт texture, UV и diffuse stream видеокарте, которая интерполирует
diffuse по треугольнику и умножает его на texture sample.

Viewer воспроизводит эту схему для всех декодированных mesh через OpenGL 3.3:

- один VAO/VBO/EBO создаётся на физический `spMeshData`;
- reference-only level placements переиспользуют эти buffers с отдельными model
  matrices;
- исходные UV поступают в repeat sampler с linear mipmap filtering без
  дополнительного `1-V`: первая загруженная строка SMO соответствует `V=0`;
- shader вычисляет `texture × interpolated vertex diffuse`, включая vertex
  alpha там, где анализ material/consumer подтверждает его использование;
- skinned geometry передаёт четыре веса/индекса и palette до 32 matrices в vertex
  shader; SAN playback обновляет palette без перестройки geometry на CPU;
- rigid mesh под анимируемым `spRenderNode` обновляет model matrix из SAN-трека;
- texture sequences выбирают GPU texture frame по записанной длительности, а
  подтверждённые layered materials семплируют base по UV0 и effect по UV1;
- GUI-контент использует orthographic projection той же OpenGL-сцены;
- opaque и transparent placements используют общий depth buffer, прозрачные
  экземпляры сортируются от камеры; сетка пола участвует в том же depth test.

CPU triangle-atlas в видимом OpenGL path не создаётся. Невидимая WPF geometry
используется только для выбора мышью; обычная 3D-модель требует OpenGL context.

На одной локальной debug-сборке pristine `Alfea03.smo` дал 509 unique mesh
buffers, 63 unique textures и 1 263 placements. Подготовка сцены сократилась с
32,17 с при CPU-atlas path до 0,20 с, GPU upload занял 0,085 с, working set —
около 281 МиБ вместо 934 МиБ. Это контрольный замер реализации, а не
гарантированная производительность на другом оборудовании.

Rigid-меши могут намеренно назначать разные vertex-diffuse RGB вершинам с
одинаковыми UV. Обратная запеканка tint в одну исходную texture тогда теряет
информацию: цвета перекрывающихся граней усредняются. OpenGL path передаёт
diffuse каждой вершины отдельно, поэтому private triangle-atlas не нужен. Это
сохраняет vertex-only цвет рамы `[681]` и сочетание
`noise04w` с голубыми, фиолетовыми, жёлтыми и зелёными деталями `[689]` в
`Alfea02.smo`; исходные SMO-данные при этом не изменяются.

Vertex alpha не всегда можно отбросить по `FinalBlendOp`: материал без bitmap
также обязан сохранять полный render-state tuple. `[2929]` (`DVlight04`)
имеет одинаковый pale-yellow vertex RGB, но меняющуюся alpha `0x00/0xCC`.
Такая комбинация является authored alpha-gradient, даже если `FinalBlendOp=2`
без контекста классифицируется как `OpaqueFinalBlend2`. Прямой GPU-path передаёт
градиент как vertex alpha. Mesh рисуется после непрозрачной геометрии. Поэтому сам
световой меш остаётся прозрачным, а стена за ним уже находится в depth buffer и
остаётся видимой.

Аналогичный контекст встречается у плоского chandelier glow `[2863]`. Его четыре
пересекающихся quad используют целочисленные repeated UV tiles в диапазоне
`-2..3`. Texture `chand` содержит 2 412 полностью прозрачных, 743 partial-alpha и
941 opaque texel. UV alpha analyzer сдвигает каждый треугольник в его единичный
тайл, не теряя данные из-за ненормализованного общего UV range. Сочетание
rigid consumer, `FinalBlendOp=2`, companion state 2 и partial texture alpha классифицируется
как `RigidTextureAlphaSurfaceFinalBlend2`. Glow рисуется после непрозрачного
chandelier body `[2859]` и не портит его depth/smoothing. У `[2859]` все normals единичной
длины; все 222 группы duplicate positions имеют совпадающие normals, то есть
сглаживание в самом mesh не потеряно.

`Alfea_broken_01.smo` подтверждает textureless вариант authored vertex light.
Mesh `[1022]` принадлежит `Ray_light_F04-000`, содержит один quad, два RGB
(`0xFFFFFF` и `0xFFF3A0`) и alpha `0/0x65`; bitmap texture в material
действительно отсутствует. Его material использует `FinalBlendOp=2`, companion
state `2` и tuple `[0,0,1,0,1,0,3,0,2,0,6]`. Поэтому Viewer передаёт RGB/alpha
напрямую в GPU и помещает quad в прозрачный проход после стены. То же правило
подтверждено на `[1005] Ray_light_D01` и `[1018] Ray_light_E03`.

Смешанные RGB с любыми не-255 alpha сами по себе недостаточны: baked floors
этого уровня содержат небольшие отклонения `236..255` при `FinalBlendOp=0` и
остаются opaque. Для mixed-RGB light требуются одновременно rigid mesh,
`FinalBlendOp=2`, companion state `2` и переход vertex alpha от нуля к видимому
значению. Прежнее правило uniform RGB продолжает покрывать `DVlight04`.

Mesh `[1078] poutreW06-000` не является textureless: material `[1077]` ссылается
на общую непрозрачную `marble2 [43]` 64×64. Она намеренно очень светлая
(`RGB 221..252`) и модулируется 144 authored vertex colors. Контрастный рисунок
этой составной балки хранится отдельно в mesh `[1082] poutreW06-001` с texture
`linegen00 [1065]`. Выбор одного физического mesh не должен трактоваться как
описание всех material passes соседнего составного объекта.

Uniform vertex RGB у rigid level-mesh также может быть авторским texture tint,
а не неиспользуемым placeholder. `[993]` из `Alfea01.smo` (`TreeB_04-001`)
использует texture `leaf01` и один цвет всех 147 вершин `0xFF3EAB00`.
Исходная texture является серой alpha-mask; без модуляции меш выглядит
белым и создаёт впечатление отсутствующей texture. Viewer применяет uniform
RGB ко всей private texture copy только для non-skinned level layouts; защита от
uniform character placeholders сохраняется.

`[381] H_DoorB07_0_745627.644531` из `Alfea01.smo` использует другой вариант
той же идеи. Его `noise03b` — почти белая непрозрачная шумовая texture 64×64, а
рисунок двери задают 199 authored vertex RGB на 2 126 вершинах. Mesh содержит
1 432 треугольника и намеренно использует одинаковые UV с разными цветами.
Запекание всех цветов обратно в одну копию `noise03b` усредняет несовместимые
участки и создаёт ложную «странную texture». GPU-path не запекает цвета: он
передаёт исходный diffuse stream вместе с UV.

`[3785] centerfloorR-000` восстанавливает отдельный приём имитации натёртой
плитки. Это плоский mesh на `Y=0` с шестью authored vertex RGB и одинаковой
alpha `0xD8` у всех 44 вершин. Его `spModel [3784]` не дублирует material внутри
своего interval: top-level field type `0`, payload `{2603, 0}` ссылается по
object ID на `[2602] spMaterialData`, содержащий `[2603] floor02`. Под плиткой
сериализована отдельная `RF`-геометрия: например, `[3804]
centerhallRRF-001` занимает тот же диапазон пола и уходит по Y примерно до
`-35.69`. Игра показывает нижний дубликат через частично прозрачную `floor02`
вместо настоящего динамического отражения.

Ранее Viewer не разрешал material reference из level-`spModel`, поэтому считал
`[3785]` textureless, создавал служебную RGB-карту и затем выставлял её alpha в
`255`. Теперь texture `floor02` умножается на vertex RGB и равномерную alpha
`0xD8`, а поверхность переносится в прозрачный проход после нижней непрозрачной
геометрии. Нулевой alpha не принимается за это правило; uniform partial alpha
сохраняется только у rigid mesh. Исходные SMO-данные не изменяются.

Тот же compact material-reference convention объясняет «размазанное»
отображение `[3795] centerhallL01-001`. `spModel [3794]` содержит ссылку object
ID `2163` на `[2162] spMaterialData`, внутри которого лежит `[2163] caro00`
64×64. У самого model есть только `[3795] spMeshData`, поэтому прежний resolver
не находил texture и пытался изобразить поверхность одной служебной картой из 18
vertex colors. Теперь `caro00` разрешается через shared material, а конфликтующие
UV/vertex RGB сохраняются в private triangle-atlas. В pristine уровнях этим
точным способом переиспользуются 50 материалов в `Alfea01.smo`, 17 в
`Alfea02.smo` и 19 в `Alfea03.smo`.

У указанного в UI элемента `[2756]` видимая геометрия находится в соседнем
`[2757] spMeshData` под `Garch_B01-000`; `[2756]` является его
`spMaterialData`. Надпись/орнамент собрана 30 треугольниками с 23 authored vertex
colors и texture `noise03b` 64×64. Все треугольники используют одни четыре UV,
причём площадь каждого UV-треугольника равна `0.39726`, то есть он адресует почти
80% texture tile.

Private triangle-atlas ранее безусловно ограничивал ячейку 32×32. В результате
каждый треугольник сначала терял половину линейного разрешения `noise03b`, после
чего WPF растягивал его на крупную геометрию и надпись выглядела размытой.
Универсальный расчёт по максимальному UV-размаху и размеру `noise03b` требует 64
texel-интервала; с крайним отсчётом и padding получается ячейка 69×69 и atlas
414×345. Это следствие данных mesh, а не отдельное правило для надписи: тот же
расчёт применяется ко всем textured mesh, которым требуется triangle-atlas.

## Skeleton и skin bind pose

Вложенность интервалов каталога описывает serializer ownership, но не всегда
логическую иерархию костей. Настоящее ребро `esfNodeChild` (field type `5`)
сериализуется как:

```text
UInt32 childObjectId
UInt32 inlineSerializedSize
Byte   inlineObject[inlineSerializedSize]
```

Нулевой `inlineSerializedSize` означает ссылку на уже сериализованный объект.
Поэтому skeleton tree должен строиться по object ID, а не по именам или только
по `ParentIndex` интервалов.

Последняя секция `spSkinSerializer` содержит аппаратную палитру из 16 костей:

```text
UInt32 reserved = 0
UInt32 boneCount = 16
repeat boneCount:
    UInt32 boneObjectId
    UInt32 inlineNodeSize
    Byte   inlineNode[inlineNodeSize]
    Single inverseBindMatrix[16]
```

Матрица хранится row-major для row-vector convention. Её инверсия даёт
bind-world соответствующей кости. Одна кость может повторяться в нескольких
палитрах; проверенные копии inverse-bind совпадают. Жёсткий attachment под
костью размещается как `attachmentLocal * boneBindWorld`. Это правильно ставит
глаза и предметы в руках в обеих dating-моделях без коррекции координат.

В skinned vertex formats `0x097E` и `0x197E` подтверждены четыре `Single`-веса
по `+12`, четыре raw bone-index bytes по `+28`, diffuse ARGB по `+44`; UV0
остаётся по `+48`, normal XYZ находится по `+32`.
Активные индексы адресуют локальную 16-костную палитру `spSkin`, не глобальный
список узлов. Неиспользуемые index bytes не обязаны быть нулевыми и должны
игнорироваться при нулевом соответствующем весе.

Diffuse ARGB модулирует texture color и должен применяться также к skinned
layouts. В `bloom_dating_outfit_03.smo` меши лица `116` и `118` содержат
тёмные вершины ресниц `0xFF0D132E`; без vertex modulation ресницы ошибочно
показывают исходный цвет atlas лица.

Сохранённые normals декодируются без подмены. Текущий основной OpenGL shader
является unlit и не использует их для недоказанной модели игрового освещения.
Нулевые normals в отдельных level meshes являются допустимыми
заглушками и сохраняются как нулевые вместо отклонения всего mesh.

## Плоская GUI-сцена igmenu_opt_pc.smo

Чистый PC-ресурс `Media/Menus/igmenu_opt_pc.smo` имеет SHA-256
`D6ED2606BFCA4C4EED41F59869D1EC1C20DFAFE7A3BFD8E7FEC6C5F109052F0E`, содержит
1 225 объектов и 99 из 99 строго декодированных mesh. После накопления node-
transform все 99 mesh остаются плоскими; глубина используется для порядка слоёв,
а не как объёмная геометрия.

Корень `Scene Root` содержит отдельные экранные/шаблонные ветви:

| Ветка | Объекты | Узлы | Mesh |
|---|---:|---:|---:|
| `save_load_quit` | 66 | 46 | 3 |
| `R1_button` | 27 | 7 | 4 |
| `L1_button` | 26 | 7 | 4 |
| `settings` | 322 | 97 | 50 |
| `template_midsmall` | 5 | 5 | 0 |
| `title` | 5 | 5 | 0 |
| `template_mid` | 5 | 5 | 0 |
| `controls` | 354 | 326 | 4 |
| `options` | 73 | 51 | 3 |
| `language` | 73 | 14 | 13 |
| `display` | 132 | 37 | 13 |
| `default` | 136 | 91 | 5 |

Один файл, таким образом, является не готовым кадром, а библиотекой нескольких
панелей и состояний. Имена предков `NORMAL*`, `HIGHLIGHTED*`, `PUSHED*` и
`DISABLED*` задают альтернативные визуальные ветви контролов; `shadow*` является
общим дополнительным слоем. В файле найдено 60 mesh под state-ветвями и два mesh
под `GUICollision*`. Последние можно показывать как интерактивные области, но
связь с runtime mouse hit-testing ещё не подтверждена.

Ветка `display` содержит якоря `resolution`, `value_resolution`,
`resolution_label` и scroll-контролы. При этом в объектном графе нет ни одного
`spTextNode`, `spTextRenderable` или `spFont`: подписи и текущие значения должны
поступать из runtime-логики. Шесть встроенных `spTextureData` — `flora_options`,
`icons`, `ps2_buttons`, `bloom_start`, `bloom_opt_def` и `musa_options` — дают
изобразительную часть меню.

`SmoGuiSceneAnalyzer` распознаёт такой ресурс по совокупности признаков, а не по
пути или имени файла: строгий decode всех mesh, не менее 80% плоских mesh,
отсутствие skin и наличие state-/collision-семантики в иерархии. Viewer после
этого включает отдельный ортографический режим с фильтрацией экранов и состояний.

## Node-only 2D-layout gameover.smo

Чистый `Media/Menus/gameover.smo` имеет размер 395 байт и SHA-256
`593DDE72EAFC36532B4976B5269EB53D0B3FAC217AB9B97472C6B8C1DBEBE2AA`. В нём
ровно шесть объектов, и все они являются `spNode`:

```text
Scene Root
└─ gameover
   ├─ shadow     P=( 0.20526648, -0.23411977,  0.14643508)
   │  └─ text_text01
   └─ NORMAL     P=(-0.20526642,  0.23411977, -0.14643507)
      └─ text_text
```

`text_text01` и `text_text` не содержат собственных transform и наследуют
позиции соответственно от `shadow` и `NORMAL`. В ресурсе нет `spMeshData`,
`spMaterialData`, `spTextureData`, `spTextNode`, `spTextRenderable` и `spFont`.
Следовательно, SMO хранит только layout двух runtime-слотов; фактическая строка
«Game Over», шрифт, размеры и создание renderable находятся вне этого файла.

Структурный тип `RuntimeNodeLayout` требует одновременно: отсутствие mesh,
не менее четырёх объектов, только `spNode`, листовые `text_*`, состояние
`NORMAL`/`shadow` и хотя бы один декодированный transform. Viewer строит world-
позиции листьев по цепочке `ParentIndex` и показывает диагностические карточки.
Это визуализация anchors и state, а не реконструкция отсутствующего текста.

## Граница world-transform в partitioned Alfea02.smo

Чистый `Media/Levels/Alfea/Alfea02.smo` имеет SHA-256
`1316A81D27254E1B20627433CC8E01327041D560BBEFACB949ADF38A336B79DF`, 4 266
объектов и 702 строго декодированных mesh. Объект `[4199]` — не mesh, а
`spModel dormBigroomNOSH-000`; его геометрия находится в `[4201] spMeshData`.

Object-directory containment вокруг partition имеет вид:

```text
PartitionSystem
└─ sector01
   └─ portal01_BackToFront
      └─ sector02
         └─ portal02_BackToFront
            └─ sector03
               └─ portal04_BackToFront
                  └─ sector04
                     └─ spStaticRenderObject Darch_A01
```

Это вложение сериализованных интервалов, а не пространственная иерархия.
Предыдущий resolver ошибочно складывал positions `sector01...sector04` с уже
готовой world-матрицей `Darch_A01`. Её translation
`(-4416.12744, 0, -1812.98193)` превращалась в
`(-22283.875, 603.69574, -9635.5752)`.

Поля `sector*`, которые строгий field decoder технически читает как
position/rotation/scale, используются partition/culling-структурой. Baked-
геометрия уже содержит мировые coordinates: `[4201]` имеет bounds
`X=-5552.13...-3870.64`, а объединённый центр геометрии каждого сектора близок к
его serialized position. Добавление position повторно разносит стены по сцене.

Подтверждённое правило resolver:

1. Local transforms применяются только к известным `spNode`, `spRenderNode` и
   `spModel`.
2. `spStaticRenderObject.Transform` применяется как готовая world-матрица и
   завершает цепочку.
3. Неизвестные partition/portal classes не становятся transform owners только
   из-за совпавших field types или каталожного containment.
4. Mesh под sector geometry group `0x94BBCA2A` без подтверждённого node/static
   placement сохраняет baked vertex coordinates.

После исправления `[4201]` получает identity world matrix, а `[1372]` под
`Darch_A01` — в точности authored static world matrix без sector offsets.

## Shared mesh instances в Alfea03.smo

Подтверждённая форма такого размещения:

```text
spStaticRenderObject <имя экземпляра>  -- authored world matrix
└─ spModel
   ├─ spMaterialData
   └─ финальная секция spModel, field type 0, payload size 8:
      UInt32 sourceObjectId, UInt32 zero
```

У ссылочного `spModel` нет физического дочернего `spMeshData`. Первый `UInt32`
однозначно разрешается через object ID в существующую запись `spMeshData`, второй
равен нулю. Resolver использует строгий `SmoModelDecoder`, принимает не-inline
Base mesh только при совпадении target class с `spMeshData`, единственном объекте
с таким ID и наличии владеющего `spStaticRenderObject`; унаследованный material
field 0 с ним больше не смешивается.

Для `[2674] spMeshData casierD16` физическое размещение находится в обычной ветви
с embedded mesh, а ещё 21 `spModel` ссылается на object ID `2675`. Каждый из них
имеет собственную authored world-матрицу; вместе Viewer показывает 22 размещения
шкафа вдоль стен. На всём уровне получается 510 физических mesh и 757 ссылочных
экземпляров, то есть 1 267 отображаемых поверхностей.

SmoExporter 0.5.0 отделяет уникальные `spMeshData` от размещений. В GLB несколько
nodes ссылаются на один `meshes[]`, в FBX несколько `FbxNode` используют общий
`FbxMesh`; отдельный режим осознанно запекает геометрию каждого размещения. OBJ
не имеет стандартной instance-семантики, поэтому разворачивает ссылки и выдаёт
явное предупреждение. Молчаливое удаление или неоговорённое дублирование
экземпляров недопустимо.

## Вспомогательные объекты bloom_jeans

Проверка object graph `bloom_jeans.smo` дала следующую рабочую классификацию:

- `[85] collision_volume_run`, `[88] collision_volume_special` и
  `[92] collision_volume_root` — узлы collision volumes. Под каждым находится
  `spCollisionInfo`, затем подтверждённый `spOBBBV` класса `0x4DA04889`.
  Собственный field 1 `spOBBBV` является полным размером ориентированного box.
- `[91] movement_tracker` — position-only служебный tracker.
- `[95] SubMaster` — корень control/IK rig с `IK-Leg*`, `IK-Hand*`,
  `UP-Knee*`, `UP-Elbow*`, `C-Hand*`, `C-Foot*` и другими control nodes.
  Это не skin palette и не дополнительный деформирующий skeleton.
- `[119] BLOOM` — именованный position-only служебный marker.
- `[120] Ambient01`, class `0x5E6402DF`, — подтверждённый `spLightData`.
  Его собственная секция содержит тип, цвет, attenuation, range, hotspot,
  falloff и enabled; transform/геометрии у объекта нет. Viewer показывает
  восстановленные имена полей информационно, без разрешения на запись.

Viewer не присваивает неизвестным hash вымышленные имена: знак `?` в дереве
явно обозначает рабочую, а не окончательную классификацию.

В панели «Ресурсы» флажок «Показывать восстановленные поля» включает read-only
инспектор собственной, последней serializer-секции выбранного объекта. Он
показывает semantic key, значение, заявленный layout, абсолютный payload offset
и hex-preview. Секции базовых классов не получают подписи производного класса,
а payload с несовпавшим или не до конца подтверждённым layout остаётся hex-only.

Для `spCollisionInfo` инспектор показывает все три подтверждённых поля:
Primitive relationship с разрешённым `spMeshBV`/`spOBBBV`/`spBoxBV`/`spSphereBV`
target, `UInt32` Collision group и 40-байтовый world transform как position,
quaternion XYZW и scale. Строгий decoder принимает три наблюдаемые формы:
Primitive; Primitive+Group; Primitive+Group+Transform. Опущенные optional-поля
не материализуются и не получают предполагаемых runtime-default.

Для `spMeshBV` строгий decoder принимает обязательный field 0 с version-2
triangle list (`UInt16` indices, `Vector3` positions) и необязательный field 1.
Второй payload начинается с class ID `wxFaceData = 0x313C4C17` и содержит ровно
по одной разреженной записи на треугольник: field 1 `UInt8 surface type`, field 2
`UInt16 flags`, field 3 `UInt8 surface ID`, затем field-zero terminator записи.
Нулевые члены опускаются. Inspector показывает bounds/degenerate count и
гистограммы metadata; surface type 1..9 подписываются как stone, dirt, grass,
water, snow, swamp, mud, deepwater и carpet. Flags и surface ID остаются
числовыми, пока их runtime-семантика не восстановлена.

Для `spPartitionNode` строгий decoder принимает точный writer order:
`DebugColor` (1), `PartitionSystem` (5), `Zone` (3), повторяемые `Child` (2),
`CollisionInfo` (0), `ZonePortal` (4), `StaticRenderObject` (7), затем ровно один
nullable `PartitionRenderable` (6) и пустой field 0 terminator. Inspector
показывает ARGB и каждую resolved relationship. Он также различает sized
reference, inline physical child и ID-only null. Field 2 сохранён в registry,
поскольку реализован PC и PS2 serializer. У 16 204 объектов точного класса он
пуст, но каждая унаследованная секция `spOctreeNode` содержит восемь записей:
`UInt32 octantSlot` плюс inline relationship на `spPartitionNode` или
`spOctreeNode`.

Для `spOctreeNode` строгий decoder сначала читает эту базовую секцию, требует
slots 0..7 и отсутствие leaf-списков, затем читает собственные fields 0/1/2 как
`Vector3 Pivot/Mins/Maxs`. Inspector показывает все 15 содержательных полей.
Bits 0/1/2 slot выбирают high-половину parent bounds по X/Y/Z; это подтверждено
на всех 2 222 вложенных octree-узлах. PC и PS2 используют одинаковый serialized
layout, хотя runtime offsets различаются.

Для `spMeshNavigationSet` decoder читает три секции: `spNode`, базовый
`spNavigationSet` и собственный mesh relationship. Базовая секция требует field
0 `UInt32 NodeCount`, field 1 `N x N` node matrix, field 2 `P x N` portal matrix,
field 3 adjacency (`UInt32 N`; для каждого узла `UInt8 id`, `UInt32 count`,
`UInt8 neighbours[count]`), repeated field 4 portals и field 5 Boolean Enabled.
Собственный field 0 разрешается как один `spMeshBV`. Обычная matrix cell — индекс
в ordered neighbours текущего узла. Out-of-degree значение 3 завершает маршрут;
на диагонали и у недостижимых пар node matrix также записано 3, а при degree 4
оно может быть обычным индексом. Decoder проверяет dimensions, dense IDs,
relationship targets, `NodeCount == mesh triangleCount`, reciprocity и полное
достижение node/portal routes без циклов. Inspector показывает все 3 254
содержательных поля 355 PC/PS2-объектов.

Для `spZone` decoder читает две секции: унаследованный `spNode` с обязательными
Position и `Animated=false`, optional Rotation/Static и собственный повторяемый
field 0 `LocalPartitionRoot`. Наблюдаемая cardinality — 0..4; все ненулевые
отношения являются owned inline `spPartitionNode` либо `spOctreeNode`. Inspector
показывает все 1 231 содержательное поле 369 PC/PS2-зон.

Для `spZonePortalNode` decoder читает унаследованную секцию `spNode`, затем
строго требует два ненулевых различных sized reference field 0 на
`spZonePortal` и второй terminator. Inspector показывает Position,
optional Rotation/Static, обязательный `Animated=false` и обе ссылки в исходном
порядке `BackToFront`, затем `FrontToBack`. Analyzer аннотировал 1 305 полей 309
PC/PS2-объектов; Position является независимым placement-полем, а polygon
остаётся в связанных `spZonePortal`.

## Соседние форматы

### SAN animation resource

PC SAN является FFPS-контейнером с одним объектом `spAnimation`
(class `0x56EE563A`). После
`SBOO` сериализуются:

```text
field 0: Single durationSeconds
repeat track:
    field 2: position curve
    field 3: rotation curve
    field 4: scale curve
    field 1: UInt16 byteLength + Latin-1 nodeName\0
field 0, empty: terminator
```

Curve payload начинается с двух `UInt32`: подтверждённого значения `1` и
`keyCount`. Затем идут `keyCount` значений времени `Single`, после них — Vector3
для position/scale либо quaternion XYZW для rotation. Размеры payload поэтому
равны `8 + keyCount*16` и `8 + keyCount*20`. `keyCount=0` означает использование
соответствующей компоненты bind-local transform.

Кривые Vector3 интерполируются линейно, quaternion — через slerp. Для row-vector
convention итоговая skinning matrix равна `inverseBind * animatedBoneWorld`.
`animatedBoneWorld` нельзя строить только по palette bones: в Bloom control rig
содержит подтверждённые рёбра `C-lowerRoot -> Pelvis` и
`C-upperRoot -> Spine_01`. SAN анимирует эти промежуточные nodes, поэтому они
обязаны участвовать в накоплении world transform, хотя сами не входят в skin
palette. Их пропуск превращает верхнюю и нижнюю половины тела в независимые
корни и визуально складывает модель около таза.
Проверка всей папки Bloom: 168 из 168 SAN успешно декодированы; `blwalk.san`
имеет duration 1 s, 78 tracks и 16 ключей в наиболее плотных каналах.

Имя track сопоставляется с именем SMO node точно и регистрозависимо. Это
подтверждено PC native trace: one-byte mutation object `[14]`
`R_Ankle -> r_Ankle` загружается без отказа, но новый ключ проходит отдельную
ветвь `char_traits<char>::compare`-дерева и не получает равенства; pristine trace
получает точное `R_Ankle == R_Ankle`. Следовательно, case-only совпадение не
является fallback и должно диагностироваться как потерянная привязка.

PC loader отдельно проверен на one-byte missing parent/leaf и duplicate toe
names в обоих порядках. Все четыре SMO загружаются и не падают: missing target
локален и получает новый binding slot `0xD8`, тогда как descendant `foot_right`
сохраняет exact slot `0x46`. Duplicate namespace имеет один exact key вместо двух
независимо адресуемых записей, а два разных `spTransformTrackEval` получают один
slot `L_Toe=0x3B` либо `R_Toe=0x3F`: это all-target на binding-слое. Точный
визуальный bind-pose/descendant world transform ещё не измерен, поскольку
контекстный `startLevel=2` не входит в активный evaluator tick.

ANM не является FFPS: это текстовая таблица из восьми comma-separated колонок
`Base, Direction, Action, Reaction, Jump, Transition, Num, Animation`. Последняя
колонка ссылается на SAN относительно каталога ANM. Комментарии начинаются с `#`,
строки завершаются `;`, таблица заканчивается строкой `end`.

- `SPL` размещает экземпляры шаблонов уровня;
- `SPT` содержит игровые компоненты и ссылки на SMO;
- `ANM` сопоставляет состояния с анимациями;
- `SAN` хранит анимационный ресурс и также использует контейнер `FFPS`.

Разбор SMO достаточен для первого просмотрщика. Загрузка SPT/SPL и анимаций
является отдельным слоем поверх общего Sparkplug-парсера.
