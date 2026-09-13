# Подтверждённая структура SMO

Этот документ описывает только наблюдения, проверенные на локальном корпусе.
Неизвестным полям и классам не назначается предполагаемая семантика.

## Контейнер FFPS

Все проверенные SMO имеют little-endian заголовок:

| Смещение | Тип | Наблюдаемое значение |
|---:|---|---|
| `0x00` | `char[4]` | `FFPS` |
| `0x04` | `UInt32` | `0x26` |
| `0x08` | `UInt32` | назначение неизвестно |
| `0x0C` | `UInt32` | полный размер файла |
| `0x10` | `UInt32` | вариант; встречены `1`, `2`, `3`, `8`, `9` |
| `0x14` | `UInt32` | абсолютное начало секции данных |
| `0x18` | `UInt32` | размер секции данных |
| `0x1C` | `UInt32` | количество объектов |

В корректном файле:

```text
DataStart + DataSize == FileSize == фактическая длина
```

Варианты `8` и `9` встречены у ресурсов с суффиксом `_ps2`; значение поля
пока не следует трактовать как доказанную версию формата.

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

## Mesh

После `SBOO` у `spMeshData` встречаются как минимум маркеры `E0` и `E1`.
Оба варианта содержат primitive type, индексный буфер, descriptor вершин,
число вершин и сериализованный vertex buffer.

Подтверждённый `primitive type = 3` — triangle strip. Просмотрщик переводит
его в явный список треугольников, меняя порядок первых двух индексов на
нечётных шагах strip и пропуская вырожденные треугольники. Значение `2`
встречается в 45 объектах корпуса, но пока намеренно не декодируется: его
точная топология ещё не подтверждена.

У `E0` после заявленного количества индексов иногда находятся ещё четыре
байта. Это не всегда padding `CD CD CD CD`: в некоторых объектах это два
последних `UInt16`-индекса strip. Выбор варианта делается по точной границе
объекта и структурно валидному заголовку vertex buffer, а не поиском
сигнатуры по файлу.

У `E1` внутреннее поле размера runtime vertex buffer не всегда совпадает
с физическим числом байт в файле. Примеры:

| Файл/layout | Runtime stride | Сериализованный stride |
|---|---:|---:|
| `fish.smo`, `0x093E` | 56 | 44 |
| `bloom_ball.smo`, `0x197E` | 76 | 64 |
| `loading.smo`, `0x0940` | 36 | 36 |

Поэтому чтение дискового буфера с runtime stride приводит к смещению записи.
Позиция в подтверждённых раскладках начинается с трёх `Single` по offset `0`.
У сжатой skinning-раскладки `0x093E` далее находятся четыре `Single` blend
weights по offset `12`, четыре локальных `UInt8` palette indices по offset `28`,
цвет по offset `32` и UV по offset `36`. Нормаль в сериализованном stride `44`
отсутствует и добавляется только в runtime vertex buffer.

На текущем, частично модифицированном корпусе строгий декодер разбирает
24 039 из 25 395 записей `spMeshData`. Оставшиеся случаи:

- 680 записей указывают не на заявленную `typeHash + SBOO`-сигнатуру;
- 494 используют ещё не описанный вариант `E1` из `_ps2.smo`;
- 137 `_ps2.smo` не совпадают с подтверждённой физической границей `E1`;
- 45 используют неподтверждённый `primitive type = 2`.

Эти объекты возвращают явную диагностику и не перенаправляются эвристическим
поиском к похожей последовательности байт.

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
[`SMOTextureTool`](https://github.com/AnDi-SD/SMOTextureTool): она находит
`spTextureData` сканированием сигнатуры по всему файлу, а при изменении размера
pixel buffer пересобирает хвост и исправляет только общие `FileSize` и
`DataSize`. Смещения и размеры последующих записей каталога при этом не
пересчитываются.

## Первый строгий texture path

Подтверждённые части texture decoder перенесены без второго `SmoDocument` и
без глобального signature scan. Каждый pixel buffer теперь ограничен точным
интервалом соответствующей записи каталога `spTextureData`:

| Format code | Source layout | Width | Height | Pixels |
|---:|---|---:|---:|---:|
| `0x0EE3` | BGRA, mip chain | `+0x24` | `+0x28` | `+0x3D` (base level) |
| `0x32E3`, `0x43E3` | BGRA | `+0x24` | `+0x28` | `+0x3D` |
| `0x29E3` | BGRA | `+0x28` | `+0x30` | `+0x34` |
| `0x54E3` | BGRA | `+0x24` | `+0x28` | `+0x3D` |

У вариантов `0x32E3`/`0x43E3` байт `+0x3C` — обязательный нулевой marker
сериализатора, а не blue/alpha-компонент первого пикселя. Реальные texels уже
записаны как BGRA и начинаются с `+0x3D`. Старое сочетание ABGR с чтением от
`+0x3C` случайно возвращало правильные RGB из-за однобайтового сдвига, но
ставило нулевой marker в alpha первого texel, переносило alpha каждого texel
на следующий и теряло настоящий alpha последнего. Marker проверяется отдельно
до чтения pixel buffer.

`0x0EE3` использует ту же раскладку полного BGRA-уровня и тот же marker, но
внешний `E3:0E`-блок содержит также последующие mip levels. Viewer строго
проверяет вложенные размеры `E3:0E`, `E1:20`, `E0:1A` и загружает в OpenGL
базовый уровень; подтверждённый пример — `[656] top` 256×256 из `Alfea03.smo`.

Каналы нормализуются в BGRA32. Вложенные serializer sizes проверяются до
декодирования, а неизвестный format code возвращает
`UNSUPPORTED_TEXTURE_FORMAT`.

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

### Animated texture sequence `0x16FB0E47`

В material mesh `93` файла `bloom_crystal.smo` находятся texture
`sparkles0001`, неизвестный ранее контейнер класса `0x16FB0E47`, а внутри него —
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

`0x54E3` использует nested block headers `E3:54`, `E1:42`, `E0:1A`. Его
pixel buffer уже имеет порядок BGRA: перестановка как для ABGR ошибочно переносит
синий канал в alpha, а настоящий `A=255` — в красный. На проверенных
`bloom_dating_outfit_02/03.smo` обе `0x54E3`-текстуры полностью непрозрачны.

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

CPU triangle-atlas в видимом OpenGL path не создаётся. Старый WPF material path
сохранён только как аварийный fallback при недоступном OpenGL context; невидимая
WPF geometry также используется для выбора мышью. Поэтому описанные ниже размеры
private atlas относятся к fallback-preview и не являются свойствами SMO или
способом работы игры.

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
градиент как vertex alpha; WPF fallback запекает его во временный triangle-atlas.
В обоих случаях mesh рисуется после непрозрачной геометрии. Поэтому сам
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
участки и создаёт ложную «странную texture». Прямой GPU-path не запекает цвета:
он передаёт исходный diffuse stream вместе с UV. Только WPF fallback разворачивает
такой mesh в private triangle-atlas, где каждый треугольник получает независимый
участок. Предельный
размер preview-atlas увеличен с 1 024 до 2 048 треугольников; он также покрывает
однотипные двери `[390]`, `[399]` и `[408]`, не меняя исходный SMO.

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
является unlit и не использует их для недоказанной модели игрового освещения;
WPF fallback преобразует normals inverse-transpose world matrix с последующим
отражением Z. Нулевые normals в отдельных level meshes являются допустимыми
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

Чистый `Media/Levels/Alfea/Alfea03.smo` имеет SHA-256
`65D0EF30F2AC4C211F8C717F340D08A98F2CE4F1D6E080CE77BC217468350FDF`, 4 825
объектов и 510 физических `spMeshData`. Помимо них object graph содержит 757
ссылочных размещений, использующих 156 физических mesh-источников.

Подтверждённая форма такого размещения:

```text
spStaticRenderObject <имя экземпляра>  -- authored world matrix
└─ spModel
   ├─ spMaterialData
   └─ serialized field type 0, payload size 8:
      UInt32 sourceObjectId, UInt32 zero
```

У ссылочного `spModel` нет физического дочернего `spMeshData`. Первый `UInt32`
однозначно разрешается через object ID в существующую запись `spMeshData`, второй
равен нулю. Resolver принимает ссылку только при точном совпадении этой структуры,
единственном объекте с таким ID и наличии владеющего `spStaticRenderObject`; это
не общее правило для любого неизвестного восьмибайтового поля.

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
  `spCollisionInfo`, затем объект класса `0x4DA04889` с vector3. По именам,
  структуре и величинам vector3 интерпретируется как полный размер
  ориентированного collision box; точное имя класса пока не установлено.
- `[91] movement_tracker` — position-only служебный tracker.
- `[95] SubMaster` — корень control/IK rig с `IK-Leg*`, `IK-Hand*`,
  `UP-Knee*`, `UP-Elbow*`, `C-Hand*`, `C-Foot*` и другими control nodes.
  Это не skin palette и не дополнительный деформирующий skeleton.
- `[119] BLOOM` — именованный position-only служебный marker.
- `[120] Ambient01`, class `0x5E6402DF`, содержит только малые state fields без
  transform/геометрии. Имя указывает на ambient/light preset, но назначение и
  имя класса пока не подтверждены, поэтому viewer показывает его информационно.

Viewer не присваивает неизвестным hash вымышленные имена: знак `?` в дереве
явно обозначает рабочую, а не окончательную классификацию.

## Соседние форматы

### SAN animation resource

PC SAN является FFPS-контейнером с одним объектом класса `0x56EE563A`. После
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
