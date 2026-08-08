# 2026-08-08 — `bloom_dating_outfit_02.smo`

## Объект исследования

`local-data/pc-pristine/Media/Characters/DatingAssets/bloom_dating_outfit_02.smo`:
159 объектов каталога, 14 meshes, 9 materials и 2 встроенные textures.

Индексы в UI не всегда являются meshes. Например, `74`/`78` — `spRenderNode`,
а их геометрия находится в `77`/`82`; контейнеры `116`, `118`, `123`, `126`,
`128` и `131` также содержат соответствующие `spMeshData`.

## Текстуры

Обнаружен ранее неподдерживаемый texture layout `0x54E3`:

- `bloom_jeansd`, 256×256, BGRA — одежда, голова и волосы;
- `bloomeye_testd`, 128×128, BGRA — глазные яблоки.

Подтверждены смещения размеров `0x24`/`0x28`, начало pixels `0x3D` и block
headers `E3:54`, `E1:42`, `E0:1A`. Подтверждён vertex format `0x0800`:
stride 20, UV0 по смещению 12.

Первоначальная трактовка `0x54E3` как ABGR оказалась неверной: она переносила
синий канал в alpha, а настоящий `A=255` — в красный, отчего модель была
полупрозрачной и розовой. В исходном buffer уже лежит BGRA; у обеих textures
alpha равна 255 для всех pixels.

После поддержки bilateral render-node family (`_L`/`_R`, `.L`/`.R`):

- 14/14 meshes имеют UV и texture binding;
- 12 meshes используют `bloom_jeansd`;
- meshes глаз `82` и `86` используют `bloomeye_testd`.

## Открытая проблема skin-space

`Eye_caps`, `Eye_Ball_L` и `Eye_Ball_R` — жёсткие потомки кости `Head`, тогда
как тело и голова уже деформированы как `spSkin`. Обычная композиция node
transforms помещает глаза около `(44, ±2, 7)`, а baked/skinned голова находится
около Y=138..146. Это отсутствие bind-pose преобразования между skeleton space
и пространством skinned vertices. Подгонять сдвиг под один файл нельзя;
следующий этап — восстановление bind/inverse-bind semantics `spSkin`.

## Проверки

- целевой файл: PASS, 102 assertions;
- выборка 10 SMO (`seed=20260808`): PASS, 271 assertions.

## `bloom_dating_outfit_03.smo`

В модели присутствуют два равновеликих атласа: ранний `b_biked` и более поздний
`bloom_jeansd`. У `hair_n_stuff` (`spMeshData` 125) material повторно не содержит
texture object. Старый fallback при одинаковой площади всегда выбирал первый
атлас и ошибочно назначал волосам `b_biked`.

Fallback уточнён: среди равновеликих character atlases выбирается ближайший
предшествующий texture object в каталоге. Теперь mesh 125 получает
`bloom_jeansd`; дополнительного или потерянного mesh в этом месте нет.

Целевой файл: PASS, 94 assertions; Release-сборка успешна.

## Восстановление skeleton и bind pose

`ParentIndex`, вычисленный по вложенным диапазонам каталога, оказался только
serializer ownership. Настоящие связи костей хранятся в `esfNodeChild`
(field type 5) как object ID, inline size и необязательная inline-копия узла.
Размер 0 означает ссылку на ранее сериализованную кость. Так подтверждены, в
частности, цепочки `L_Thigh → L_calf → L_Ankle → L_Toe` и
`Spine_01 → Spine_02 → Spine_03 → Neck → Head`.

В последней секции каждого `spSkin` подтверждена палитра из 16 записей. Каждая
запись содержит reference на `spNode`, optional inline node и row-major 4×4
inverse-bind matrix. У секции без inline nodes размер строго равен
`8 + 16 × (8 + 64) = 0x488`.

Инверсия матрицы даёт bind-world. Для `Head`/`Bloom_head_geometry` в
`outfit_02` все повторения дают примерно `(0, 136.693, -1.512)`. Формула
`attachmentLocal * parentBindWorld` помещает глаза на Y≈144.205 и сохраняет
левую/правую координату X. В `outfit_03`, где персонаж ориентирован иначе,
та же формула без ветвления помещает глаза около Y≈61, Z≈64 рядом с головой.

Для `0x097E`/`0x197E` подтверждены четыре blend weights по `+12` и четыре raw
palette indices по `+28`. Проверяются конечность и нормализация весов, а index
проверяется только при ненулевом соответствующем весе: неактивные bytes могут
содержать значения за пределами палитры.

Регрессии после включения bind path:

- `bloom_dating_outfit_02.smo`: PASS, 383 assertions;
- `bloom_dating_outfit_03.smo`: PASS, 324 assertions;
- 10 SMO, seed `20260808`: PASS, 1045 assertions;
- `Alfea02.smo`: PASS, 3748 assertions, 702 meshes.

## Material-only props в `outfit_02`

UI-объекты `3`, `7` и `35` содержат meshes `6`, `10` и `38`:
`iceCream_ball_01`, `iceCream_ball_02`, `iceCream_spoon01`. В их material нет
texture object; `bloom_jeansd` назначался ошибочно через primary-atlas fallback.

Наличие `spSkin` среди дальних interval-предков недостаточно: ложка является
жёстким `spModel` под костью руки. Fallback теперь разрешён только mesh под
`spSkin`, если между ними нет `spModel`. Все три props остаются без texture
binding и используют подтверждённый собственный material color. То же правило
структурно применяется к `Eye_caps`.

Итог `outfit_02`: 14 meshes, 10 texture bindings, PASS 385 assertions.

## Ресницы `outfit_03`

Meshes `116` и `118` используют правильный atlas лица, но их skinned layout
`0x097E` дополнительно хранит diffuse ARGB по `+44`. Ранее эти четыре байта не
декодировались, а renderer разрешал vertex modulation только для статических
layouts, поэтому ресницы оставались текстурированными цветом кожи.

В mesh `116` найдено 56 diffuse colors и 27 тёмных вершин из 224; в mesh `118`
— 21 цвет и 13 тёмных вершин из 95. Самый тёмный сохранённый цвет обоих мешей —
`0xFF0D132E`. Форматы `0x097E`/`0x197E` теперь декодируют diffuse по `+44` и
проходят через texture × vertex-color modulation.

Итог `outfit_03`: PASS 327 assertions; выборка 10 SMO: PASS 1042 assertions.

## Светлые полосы на UV-швах

WPF не поддерживает texture × interpolated per-vertex color напрямую, поэтому
viewer запекает diffuse modulation в отдельную копию atlas для каждого mesh.
За границей растеризованного UV-острова tint отсутствовал; билинейная фильтрация
смешивала край с исходным светлым texel и создавала полосы на стыках лица,
одежды и соседних mesh-частей.

После растеризации vertex color добавлен двухпиксельный gutter: tint дважды
расширяется на соседние непокрытые texels средним цветом покрытых соседей.
Исходные UV, texture, geometry и diffuse ARGB не изменяются. `outfit_03`
остаётся PASS 327 assertions; выборка 10 SMO — PASS 1042 assertions.

## Нормали и ступени ресниц `outfit_01`

В `bloom_dating_outfit_01.smo` meshes `120` и `124` имеют непрозрачный atlas;
alpha-test не является причиной ступенчатого вида нижних ресниц. Layout `0x097E`
хранит normal XYZ по `+32`, но viewer их не передавал в `MeshGeometry3D`, поэтому
WPF создавал faceted lighting по отдельным треугольникам.

Для `0x097E`/`0x197E` декодируются normals `+32`, для `0x0940`/`0x1940` — `+12`.
В renderer они преобразуются inverse-transpose world matrix и отражаются по Z
вместе с переходом из left-handed Sparkplug в right-handed WPF. Нулевые normals
уровней сохраняются как нулевые заглушки.

Регрессии: `outfit_01` — PASS 327 assertions; `Alfea02` снова декодирует все
702 meshes и проходит 3748 assertions.

## `prince_dating_outfit_02.smo`, material boundary

В `spRenderNode Sky_Head` mesh `89` явно связан с texture `sky`, затем mesh `108`
не повторяет material, а поздние `122/124` используют новый явный atlas
`spe_body`. Старый resolver выбирал крупнейшую texture всего render-node,
поэтому поздний `spe_body` ошибочно назначался более раннему mesh `108`.

Наследование внутри render-node теперь порядковое: выбирается texture ближайшего
предшествующего явно связанного mesh. Результат: `89/108 → sky`,
`122/124 → spe_body`. Целевая модель проходит 337 assertions; три Bloom dating
модели и выборка 10 SMO также проходят без регрессий.

## `prince_dating_outfit_03.smo`, вложенный material scope

Модель намеренно оставлена с её тёмными исходными vertex/material colors: это
соответствует ночному варианту в игре. Ошибка meshes `66` и `68` относилась не к
цвету, а к области наследования texture. Их `spRenderNode Sky_Head` не повторяет
material, тогда как правильный ранний binding `sky` находится у родительского
`spRenderNode Sky`; глобальный fallback ошибочно выбирал поздний `spe_body`.

Для непосредственных детей `spSkin` resolver теперь поднимается по цепочке
render-node и выбирает ближайший предшествующий явный binding. Для rigid
`spModel` это правило не применяется, поэтому mouth/eye-cap meshes не получают
чужую текстуру. Результат: `66/68 → sky`, оба eye-ball mesh сохраняют
`bloomeye_testd`, пять поздних body meshes используют `spe_body`.

Целевая модель: PASS 344 assertions. Регрессии: Prince `outfit_02` — PASS 337;
Bloom `outfit_01/02/03` — PASS 327/385/327; выборка 10 SMO с seed `20260808` —
PASS 1042 assertions.

## `prince_dating_outfit_01.smo`, светотень лица

Meshes `131` и `133` правильно используют atlas `sky` и layout `0x097E`.
Нормали декодируются как единичные, но diffuse содержит соответственно 242 и
95 разных сохранённых цветов, включая почти чёрные значения. Это готовая
vertex-светотень модели. Гипотеза о лишнем WPF directional lighting проверена,
но unlit-режим для WPF оказался непригоден из-за alpha-смешивания emissive.

Также выявлено перекрытие UV с разными diffuse: 312 из 350 вершин mesh `131` и
101 из 118 вершин mesh `133` входят в конфликтующие UV-группы. Старый tint bake
перезаписывал texel последним треугольником. Теперь вклады пересекающихся
треугольников усредняются. Проверенный эксперимент с unlit `EmissiveMaterial`
отменён: alpha атласов превращала его в светящийся полупрозрачный материал.
Viewer сохраняет обычный `DiffuseMaterial`. Исходные mesh, texture, normals и
colors не изменяются.

Оставшиеся чёрные треугольники носа и рта оказались не ошибкой topology: в
каждом из meshes `131/133` найдено по шесть треугольников с ненулевой
геометрической площадью, но одинаковыми тремя UV
`(0.65321416, 0.46899855)`. Их diffuse лежит в нормальном серо-бежевом диапазоне,
а единственный texel `sky` в этой точке чёрный. Это vertex-color-only полигоны с
UV sentinel. Viewer теперь заменяет окрестность sentinel только в приватной
tinted-копии atlas на средний diffuse и принудительно непрозрачную alpha.

Целевая модель: 13 meshes, 10 texture bindings, PASS 342 assertions. Release-
сборка успешна; остальные Prince/Bloom dating и выборка 10 SMO прошли прежние
регрессии.

## `Amaryl.smo`, чёрный diffuse sentinel

Все шесть textured meshes имеют layout `0x097E`, но каждый diffuse во всех
вершинах равен `0xFF000000`: 288/288, 71/71, 103/103, 220/220, 131/131 и
448/448. Это отключённый vertex-color поток, а не нулевое освещение персонажа.
Viewer больше не умножает texture на полностью однородный чёрный diffuse.
Смешанные массивы, содержащие как чёрные, так и другие цвета, продолжают
модулироваться для ресниц и ночных моделей.

Целевая модель: 6/6 meshes используют `amaryl`, PASS 243 assertions;
Release-сборка успешна. Prince outfit 01,
Bloom outfit 03 и выборка 10 SMO проходят без регрессий.

## `bloom_bike.smo`, тёмные глаза

Mesh `23` правильно использует `bloomeye`, но все его вершины имеют один diffuse
`0xFF202020`, что при ошибочной модуляции затемняло глаза примерно в восемь раз.
Остальные пять meshes модели также однородны, но заполнены `0xFF000000`.
Следовательно, значимый vertex-color поток определяется вариативностью RGB, а
не просто ненулевым значением. Viewer теперь игнорирует любой однородный diffuse
при texture tint; смешанные и градиентные потоки остаются активными.

Целевая модель: 6 meshes, bindings `b_bike` 5 и `bloomeye` 1, PASS 275
assertions. Amaryl, Prince outfit 01, Bloom outfit 03 и выборка 10 SMO проходят
без регрессий; Release-сборка успешна.

## `bloom_crystal.smo`, animated sparkle material

Mesh `93` ранее оставался без texture из-за десяти слоёв в одном material.
Объекты `82`, `84…92` — кадры `sparkles0001…0010`; объект `83` класса
`0x16FB0E47` содержит кадры 2–10 и префикс с 38 временными ключами. Последний
ключ равен примерно `1.266667 s`, поэтому десять кадров переключаются каждые
`0.126667 s` (около 7.9 fps).

Core теперь возвращает animated texture binding с кадрами и временем кадра.
Viewer заранее создаёт материалы кадров и циклически меняет их через общий
render tick. Effect binding не участвует в material inheritance: `93` получает
sparkle sequence, а `95/97/99/101/103` сохраняют основной `b_crysta`.

Целевая модель: 9/9 textured meshes, 10 animation frames, PASS 300 assertions.
Bloom bike, Amaryl, Prince outfit 01, Bloom outfit 03 и выборка 10 SMO проходят
без регрессий; Release-сборка успешна.

После визуальной проверки выяснилось, что sparkle sequence является вторым
texture stage, а не самостоятельным diffuse: при прямом выводе её чёрный фон
закрывал зелёную основу. Layout `0x197E` подтверждён как двухканальный: UV1 лежит
по `+56` и у mesh `93` постоянен `(0.035156, 0.464844)`; для `0x1940` UV1 лежит
по `+36`. Animated binding теперь дополнительно сохраняет ближайший
предшествующий `b_crysta` как base texture. Кадры запекаются аддитивно поверх
базы по UV1.

Выбор объекта мышью больше не заменяет animated material подсветкой: связанный
узел подсвечивается в дереве, а эффект продолжает обновляться во время
взаимодействия с камерой и моделью.
