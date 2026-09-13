# Подтверждённая структура SMO

Этот документ описывает только наблюдения, проверенные на локальном корпусе.
Все 36 встреченных SMO-классов имеют строгий structural/read-only decoder;
неизвестным runtime-семантикам и ненаблюдавшимся serializer-ветвям не
назначаются вымышленные значения. Канонический список оставшихся вопросов:
[`../../../research/open-questions.md`](../../../research/open-questions.md),
план быстрых игровых проверок:
[`../../../docs/research/smo-runtime-validation-plan.md`](../../../docs/research/smo-runtime-validation-plan.md).

## Контейнер FFPS

Все проверенные SMO имеют little-endian заголовок:

| Смещение | Тип | Наблюдаемое значение |
|---:|---|---|
| `0x00` | `char[4]` | `FFPS` |
| `0x04` | `UInt32` | serializer version `0x26` |
| `0x08` | `UInt32` | 15-битный export/session tag candidate; точный producer неизвестен |
| `0x0C` | `UInt32` | полный размер файла |
| `0x10` | `UInt32` | platform mask: common `1`, PC `2`, PS2 `8` |
| `0x14` | `UInt32` | абсолютное начало секции данных |
| `0x18` | `UInt32` | размер секции данных |
| `0x1C` | `UInt32` | количество объектов |

В корректном файле:

```text
DataStart + DataSize == FileSize == фактическая длина
```

PC loader принимает маски с битом `1` или `2`, PS2 loader — с битом `1` или
`8`; поэтому `3` и `9` являются combined masks. `0x08` не читается функцией
валидации заголовка и допускает ноль в простом и контекстном Bloom runtime-тесте.
Все значения канонического корпуса лежат в `0x0029..0x7FBE`, не коррелируют с
размерами/offsets/object count и сохраняются при изменении содержимого файла.

## Каталог объектов

С `0x20` находятся записи переменной длины:

```text
UInt32 id
UInt16 nameLength
Byte   name[nameLength]    // длина включает завершающий NUL
UInt32 typeHash
UInt32 logicalOffset       // относительно DataStart
UInt32 serializedSize
```

За последней записью и непосредственно перед `DataStart` находятся четыре
нулевых байта.

В исходных файлах физический адрес объекта равен:

```text
physicalOffset = DataStart + logicalOffset
```

Интервалы объектов могут быть непересекающимися или полностью вложенными.
`serializedSize` описывает объект вместе с его сериализованным поддеревом,
поэтому размер нельзя вычислять как расстояние до следующей записи.

## Объекты SBOO

Обычная запись объекта начинается так:

```text
UInt32 typeHash
char[4] "SBOO"
... сериализованные поля ...
```

Первый байт поля `spDataBlockSerializer`:

```text
bits 0..4  field type
bits 5..7  size code
```

Коды размера:

| Код | Размер payload |
|---:|---|
| `0` | 0 |
| `1` | 1 |
| `2` | 2 |
| `3` | 4 |
| `4` | 8 |
| `5` | следующий `UInt8` |
| `6` | следующий `UInt16` |
| `7` | следующий `UInt32` |

Если пятибитный type равен `0x1F`, настоящий type хранится в следующем байте.

Подтверждённые class ID находятся в `SmoClassRegistry` проекта Core.
Основные из них:

- `0x695C0F65` — `spNode`;
- `0x603625D0` — `spRenderNode`;
- `0x763277DB` — `spModel`;
- `0x681F2043` — `spSkin`;
- `0x52E86EFE` — `spTextNode`;
- `0x19A745D7` — `spTextRenderable`;
- `0x4693490A` — `spFont`;
- `0x33C34CF0` — `spMeshData`;
- `0x6160348B` — `spMaterialData`;
- `0x78EA082B` — `spTextureData`;
- `0x47A97C0E` — `spCollisionInfo`.

## Node и RenderNode

`spNode` сериализует поля 0..8: position, rotation, scale, bone/static flags,
повторяемые child relationships, billboard axis, collision relationships и
animated flag. Transform и false/zero defaults могут опускаться. Relationship
имеет одну из трёх форм:

```text
UInt32 objectId
UInt32 objectId; UInt32 inlineSerializedSize
UInt32 objectId; UInt32 inlineSerializedSize; byte inlineSboo[]
```

`spRenderNode` состоит ровно из двух законченных секций. Сначала идёт весь
унаследованный serializer `spNode`, затем собственная секция с повторяемым field
0 `esfRenderNodeRenderable`. Её target имеет тип `spModel`, `spSkin`,
`spParticleSystem` или `spLensFlare`; пустой список допустим. Внутри Viewer
секции различаются по позиции от конца объекта, поэтому собственный field 0 не
смешивается с унаследованным position.

PC и PS2 используют один контракт. В PC встречаются редкие четырёхбайтовые
ID-only renderable/collision references; это валидный legacy encoding, а не
повреждённый inline size. Подробнее:
[`smo-class-sp-render-node.md`](../../../docs/research/smo-class-sp-render-node.md).

## Model

`spModel` также состоит ровно из двух секций. Унаследованная секция
`spRenderable` содержит optional field 0 material, optional field 1 fog и
совместную пару `UInt32` fields 2/3 `AlphaSortEnable`/`Priority`. Собственная
секция содержит обязательный field 0 Base mesh и optional field 1 Projection
group. Три relationship ведут только в `spMaterialData`, `spFog` и
`spMeshData` соответственно.

Строгий decoder поддерживает ID-only, sized-reference и inline-SBOO encodings и
семь подтверждённых вариантов присутствия. Position/rotation/scale не являются
полями `spModel`; transform приходит от node или `spStaticRenderObject`.
Viewer показывает все шесть semantic fields read-only. Полный корпусный отчёт:
[`smo-class-sp-model.md`](../../../docs/research/smo-class-sp-model.md).

## Mesh

`spMeshData` является контейнером из optional field `0` cross-platform geometry,
optional field `1` Direct3D либо native PS2 representation, optional field `2`
PS2 AABB и финального нулевого terminator. Полный v2-корпус содержит 66 191
уникальный mesh, 87 330 непустых полей и семь структурно-семантических вариантов;
все они строго декодируются до границы объекта.

PC E0/E1 содержат `primitiveType`, 16-битный index buffer, vertex format/count
и disk vertex buffer. Поддерживаются `2 = triangle list` и `3 = triangle strip`.
E0 допускает два последних strip-index в дополнительном слове либо
`CDCDCDCD` padding. E1 повторяет format/count после indices и отдельно хранит
runtime vertex byte size; у skinning-layouts disk/runtime strides различаются.

Native PS2 field `1` состоит из 40-байтового header и
`dmaQwordCount × 16` байт DMA/VIF. Header содержит bounding sphere, два count,
vertex format и числа дополнительных UV/weights. Field `2` — шесть `Single`
`minXYZ/maxXYZ`. DMA/VIF stream пока остаётся непрозрачным read-only payload.

Платформа файла не задаёт layout representation: в пяти `_ps2.smo` PC-корпуса
находятся 629 настоящих native PS2 mesh, 135 из них с cross-platform copy. Это
объясняет прежние группы «PS2 preamble/boundary» и согласуется с наличием
`spPS2MeshDataSerializer` в PC executable. Полный отчёт и таблица 13 Direct3D
vertex layouts:
[`smo-class-sp-mesh-data.md`](../../../docs/research/smo-class-sp-mesh-data.md).

## Изменённые текстуры

В нескольких экспериментально изменённых файлах увеличенный raw texture
buffer сдвинул последующие физические данные, но каталог объектов сохранил
старые logical offsets. Игра такие файлы может загружать последовательным
сериализатором, однако прямое вычисление адреса из каталога после первой
замены становится недостоверным.

Парсер обязан:

1. проверять `typeHash + SBOO` по вычисленному адресу;
2. выдавать диагностику вместо чтения неверного блока;
3. использовать чистые или `_old`-файлы для восстановления базовой структуры.

Это согласуется с реализацией
[`SMOTextureTool`](../../SMOTextureTool/README.md): она находит
`spTextureData` сканированием сигнатуры по всему файлу, а при изменении размера
pixel buffer пересобирает хвост и исправляет только общие `FileSize` и
`DataSize`. Смещения и размеры последующих записей каталога при этом не
пересчитываются.

## Строгий `spTextureData` path

Каждый texture payload ограничен точным интервалом object-directory entry и
разбирается как дерево data-block полей. Глобальный signature scan и таблица
фиксированных offsets больше не используются.

Старая классификация `0x0EE3`, `0x32E3`, `0x43E3`, `0x54E3` и `0x29E3` была
ошибочной. `E3` — это compact header field 3 с 32-битным payload size, а
следующий байт — младшая часть этого размера. Значение зависит от общей длины
representation и mip chain, а не от pixel format. Байт `+0x3C` в обычной
Direct3D-форме является последним байтом `mipHeight`; отдельного serializer
marker там нет. `SmoTexture.FormatCode` временно хранит эту пару байтов только
как legacy diagnostic signature.

Прямые source fields:

- field 0 — legacy cross-platform BGRA32;
- field 2 — `SourceNone`, executable-confirmed, в корпусе не наблюдается;
- field 3 — embedded source;
- field 4 — `SourceReference`, executable-confirmed, в корпусе не наблюдается;
- field 6 — platform type, встречается перед семью legacy PC field 0.

Embedded field 3 содержит base-секцию `field 2 = false`, затем производную
секцию с optional platform field 6, optional cross field 0 и optional native
field 1. Platform values 6/7 выбирают Direct3D, 8/9 — PS2; значение 9 сохраняет
также cross-копию. Legacy layouts могут опускать field 6.

Cross-platform representation содержит width, height, auxiliary value,
`bytesPerPixel=4` и точный `width*height*4` BGRA buffer. Direct3D representation
содержит первый mip в field 0 и последующие mip в повторяемых fields 1; каждый
уровень хранит width, `rowStride=width*4`, height и BGRA buffer.

PS2 representation хранит pixel format, width/height, auxiliary value,
`mipCount`, optional palette и mip records. Наблюдаемые форматы:

| Pixel format | Представление | Palette | Размер mip buffer |
|---:|---|---:|---:|
| 0 | indexed 4-bit | 16×4 байта | `ceil(width*height/2)` |
| 1 | indexed 8-bit | 256×4 байта | `width*height` |
| 3 | 32-bit | нет | `width*height*4` |

Все 7 485 PC/PS2-объектов проходят строгую проверку контейнеров, палитр и
1–9 mip-уровней. Viewer нормализует cross/Direct3D данные в BGRA32. Native PS2
swizzle/channel conversion пока не угадывается, но вся его структура видна в
read-only inspector. Полная статистика и пять storage-вариантов приведены в
[`smo-class-sp-texture-data.md`](../../../docs/research/smo-class-sp-texture-data.md).

### FinalBlendOp, MaterialRenderStates и прозрачный проход

В `spMaterialData` data-block field type `3` с payload `UInt32` хранит
`FinalBlendOp`, а не независимые render flags. Первый field type `0` размером
44 байта содержит связанные 11 значений `MaterialRenderStates`. Операции нельзя
разбирать побитово: `0x4` и `0x5` принадлежат разным effect-семействам, а `0x6`
требует подходящего companion/consumer state. `0x0` и большинство `0x2`
проходов непрозрачны, однако одно значение операции само по себе не задаёт
полную семантику материала.

У эталонного `Media/Characters/Bloom/bloom_princess.smo` подтверждено узкое
исключение для `0x2`. Tiara mesh `[83] Bloom_princess_mesh_1_2899.387222`
находится под `spSkin [81]` и material `[82]`; material имеет точный tuple
`[0, 0, 1, 0, 1, 0, 3, 0, 4, 0, 6]`, skin —
`AlphaSortEnable=0`, `Priority=1`, а положительные vertex weights используют
только palette slot `14` (`Head`). Vertex diffuse всех 18 вершин равен
`0xFF000000`. Material `[82]` ссылается на общий texture object `[5] b_prince`,
который встроен под material `[4]`; это reference-only sharing, а не отдельная
копия atlas.

Полный `b_prince` 256×256 содержит 370 texels с alpha 0, 15508 с alpha 1..254 и
49658 с alpha 255. Raster по 16 UV-треугольникам tiara покрывает 3948 уникальных
texels: 53/3746/149 соответственно. Все 16 треугольников затрагивают
промежуточный alpha. Следовательно, этот эталон нельзя описывать как
подтверждённый бинарный alpha-test/cutout. Viewer распознаёт его как
`PrincessTransparentSurfaceFinalBlend2` только при одновременном совпадении
точного tuple, skinned surface consumer, `spSkin A0/P1`, чёрного vertex diffuse
и реально покрытых UV0 texels как с нулевым, так и с промежуточным alpha.
Sibling chunks `[85]`, `[87]`, `[89]` используют тот же material context, но не
покрывают ни одного texel с alpha 0 и поэтому остаются opaque. Произвольный
`op2`, сам факт наличия alpha в atlas или alpha в неиспользуемой UV-области
остаются недостаточными. Эта классификация управляет лишь OpenGL alpha preview и
прозрачным порядком; она не доказывает native blend equation и не модифицирует
SMO.

В pristine `Media/Characters/Minautor/Minautor.smo` mesh `[13]` подтверждает
второй точный `FinalBlendOp=0x2` skinned-surface контракт:
`MaterialRenderStates=[0,0,1,0,1,1,3,0,4,0,6]`, consuming `spSkin A1/P1`,
uniform vertex diffuse `0xFF000000` и UV0-covered partial texture alpha
(`17628` sampled texels: `44/1377/16207` alpha-zero/partial/opaque). Viewer
называет этот bound режим `SkinnedTransparentSurfaceFinalBlend2`, включает
authored alpha и прозрачный порядок, но не emissive. Он не объединяется с
`PrincessTransparentSurfaceFinalBlend2`: у princess tiara tuple имеет
`RS[5]=0`, а consuming skin — `A0/P1`.

Гибрид princess tuple `[0,0,1,0,1,0,3,0,4,0,6]` + `spSkin A1/P1` + partial
UV alpha не переводится обратно в opaque, поскольку это скрывало бы реальную
прозрачную геометрию в Viewer. Он получает отдельную непроверенную классификацию
`UnconfirmedTransparentSurfaceFinalBlend2Hybrid`, authored-alpha preview и
`UNCONFIRMED_FINAL_BLEND_2_HYBRID`. Pristine bound fixture для этой комбинации
не найден; OpenGL-кадр не подтверждает её корректность в игре.

Для подтверждённого princess-профиля и указанного гибрида Analyzer сравнивает
диагональ partial-alpha run с opaque character bounds. При доле от `35%` и
`RS[5]=0` выводится `LARGE_ALPHA_RS5_ZERO_ORDERING_UNCONFIRMED`. Это структурный
маркер больших крыльев/накладок, которые могут взаимодействовать с foliage и
другими level draws иначе, чем в изолированном OpenGL viewport. Название
диагностики отражает наблюдаемый профиль; точная семантика `RS[5]` как native
depth-write state всё ещё не доказана.

Отдельный `IMPORTED_FACE_VERTEX_DIFFUSE_MISMATCH` применяется к generated
`imp_o_x_*`: если сохранённые skinned-поверхности имеют uniform `FFFFFFFF`, а
opaque face runs — `FF000000`, Viewer показывает баннер
**MIXED FACE VERTEX DIFFUSE**. Native fixed-function lighting может обработать
такие runs неодинаково при вращении модели, но текущий unlit OpenGL shader не
воспроизводит эту формулу. Turntable **«Поворот модели»** лишь вращает model root
для осмотра геометрии и не выдаётся за эмуляцию игрового света.

Для этого же точного princess-style профиля Viewer выполняет отдельную структурную
проверку native draw granularity. Много пространственно удалённых связных компонентов
и несколько активных deform-targets внутри одного `spSkin/material/mesh` дают
`NATIVE_ALPHA_RUN_GRANULARITY_UNCONFIRMED`. OpenGL может удачно нарисовать такой
mesh, но это не подтверждает native depth/sort: независимые глаза, рот, украшения, крылья
и подвески должны сохранять исходные material/renderable run boundaries. Это
консервативная диагностика структуры, а не утверждение о доказанной причине кадра.

Независимая проверка `NATIVE_ALPHA_DECAL_DEPTH_UNCONFIRMED` применяется только к
точно подтверждённому princess-style `op2` partial-alpha skinned run с normals.
Она сравнивает bind-pose вершины с треугольниками непрозрачных skinned-соседей.
Риск фиксируется, если диагональ кандидата не больше 35% диагонали opaque-body,
медианное расстояние не больше 0,1% этой диагонали, а не меньше 50% вершин лежат
в том же допуске и имеют `abs(dot(normalCandidate, normalOpaque)) >= 0.85`.
Такая геометрия может корректно смешиваться в OpenGL, но остаётся чувствительной к
неподтверждённым native depth rejection, z-fighting и draw order.

При срабатывании Viewer выводит явно подписанную диагностику
`NATIVE_ALPHA_DECAL_DEPTH_UNCONFIRMED`. OpenGL продолжает рисовать authored alpha
обычным прозрачным проходом с отключённой записью depth; Viewer не обнуляет alpha
и не симулирует исчезновение overlay. Это предупреждение о недоказанных native
depth rejection, z-fighting и draw order, а не попытка воспроизвести точный кадр
игры; файл SMO и texture resources не меняются.
Большой `RS[5]=0` run при этом может независимо получить предупреждение о
неподтверждённом порядке относительно геометрии уровня.

Другое подтверждённое семейство прозрачных skinned-поверхностей с normals у
IceWorm и Yeti имеет
`FinalBlendOp = 0x6` и точный `MaterialRenderStates` tuple
`[0, 0, 1, 2, 1, 1, 3, 0, 4, 0, 6]`. Проверки только
`MaterialRenderStates[8] = 4` недостаточно: у generated Bloom-моделей встречается
`[0, 0, 1, 0, 1, 1, 3, 0, 4, 0, 6]`, где отличается также `RS[3]`.
Подтверждённый rigid-пример — щит Knut — использует точный tuple
`[0, 0, 1, 0, 1, 0, 3, 0, 2, 0, 6]`. Эти tuples являются
consumer-specific corpus evidence, а не универсальными определениями native
blend equations. Effect operation `0x4` встречается у светящихся SFX; её
отображение как обычного alpha раньше маскировало ошибочные модели в Viewer.

Rigid level effects не обязаны использовать только первую из этих пар:
book glow/spark meshes `[273]`, `[278]`, `[294]`, `[298]`, `[313]`, `[317]` в
`Alfea03.smo` не имеют skinning и хранят точный effect tuple
`[0, 0, 1, 2, 1, 1, 3, 0, 2, 0, 6]`. Поэтому `RS[3]`/`RS[5]` сами по себе не
доказывают skinned consumer. Viewer принимает обе подтверждённые формы для
rigid/effect consumer и показывает op6/companion2 эмиссионным приближением.

Surface-профиль также нельзя распространять на всякую skinned-геометрию с
`FinalBlendOp=0x6`. Droid/Golem trails используют
`[0, 0, 1, 0, 1, 0, 3, 0, 2, 0, 6]`, а Stormy thunder/lightbeam —
`[0, 0, 1, 2, 1, 1, 3, 0, 2, 0, 6]`; это подтверждённые skinned effect
семейства с меняющимися `AlphaSortEnable`/`Priority`. Viewer распознаёт эти два
точных effect tuple отдельно и не выдаёт по ним ложное предупреждение surface
family. Generated Bloom tuple с сочетанием `RS[3]=0`, `RS[5]=1` не совпадает ни
с подтверждённой surface, ни с подтверждёнными effect-парами.

У непосредственно владеющего mesh объекта `spSkin` прямой 4-байтовый field
type `2` декодируется как `AlphaSortEnable`, а следующий прямой field type `3` —
как `Priority`. Alpha-consuming skins `[23]` IceWorm и `[93]` Yeti имеют
`AlphaSortEnable=1`, `Priority=1`; generated Bloom skins имеют соответственно
`0` и `1`. Viewer привязывает эти значения по ближайшему ancestor `spSkin`,
показывает их в render-state diagnostic и считает generated пару
неподтверждённой. Направление сортировки по `Priority` и точная native-семантика
этих полей пока не восстановлены, поэтому OpenGL не имитирует их как доказанный
render order.

В тех же сравниваемых IceWorm/Yeti alpha meshes vertex diffuse однородно равен
`0xFF000000`, тогда как пять основных generated Bloom chunks однородно заполнены
`0xFFFFFFFF`. Viewer сообщает это как
`VERTEX_DIFFUSE_PROFILE_DIVERGENCE_UNCONFIRMED`: это наблюдаемое расхождение,
но причинная связь с native-свечением или прозрачностью не установлена и только
оно само не переключает OpenGL preview на effect-like материал.

В `knutBoss.smo` shield mesh `[13]` связан с материалом `[10]` (`0x6`, companion
state `2`) и текстурой `gr_01`: 211 пикселей имеют alpha 0, 746 — промежуточный
alpha, 67 — alpha 255. Viewer сначала добавляет непрозрачную геометрию, затем
смешиваемую геометрию. `0x4`/`0x5`, несовпадающий полный consumer tuple и
`AlphaSortEnable`-divergence получают отдельное явно диагностированное
OpenGL-приближение. Оно помогает увидеть данные, но не является точной реализацией
native blend equation или native frame.

Exporter считает сигналом blending также alpha явного diffuse-цвета
материала или фактически записываемого `COLOR_0`. В `Icy.smo` material
alpha около `0.498` поэтому включает GLB `alphaMode=BLEND`. Alpha texture
без render/material/vertex сигнала не считается доказательством
прозрачности: opaque GLB получает RGB PNG, blend GLB — RGBA PNG.

В OBJ/MTL `map_Kd` остаётся RGB, texture alpha выносится в отдельную
grayscale `map_d`, а material и uniform vertex alpha записываются через `d`.
Varying `COLOR_0` alpha непредставим в OBJ. FBX завершается ошибкой
до Blender при любом `COLOR_0` alpha меньше `1` или при сочетании
texture alpha × material alpha. Подтверждена только перечисленная выше часть
семантики; MASK/alpha cutoff и точные native equations операций `0x4`/`0x5`
не восстановлены.

Двухслойный материал экспортируется как base-color texture по UV0 и
первый кадр effect texture по UV1; полная временная texture sequence пока не
имеет прямого стандартного представления glTF и сопровождается предупреждением.

Rigid mesh под анимируемым `spRenderNode` хранит локальный bind transform
относительно этого узла. Пример: mesh `[6]` очков `knut.smo` привязан к render
node `[2] Knut_TEMP_glasses` и должен следовать одноимённому SAN-треку. Viewer
использует эту связь при CPU-анимации, Exporter сохраняет её и сам render node в
иерархии GLB/FBX.

Связь texture → material → owner ← mesh строится по `ParentIndex` каталога.
Сначала binding строится по общему interval-владельцу mesh/material. Текстура
может находиться глубже material/pass/layer, поэтому обходится всё поддерево
`spMaterialData`, а не только непосредственные дети. Для character-моделей
весь binding первой mesh-части — texture, diffuse ARGB, alpha-blend state
и layered/animation metadata — распространяется на остальные `spSkin`-части того же
`spRenderNode`; нумерованные sibling render nodes могут образовывать одно
семейство материала. Отдельные части без собственного материала используют
первичный atlas модели. Material layers с несколькими текстурами пока получают
fallback.

Family/primary fallback не объединяет blend-состояния разных материалов:
выбирается ближайший цельный source binding для той же texture. Явный
diffuse текущего материала имеет приоритет над унаследованным `DiffuseArgb`.

Если один `spRenderNode` последовательно содержит несколько явных материалов,
не указавший material `spSkin` наследует texture от ближайшего предшествующего
текстурированного mesh-блока, а не самый большой atlas всего render-node. Так
сохраняется граница переключения `sky → spe_body` в `Sky_Head` модели
`prince_dating_outfit_02.smo`.

Вложенный `spRenderNode` может вообще не повторять material. Для mesh,
непосредственно принадлежащего `spSkin`, поиск тогда продолжается вверх по
родительским render-node и по-прежнему выбирает ближайший предшествующий явный
binding. Это восстанавливает `sky` для meshes `66/68` в
`prince_dating_outfit_03.smo`. Подъём запрещён для rigid `spModel`, чтобы детали
рта и накладки глаз не получали чужой character atlas.

Vertex diffuse в character layouts может уже содержать экспортированную
светотень. При запекании tint в копию atlas несколько UV-треугольников могут
покрывать один texel разными цветами. Их вклады усредняются, а не
перезаписываются последним треугольником. Материал остаётся обычным WPF
`DiffuseMaterial`: `EmissiveMaterial` несовместим с alpha игровых атласов и
создаёт аддитивное свечение и визуальную прозрачность.
Это особенно существенно для meshes `131/133` в
`prince_dating_outfit_01.smo`, где конфликтующие UV имеют соответственно
312/350 и 101/118 вершин.

В тех же meshes найдено по шесть геометрически корректных треугольников, у
которых все три UV намеренно равны `(0.65321416, 0.46899855)`. Их vertex diffuse
не чёрный, но sentinel texel атласа чёрный; обычное текстурирование растягивает
его на полигоны носа и рта. В приватной tinted-копии atlas окрестность такого
sentinel заменяется средним непрозрачным diffuse вырожденных UV-треугольников.
Исходная texture и геометрия не модифицируются.

Наличие diffuse-поля в layout ещё не гарантирует, что exporter включил vertex
color. В `Amaryl.smo` все шесть meshes формата `0x097E` заполняют каждую вершину
одним `0xFF000000`; глаза `bloom_bike.smo` аналогично заполнены постоянным
`0xFF202020`. Однородный RGB-поток является material/export sentinel и не
модулирует texture. В `Troll.smo` mesh `[86]` содержит 425 чёрных и только 10
белых вершин; такой почти полностью чёрный двухцветный поток также является
placeholder, иначе корректный цветной atlas становится чёрным. Для character
layouts `0x097E`/`0x197E` он игнорируется при доле точного opaque-black
`0xFF000000` более 95% и не более чем двух различных RGB; настоящие
многотональные градиенты ресниц и ночных dating-моделей остаются активными.

### Animated texture sequence `spAnimTexController` (`0x16FB0E47`)

В material mesh `93` файла `bloom_crystal.smo` находятся texture
`sparkles0001`, контейнер `spAnimTexController`, а внутри него —
`sparkles0002…0010`. Префикс контейнера начинается блоком `E0 0C 44 02 00`,
затем хранит `uint32 keyCount` и `keyCount` значений времени `float32`. Для
проверенного объекта `keyCount=38`, последний ключ равен примерно `1.266667 s`.

Десять одноразмерных текстур с общим именованным префиксом декодируются как
одна sequence. Viewer использует длительность последнего ключа и получает время
кадра `1.266667 / 10 ≈ 0.126667 s`. Animated binding локален своему material и
не наследуется следующими `spSkin`-частями; иначе sparkle ошибочно заменяет
основной `b_crysta` atlas у meshes `95…103`.

Форматы `0x1940` и `0x197E` хранят UV1 в последних восьми байтах вершины:
смещения `+36` и `+56` соответственно. У crystal mesh `93` UV0 выбирает зелёный
участок предшествующего `b_crysta`, а постоянный UV1
`(0.035156, 0.464844)` семплирует текущий sparkle frame. Viewer запекает каждый
кадр как `b_crysta(UV0) + sparkles(UV1) × alpha`; поэтому чёрный фон effect-map
не заменяет зелёную основу.

Статический layout `0x1900` со stride 32 хранит `XYZ` по `+0`, diffuse ARGB по
`+12`, UV0 по `+16` и UV1 по `+24`; normal stream отсутствует. Все пять таких
mesh в pristine `Alfea_broken_01.smo` принадлежат `WallA-000`. Крупный mesh
`[4085]` имеет 877 вершин и 303 различных RGB, материал без texture object и
непрозрачный `FinalBlendOp=0`. Следовательно, его поверхность должна получать
интерполированный vertex diffuse, а не служебный голубой fallback Viewer;
неоднородные старшие байты diffuse при opaque-материале как прозрачность не
используются.

Тот же двухканальный контракт встречается без animation sequence. Материал
`[4599]` объекта `Pcrystal12` в `Alfea03.smo` последовательно содержит
`[4600] crystal2`, `[4601] cryst_hl` и `[4602] spUVController`, а mesh `[4603]`
имеет layout `0x1940`. Это статическая композиция `crystal2(UV0) +
cryst_hl(UV1) × alpha`, а не неоднозначный набор из двух конкурирующих текстур.

Если конфликтующие vertex RGB требуют private triangle-atlas, его разрешение
выбирается только из данных mesh и назначенной texture. Для каждого треугольника
Viewer измеряет размах `ΔU × texture.Width` и `ΔV × texture.Height`; требуемая
сторона ячейки равна округлённому вверх максимальному размаху, плюс один конечный
отсчёт и два защитных пикселя с каждой стороны. Имя, назначение объекта, число UV
и эвристика «маленького mesh» в этом решении не участвуют.

Mesh `[2756] plaque02` из `Alfea03.smo` имеет 20 треугольников и 16 UV, причём
несколько узких треугольников адресуют почти 128 texels `sign_faragonda` по
горизонтали. Поэтому универсальная формула даёт ячейку 133×133; прежние 32×32
уменьшали надпись примерно в четыре раза ещё до экранной фильтрации WPF.

WPF-preview ограничивает один такой bitmap размером 2048 по стороне и 1 048 576
пикселями. Это общий ресурсный предел для всех mesh, а не вывод о содержимом SMO:
если требуемая ячейка в него не помещается, фактическое preview-разрешение
снижается и не должно описываться как достоверность исходного рендера. Без
ограничения равномерная квадратная раскладка на проверенном корпусе потребовала бы
в худшем случае bitmap 22 178×20 472 (около 1,8 GB BGRA) для одного mesh. При
выборе mesh в дереве журнал показывает `требуется`, `используется`, итоговый
размер bitmap и явную отметку о срабатывании бюджета.

Некоторые `bloom_dating_outfit_02/03.smo` дают legacy diagnostic signature
`0x54E3`, но это не отдельный format: `E3` — field header, `0x54` — младший байт
payload size. Их structurally decoded Direct3D pixel buffer имеет порядок BGRA;
перестановка как для ABGR ошибочно переносит синий канал в alpha, а настоящий
`A=255` — в красный. Обе проверенные текстуры полностью непрозрачны.

Если character export содержит несколько первичных atlas одинаковой площади,
материал отдельной `spSkin`-части может не повторять texture object. В таком
случае выбирается ближайший предшествующий равновеликий atlas в каталоге. Это
различает `b_biked` и `bloom_jeansd` для `hair_n_stuff` в
`bloom_dating_outfit_03.smo`. Правило применяется только к небольшим character
graphs; level graphs не получают character fallback.

Primary-atlas fallback применяется только непосредственно к `spSkin`-частям.
Жёсткий `spModel`, даже если он сериализован внутри поддерева кости/skin, без
явного texture object сохраняет material/vertex color. Иначе props вроде
`iceCream_ball` и `iceCream_spoon` ошибочно получают atlas одежды.

Для vertex format `0x940` со serialized stride 36 подтверждены diffuse ARGB
по `+24`, normal XYZ по `+12` и UV0 как два `Single` по `+28`. Для `0x1940`
подтверждены те же offsets при serialized stride 44. Не внесённые в реестр vertex formats
остаются position-only до отдельного подтверждения.

Для vertex format `0x0800` подтверждены serialized stride 20 и UV0 как два
`Single` по `+12`; diffuse color в этом layout отсутствует. Формат встречается
у глаз в `bloom_dating_outfit_02.smo`.

Для vertex format `0x0840` подтверждены serialized/runtime stride 32, normal XYZ
по `+12` и UV0 как два `Single` по `+24`; diffuse color отсутствует. Fixture —
меш `[284]` (`dseed`) из `Alfea02.smo`: 53 вершины, UV
`(0.042858098, 0.014410913)..(0.9300215, 0.9888289)` и texture `dseed` 128×128.

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

Проверка `bloom_jeans.smo` уточняет практическое следствие: 95 `spNode` и полный
скелет распределены между шестью независимыми `spSkin`/mesh. Основная часть тела
использует 16 slots для ног, корпуса, головы и clavicle; отдельные palettes содержат
`L_Bicep`/`R_Bicep`, `L_UpperArm`/`R_UpperArm`, кисти и пальцы. Одинаковый числовой
slot в разных palettes обозначает разные кости. GUI и importer поэтому должны
идентифицировать кость по object ID/index, а palette index показывать только в
контексте конкретного `spSkin`.

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

В shipped-корпусе соседний field 2 `InvTransform` не вычисляется общим matrix
inverse. Sparkplug транспонирует верхний `3x3` field 1 и записывает translation
`-T*A^T`; при authored scale это отличается от `Matrix4x4.Invert`.

Ранний Level Creator записал в `Alfea02.smo` математическую обратную матрицу, и
игра всё равно показывает эти объекты. Это подтверждает совместимость результата,
но пока не доказывает механизм: runtime может принять field 2 как второй вариант,
проигнорировать его, пересчитать либо использовать только в отдельном consumer.
Строгий decoder распознаёт обе формы для совместимости редактора, а placement
writer создаёт каноническую transpose-basis пару shipped-корпуса. Точная
семантика остаётся открытой до трассы executable.

После исправления `[4201]` получает identity world matrix, а `[1372]` под
`Darch_A01` — в точности authored static world matrix без sector offsets.

## Shared mesh instances в Alfea03.smo

Чистый `Media/Levels/Alfea/Alfea03.smo` имеет SHA-256
`65D0EF30F2AC4C211F8C717F340D08A98F2CE4F1D6E080CE77BC217468350FDF`, 4 825
объектов и 510 физических `spMeshData`. Помимо них object graph содержит 757
ссылочных размещений, использующих 156 физических mesh-источников.

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

Viewer сохраняет различие между источником и размещением: в дереве ссылка
помечается `Mesh instance → [индекс источника]`, в корне, статусе и журнале числа
физических mesh и экземпляров выводятся отдельно. Выбор физического источника
подсвечивает все его размещения в собранной сцене. Это предотвращает ошибочное
впечатление, что файл действительно содержит сотни дублированных mesh.

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

Для `spPartitionRenderable` строгий decoder принимает обязательный field 1
`UInt32 DebugColor` и один или несколько повторяемых field 0. Каждый field 0
должен быть ненулевым inline relationship на `spModel`, разрешаться через object
ID и одновременно быть физическим child выбранного partition renderable.
Inspector показывает цвет как `#AARRGGBB` с каналами и target каждой модели.
Полный корпус содержит 7 208 объектов, 19 989 отношений и только этот один
структурный вариант; большие serialized sizes принадлежат вложенным моделям.

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

Для `spBSPNode` decoder читает ту же базовую секцию, но требует slots 0/1.
Ветка является inline physical-child `spBSPNode` либо sized reference на
terminal `spPartitionNode`; Zone и PartitionRenderable в корпусе записаны как
нулевые ID-only relationships. Собственная секция всегда содержит field 0
Plane (`Vector3 normal + Single constant`) с единичной конечной нормалью и может
содержать field 1 Polygon (`UInt32 count + Vector3[count]`). Polygon поддержан
обоими executable, но отсутствует во всех 324 объектах. Analyzer проверяет 54
полных двоичных дерева и точную формулу размера `101 * BSP split count`.

Для `spOcclusionVolume` decoder читает унаследованную секцию `spNode`, затем
требует собственные field 0 IndexBuffer и field 1 VertexBuffer. Первый имеет
форму `UInt32(2), UInt32 triangleCount, UInt32(0), UInt16[3*T]`; второй —
`UInt32(0), UInt32 vertexCount, UInt32(0), Vector3[V]`. Inspector показывает
число треугольников, index format, число вершин и bounds. Все 60 объектов
трёх корпусов декодируются; 20/20 PC/PS2-пар совпадают побайтно. Наблюдаемые
формы являются connected planar disks с `T=V-2`. Их cardinality — 4/2, 5/3,
6/4 и 10/8 vertices/triangles. 54/60 строго выпуклые; шесть повторов одного
decagon имеют неглубокую authored-вогнутость, хотя executable содержит
convexity validation messages.

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

Для `spPartitionSystem` decoder читает три секции: унаследованные `spNode` и
`spRenderNode`, затем собственный field 0 `PartitionRoot`. В корпусе node-поля
образуют `IsStatic`, `IsAnimated`, упорядоченные zone/portal children и collision
references; renderable-список пуст. Root обязателен и разрешается как inline
physical-child `spBSPNode` либо sized reference на `spOctreeNode`. Inspector
показывает все 5 587 содержательных полей 88 PC/PS2-систем.

Для `spZone` decoder читает две секции: унаследованный `spNode` с обязательными
Position и `Animated=false`, optional Rotation/Static и собственный повторяемый
field 0 `LocalPartitionRoot`. Наблюдаемая cardinality — 0..4; все ненулевые
отношения являются owned inline `spPartitionNode` либо `spOctreeNode`. Inspector
показывает все 1 231 содержательное поле 369 PC/PS2-зон.

Для `spZonePortal` decoder читает одну секцию и строго требует field 0
`DestinationZone`, field 1 `Polygon`, field 2 `Open`, затем один terminator.
Destination является ненулевым relationship на `spZone`; polygon содержит
`UInt32 count` и конечные `Vector3`, Boolean обязан быть 0 либо 1. Inspector
показывает все 1 854 поля 618 PC/PS2-порталов. В корпусе каждый polygon имеет
четыре вершины, `Open=true`, а winding двух порталов одной пары точно обратен.

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
