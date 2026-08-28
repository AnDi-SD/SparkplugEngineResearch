# Итог полного разбора классов SMO

Первоначальная цель исследования выполнена: для всех 36 встреченных class ID
восстановлены наблюдаемые формы ресурсов, serializer-секции наследования, поля,
подвиды и связи отдельно для PC и PlayStation 2. Документ сохраняет выполненную
последовательность и критерии evidence; он больше не является очередью работ.
Непроверенная runtime-семантика и безопасность изменения вынесены в
[`../../research/open-questions.md`](../../research/open-questions.md) и
[`smo-runtime-validation-plan.md`](smo-runtime-validation-plan.md).

## Источники по умолчанию

Каждый класс исследуется сразу по совокупности источников обеих платформ:

1. все экземпляры PC-класса в отдельных корпусах `pc-pristine` и `pc-working`;
2. все уникальные PS2-ресурсы и все их физические вхождения в PCK-контейнеры;
3. pristine PC `WinxClub.exe` от 14 марта 2006 года: регистрация класса,
   serializer, фабрики и код использования;
4. PS2 `SLES_532.19`: MIPS-код загрузки, serializer и платформенные ветки;
5. соседние файлы того же ресурса или уровня на обеих платформах: прежде всего
   SAN, ANM, SPT, SPL, STX, SNC и SFT, затем конфигурационные файлы и шейдеры;
6. изменённые executable как сравнительный источник, но не как основание
   утверждения об оригинальной игре;
7. контролируемая мутация и запуск соответствующей версии игры, когда
   статического доказательства недостаточно.

Совпадение номера поля само по себе не является доказательством семантики:
Sparkplug повторно использует номера в разных serializer-секциях наследования.
Большой payload также может содержать inline-объект или список объектов, поэтому
его размер нельзя автоматически считать признаком отдельного подвида.

## Доступные корпуса

| ID корпуса | Платформа | Состояние | Наблюдаемый вход |
|---|---|---|---|
| `pc-pristine` | PC | исходный | 416 SMO, pristine `WinxClub.exe` |
| `pc-working` | PC | изменяемый | 416 SMO, текущая исследовательская установка |
| `ps2-pristine` | PS2 | исходный | `SLES_532.19`, 78 PCK |

PC-корпуса нельзя объединять: при одинаковом числе SMO их содержимое различается.
Полный manifest v2 обнаружил 29 изменённых SMO, хотя агрегаты классов, объектов и
полей совпадают. Оба корпуса проиндексированы отдельно.

Индексы 78 PS2 PCK структурно читаются без извлечения файлов: 12 229 записей,
0 нарушений проверенных границ. В них находятся 1 481 физическое вхождение SMO,
4 324 SAN, 4 100 VAG, 863 SPT, 607 STX, 290 ANM, 261 SNC, 82 SPL и 20 SFT.
Одинаковый логический путь или одинаковое содержимое может повторяться в разных
stage-архивах. После дедупликации остаётся 317 уникальных PS2 SMO, поэтому число
1 481 нельзя напрямую сравнивать с 416 PC-файлами.

## Этап 0 — платформенная база до разбора классов — завершён

27 августа 2026 года рядом с сохранённой v1-базой построена schema v2 и
проиндексированы все три корпуса. Реализованная модель:

1. `platforms` — стабильные ключи `pc`, `ps2` и сведения об архитектуре;
2. `corpora` — `pc-pristine`, `pc-working`, `ps2-pristine`, происхождение,
   executable, состояние `pristine/working` и manifest/hash;
3. `containers` — обычный каталог либо PCK с путём, SHA-256 и размером;
4. `files` — уникальная версия логического ресурса внутри корпуса; ключ включает
   корпус, нормализованный путь и hash содержимого;
5. `file_occurrences` — каждое физическое размещение файла: обычный путь либо
   PCK, индекс записи, offset и size;
6. `executables` и `evidence` — платформа, hash, архитектура и точный locator
   свидетельства: file offset/virtual address, PCK entry или соседний ресурс;
7. платформенный scope у `field_definitions` и `class_variants`: `common`, `pc`,
   `ps2` либо конкретный корпус;
8. `resource_pairs` и `object_pairs` — явные PC/PS2-сопоставления с методом,
   уверенностью и доказательством.

`objects` и `direct_fields` не дублируют строковую метку платформы: она однозначно
получается через `file -> corpus -> platform`. Платформенные views должны считать
отдельно физические вхождения, уникальные логические пути и уникальные SHA-256.
Так повтор одного SMO в нескольких PCK не раздувает статистику вариантов, но его
фактическое использование уровнями не теряется.

Для PCK нужен managed reader в `SmoViewer.Corpus`, проверяемый теми же условиями,
что существующий `research/winx_sparkplug_pck.bms`: `offset = sector * 0x800` и
`offset + size` внутри архива. SMO передаётся в `SmoDocument.Parse` прямо из
среза PCK; обязательное массовое извлечение на диск не требуется.

После миграции выполняются три независимых inventory, а затем общий отчёт:

- классы и поля `pc-pristine`;
- классы и поля `pc-working`, включая явный diff от pristine;
- классы и поля `ps2-pristine`;
- общие, PC-only и PS2-only class ID, поля и структурные формы.

Миграция выполнена без изменения рабочей v1-базы на месте:

1. создать рядом `smo-corpus-v2.sqlite` с новой схемой;
2. зарегистрировать текущий источник как `pc-working` и получить контрольные
   416 файлов, 177 369 объектов и 1 112 916 прямых полей;
3. независимо проиндексировать `pc-pristine` и сохранить manifest различий;
4. добавить 78 PCK и 12 229 `file_occurrences`, дедуплицируя ресурсы по
   нормализованному пути и SHA-256;
5. разобрать уникальные PS2 SMO и проверить конфликты corpus platform против
   platform mask внутри FFPS;
6. агрегаты v1/v2 совпали, SQLite integrity check вернул `ok`; v2 назначена
   основной исследовательской базой, а v1 оставлена восстанавливаемым локальным
   артефактом.

Путь корпуса является первичным доказательством происхождения. Внутренний
platform mask сохраняется как отдельное наблюдаемое поле: его несоответствие
корпусу создаёт диагностику, а не молча меняет метку платформы.

Итог этапа: 36 классов на PC, 32 на PS2, из них 32 общих, четыре PC-only и ни
одного PS2-only. PC-only: `spMaterialColorController`, `spFont`,
`spTextRenderable`, `spTextNode`. Все 317 уникальных PS2 SMO и все 416 SMO в
каждом PC-корпусе разобраны без ошибок. Подробные числа и команды приведены в
[`smo-corpus-database.md`](smo-corpus-database.md).

## Частота классов

Таблица ниже сохраняет исходный порядок по числу уникальных объектов
`pc-working` без оценки сложности. Полный v2-индекс публикует независимые рейтинги для всех трёх
корпусов; суммарный рейтинг PC+PS2 не используется, потому что PCK содержат
повторные физические вхождения ресурсов.

| № | Класс | Объекты | SMO |
|---:|---|---:|---:|
| 1 | `spModel` | 40 555 | 311 |
| 2 | `spMaterialData` | 36 334 | 412 |
| 3 | `spMeshData` | 22 649 | 396 |
| 4 | `spStaticRenderObject` | 20 469 | 29 |
| 5 | `spNode` | 17 841 | 416 |
| 6 | `spRenderNode` | 14 064 | 413 |
| 7 | `spPartitionNode` | 5 254 | 29 |
| 8 | `spCollisionInfo` | 3 643 | 194 |
| 9 | `spMeshBV` | 3 609 | 95 |
| 10 | `spTextureData` | 2 564 | 373 |
| 11 | `spMaterialColorController` | 2 391 | 31 |
| 12 | `spPartitionRenderable` | 2 324 | 29 |
| 13 | `spUVController` | 1 607 | 186 |
| 14 | `spSkin` | 748 | 117 |
| 15 | `spOctreeNode` | 731 | 11 |
| 16 | `spParticleSystem` | 619 | 147 |
| 17 | `spLightData` | 514 | 170 |
| 18 | `spFog` | 413 | 413 |
| 19 | `spZonePortal` | 206 | 18 |
| 20 | `spOBBBV` | 148 | 97 |
| 21 | `spZone` | 123 | 30 |
| 22 | `spMeshNavigationSet` | 119 | 27 |
| 23 | `spBSPNode` | 108 | 18 |
| 24 | `spZonePortalNode` | 103 | 18 |
| 25 | `spNavigationPortal` | 70 | 16 |
| 26 | `spSkyBox` | 43 | 22 |
| 27 | `spPartitionSystem` | 29 | 29 |
| 28 | `spNavigationGraph` | 27 | 27 |
| 29 | `spOcclusionVolume` | 20 | 8 |
| 30 | `spTextRenderable` | 10 | 1 |
| 31 | `spFont` | 10 | 1 |
| 32 | `spTextNode` | 10 | 1 |
| 33 | `spAnimTexController` | 9 | 6 |
| 34 | `spLensFlare` | 2 | 2 |
| 35 | `spBoxBV` | 2 | 2 |
| 36 | `spSphereBV` | 1 | 1 |

## Выполненная последовательность исследования

Порядок был основан на PC-частоте и зависимостях. Этап 0 не обнаружил PS2-only
классов. Каждый номер ниже означает выполненный совместный межплатформенный
разбор, а не две несвязанные задачи.

### Этап 1 — простые и хорошо сравнимые формы

1. [`spMaterialColorController`](smo-class-sp-material-color-controller.md) —
   **завершён структурный/read-only разбор**: 2391 PC-экземпляр, один 54-байтовый
   layout, пять evaluator-секций и одно значение payload. PS2 executable
   поддерживает тот же serializer, но PS2-ресурсы класс не используют. Безопасная
   мутация намеренно остаётся открытой.
2. [`spFog`](smo-class-sp-fog.md) — **завершён структурный/read-only разбор**:
   1 140 объектов трёх корпусов, один общий 31-байтовый layout, два наблюдаемых
   подтипа (`none`/`linear`) и 22 payload. Все 314 одноимённых PC/PS2-пары
   совпадают побайтно; безопасная мутация остаётся открытой.
3. [`spOBBBV`](smo-class-sp-obbbv.md) — **завершён структурный/read-only
   разбор**: 423 объекта трёх корпусов, один общий 23-байтовый layout и 80
   векторов полного размера. Все 97 PC-пар и 87 PC/PS2-пар ресурсов совпадают;
   необязательные position/rotation подтверждены обоими executable, но в
   корпусе не встречаются. Безопасная мутация остаётся открытой.
4. [`spUVController`](smo-class-sp-uv-controller.md) — **завершён
   структурный/read-only разбор**: 4 705 объектов трёх корпусов, семь evaluator,
   UV pivot и rotation axis. Одиннадцать размеров оказались разреженными формами
   одного layout; 148 полных payload, 186/186 равных PC-пар и 157/162 равных
   PC/PS2-пар. Безопасная мутация остаётся открытой.
5. [`spLightData`](smo-class-sp-light-data.md) — **завершён структурный/read-only
   разбор**: 1 482 объекта трёх корпусов, девять optional-полей с общими PC/PS2
   defaults и четыре типа света. Directional, point и ambient наблюдаются; spot
   подтверждён executable, но в корпусе отсутствует. Все 170 PC-пар и 124 общие
   PC/PS2-пары ресурсов совпадают по собственной секции.
6. [`spBoxBV`](smo-class-sp-box-bv.md) — **завершён структурный/read-only
   разбор**: шесть объектов трёх корпусов, один общий 23-байтовый layout и два
   вектора полного размера. Оба PC-ресурса и обе PC/PS2-пары совпадают побайтно;
   optional position подтверждена обоими executable, но в корпусе отсутствует.
   Loader обеих платформ выводит half-extents и sphere radius из full size.
7. [`spSphereBV`](smo-class-sp-sphere-bv.md) — **завершён
   структурный/read-only разбор**: три уникальных объекта трёх корпусов, один
   общий 14-байтовый radius-only layout и значение `92.0507889`. Единственная
   PC-пара и единственная PC/PS2-пара совпадают побайтно; optional position
   подтверждена обоими executable, но в корпусе отсутствует.

### Этап 2 — основа object/render graph

8. [`spNode`](smo-class-sp-node.md) — **завершён структурный/read-only разбор**:
   51 396 объектов трёх корпусов, один общий девятиполевый serializer-контракт,
   228 614 декодированных полей, 89 926 child- и 432 collision-отношения.
   Подтверждены defaults transform/flags, legacy PC ID-only child-ссылка и
   ненаблюдаемый field 6 billboard axis. Нормализованно совпадают 387/416 PC-пар
   и 288/317 PC/PS2-пар; остальные различаются содержимым графа, а не layout.
9. [`spRenderNode`](smo-class-sp-render-node.md) — **завершён
   структурный/read-only разбор**: 38 443 объекта трёх корпусов и единый
   двухсекционный PC/PS2 serializer, состоящий из наследованной секции `spNode`
   и повторяемого `esfRenderNodeRenderable`. Декодированы 183 630 полей и 53 057
   renderable-связей с `spModel`, `spSkin`, `spParticleSystem` и `spLensFlare`;
   подтверждены пустые списки, максимум 27 элементов и три различные legacy
   PC ID-only связи. Совпадают 413/413 PC-пар и 190/314 PC/PS2-графов.
10. [`spTextureData`](smo-class-sp-texture-data.md) — **завершён структурный/
    read-only разбор**: строго декодированы все 7 485 объектов трёх корпусов,
    пять storage-вариантов, cross-platform/Direct3D BGRA и PS2 formats 0/1/3 с
    палитрами и 1–9 mip-уровнями. Исправлена старая ошибка: `0x32E3` и похожие
    значения являются байтами field header/размера, а не pixel format. Native
    PS2 swizzle-preview и безопасная мутация остаются открытыми.
11. [`spMaterialData`](smo-class-sp-material-data.md) — **завершён полный
    структурный/read-only разбор наблюдаемого PC/PS2 layout**: строго
    декодированы 105 588 уникальных материалов, 736 236 прямых полей и 108 601
    проход. Подтверждены четыре legacy/current × single/multi-pass варианта,
    11 material states, 9 texture states, цвета, static UV и четыре типа
    object relationship. Из 28 793 надёжно сопоставленных PC/PS2-пар 28 067
    имеют одинаковое нормализованное собственное состояние; mutation safety и
    ненаблюдаемые executable-only layer sources остаются открытыми.
12. [`spMeshData`](smo-class-sp-mesh-data.md) — **завершён полный структурный/
    read-only разбор наблюдаемых контейнеров и PC geometry layouts**: все 66 191
    уникальных mesh трёх корпусов строго разложены на семь вариантов и 87 330
    полей. Подтверждены cross-platform E0, Direct3D E1, native PS2 header/DMA
    boundary, AABB, 13 Direct3D vertex layouts и обе топологии. Обнаружены 629
    native PS2 mesh в пяти `_ps2.smo` каждого PC-корпуса; PS2 DMA/VIF streams и
    mutation safety остаются read-only research.
13. [`spModel`](smo-class-sp-model.md) — **завершён полный структурный/read-only
    разбор наблюдаемого PC/PS2 layout**: все 118 720 моделей строго разложены на
    унаследованную секцию `spRenderable` и собственную секцию `spModel`, шесть
    semantic fields, 705 928 прямых полей и семь вариантов присутствия.
    Material/fog/base-mesh relationships проверены по target class; два legacy
    PC-объекта используют ID-only форму. Совпадают 311/311 PC-ресурсов; надёжно
    сопоставлены 32 067 PC/PS2-моделей. `ProjectionGroup` runtime semantics и
    mutation safety остаются открытыми.
14. [`spStaticRenderObject`](smo-class-sp-static-render-object.md) — **завершён
    полный структурный/read-only разбор наблюдаемого PC/PS2 layout**: все 61 851
    размещение имеют Transform, engine InvTransform и один inline `spModel`.
    Подтверждена необычная формула `transpose(A)`/`-T*transpose(A)`, из-за которой
    field 2 не является общим математическим inverse при scale. Аннотированы
    185 553 поля, назначен один вариант; writer исправлен для authored scale.
15. [`spSkin`](smo-class-sp-skin.md) — **завершён полный структурный/read-only
    разбор наблюдаемого PC/PS2 layout**: строгий decoder разложил все 1 758 skin
    на секции `spRenderable`, `spModel` и `spSkin`, аннотировал 12 213 полей и
    назначил три варианта присутствия. Подтверждены 16 PC/64 PS2 palette slots,
    40 704 affine/invertible inverse-bind matrices и полное согласие повторов
    одной кости. Первое слово palette оказалось не reserved: ненулевые 1..4 во
    всех 66 декодируемых PC-копиях точно равны максимуму blend influences на
    вершину. Mutation safety остаётся открытой.

На этом этапе сначала восстанавливаются собственные поля и правила ссылок, затем
inline/child payload. Сырые сочетания полного размера и списка полей здесь дают
тысячи сигнатур главным образом из-за вложенного содержимого и не равны тысячам
подвидов класса.

### Этап 3 — коллизии и пространственное разбиение

16. [`spCollisionInfo`](smo-class-sp-collision-info.md) — **завершён полный
    структурный/read-only разбор наблюдаемого PC/PS2 layout**: все 10 594 объекта
    строго декодированы, 31 286 полей аннотированы и распределены по трём
    вариантам присутствия. Подтверждены 10 162 `spMeshBV`, 423 `spOBBBV`, шесть
    `spBoxBV` и три `spSphereBV`, четыре legacy ID-only primitive relationship,
    значения group 1/2 и 10 108 конечных transform.
17. [`spMeshBV`](smo-class-sp-mesh-bv.md) — **завершён полный
    структурный/read-only разбор наблюдаемого PC/PS2 layout**: все 10 513
    объектов строго декодированы и распределены по geometry-only/with-face-data
    вариантам. Подтверждены 291 065 треугольников, 245 554 вершины и 106 832
    разреженные записи `wxFaceData`; восстановлены surface type 0..9, а точные
    числа flags/surface ID сохранены без вымышленных значений.
18. [`spPartitionRenderable`](smo-class-sp-partition-renderable.md) — **завершён
    полный структурный/read-only разбор наблюдаемого PC/PS2 layout**: все 7 208
    объектов строго декодированы, 27 197 полей аннотированы и назначен один
    вариант. Подтверждены обязательный ARGB `DebugColor`, 19 989 повторяемых
    inline physical-child relationships на `spModel` и cardinality 1..67.
    Единственное различие общих PC/PS2 ресурсов — две дополнительные модели в
    PS2 `Alfea03`; формат общий.
19. [`spPartitionNode`](smo-class-sp-partition-node.md) — **завершён полный
    структурный/read-only разбор наблюдаемого PC/PS2 layout**: строго
    декодированы все 16 204 узла и 232 035 отношений, аннотированы 248 239
    полей. Подтверждены семь именованных отношений, единый порядок serializer,
    74 324 inline physical children. Field 2 `Child` отсутствует у точного класса,
    но его индексированный формат найден в унаследованной секции `spOctreeNode`.
20. [`spOctreeNode`](smo-class-sp-octree-node.md) — **завершён полный
    структурный/read-only разбор PC/PS2 layout**: строго декодированы 2 256
    объектов, 24 816 отношений и 33 840 полей. Каждый узел содержит восемь
    inline children со slots 0..7, `Pivot`, `Mins` и `Maxs`; восстановлена
    битовая нумерация октантов X/Y/Z. Все 731 общих PC/PS2-узла семантически
    совпадают.
21. [`spPartitionSystem`](smo-class-sp-partition-system.md) — **завершён полный
    структурный/read-only разбор PC/PS2 layout**: строго декодированы все 88
    систем, 5 411 отношений и 5 587 полей. Подтверждены три serializer-секции,
    один owned `spZone`, ссылки на zones/collisions, inline portal nodes и
    обязательный полиморфный `PartitionRoot`: 54 inline `spBSPNode` либо 34
    ссылки на `spOctreeNode`. Все 29 общих PC/PS2-графов совпадают; связанный
    owned `spZone` разобран в следующем пункте.
22. [`spZone`](smo-class-sp-zone.md) — **завершён полный структурный/read-only
    разбор PC/PS2 layout**: строго декодированы 369 именованных zones, 412 owned
    inline local roots и 1 231 поле. Подтверждены две секции `spNode + spZone`,
    Position, optional Rotation/Static, обязательный `Animated=false` и 0..4
    полиморфных `LocalPartitionRoot`: 378 `spPartitionNode` и 34 `spOctreeNode`.
    Совпадают 122/123 PC/PS2-графа; единственное отличие — rootless PC против
    octree-root PS2 в `Gardenia03`. Связанный portal graph разобран далее.
23. [`spZonePortal`](smo-class-sp-zone-portal.md) — **завершён полный
    структурный/read-only разбор PC/PS2 layout**: строго декодированы 618
    объектов и 1 854 поля. Подтверждены обязательные `DestinationZone`,
    четырёхвершинный `Polygon` и `Open=true`, две ownership-формы destination и
    103 двунаправленные пары на корпус. Во всех 309 парах winding polygon точно
    обращён; все 206 PC/PS2-объектов совпадают семантически. Связанные portal
    nodes разобраны далее.
24. [`spZonePortalNode`](smo-class-sp-zone-portal-node.md) — **завершён полный
    структурный/read-only разбор PC/PS2 layout**: строго декодированы 309
    объектов, 618 portal-ссылок и 1 305 полей. Подтверждены две секции
    `spNode + spZonePortalNode`, три формы optional node-полей и фиксированный
    порядок `BackToFront`, затем `FrontToBack`. Все пары обменивают source и
    destination zones и имеют точно обратный polygon winding; Position является
    независимым placement-полем. Все 103 PC/PS2-пары совпадают семантически,
    88 совпадают побайтно; в остальных меняются только object IDs ссылок.
25. [`spBSPNode`](smo-class-sp-bsp-node.md) — **завершён полный
    структурный/read-only разбор PC/PS2 layout**: строго декодированы 324
    split-узла в 54 полных двоичных деревьях, 648 child-полей и 2 268
    содержательных полей. Подтверждены slots 0/1, 270 inline BSP edges, 378
    terminal references, обязательная нормализованная Plane и executable-only
    optional Polygon. Все 108 PC/PS2-пар совпадают семантически; геометрические
    имена сторон slots пока намеренно не назначены.
26. [`spOcclusionVolume`](smo-class-sp-occlusion-volume.md) — **завершён полный
    структурный/read-only разбор PC/PS2 layout**: строго декодированы 60 объектов
    и 282 содержательных поля. Подтверждены секции `spNode + spOcclusionVolume`,
    обязательные portable triangle-list/UInt16 IndexBuffer и position-only
    VertexBuffer, четыре V/T cardinality и один общий variant. Все 20 PC/PS2-пар
    совпадают побайтно. Все формы — связные planar disks; 54/60 строго выпуклые,
    а остальные шесть повторяют один слегка вогнутый десятиугольник несмотря на
    convexity-сообщения executable.

### Этап 4 — навигация

27. [`spMeshNavigationSet`](smo-class-sp-mesh-navigation-set.md) — **завершён
    полный структурный/read-only разбор PC/PS2 layout**: строго декодированы 355
    объектов и аннотированы 3 254 поля. Подтверждены три serializer-секции,
    `NodeCount`, обе UInt32 routing matrices, byte-addressed adjacency, repeated
    portals, `Enabled` и relationship к `spMeshBV`. Все 393 496 достижимых
    node-маршрутов и 14 371 portal-маршрут валидны; out-of-degree значение 3
    является терминатором. Явный graph содержит по 32 authored non-edge links и
    четыре отключённых shared-edge directions на corpus snapshot. Core semantics
    совпадают у всех 117 PC/PS2-пар; два test-world объекта существуют только на
    PC.
28. [`spNavigationPortal`](smo-class-sp-navigation-portal.md) — **завершён полный
    read-only разбор PC/PS2**: 209 объектов, две endpoint sets, node pairs и все
    8 044 обратные membership-записи проверены против графов; найдены три
    relationship-storage variants.
29. [`spNavigationGraph`](smo-class-sp-navigation-graph.md) — **завершён полный
    read-only разбор PC/PS2**: 80 объектов, 2 507 строк квадратных path tables,
    1 282 reachable routes и 2 344 alternatives топологически проверены без
    циклов и согласованы с portals.

### Этап 5 — специализированный рендер и эффекты

30. [`spSkyBox`](smo-class-sp-sky-box.md) — **завершён полный read-only разбор
    PC/PS2**: 126 объектов, три model-count variants и все 159 inline `spModel`.
31. [`spParticleSystem`](smo-class-sp-particle-system.md) — **завершён полный
    read-only разбор PC/PS2**: 1 745 объектов, fields 0..19 и семь точных
    emission-region layouts.
32. [`spAnimTexController`](smo-class-sp-anim-tex-controller.md) — **завершён
    полный read-only разбор PC/PS2**: 28 controllers, три storage variants и все
    974 timed texture frames.
33. [`spLensFlare`](smo-class-sp-lens-flare.md) — **завершён полный read-only
    разбор PC/PS2**: все шесть объектов, compound element/glare, occlusion и
    render-node fields.

### Этап 6 — редкая связанная текстовая подсистема

34. [`spFont`](smo-class-sp-font.md) — **завершён полный PC read-only разбор и
    PS2 executable verification**: 20 объектов, atlas ownership и все 4 480
    glyph records.
35. [`spTextRenderable`](smo-class-sp-text-renderable.md) — **завершён полный PC
    read-only разбор и PS2 executable verification**: inherited renderable,
    inline font, UTF-16LE text, ARGB и optional wrap/alignment.
36. [`spTextNode`](smo-class-sp-text-node.md) — **завершён полный PC read-only
    разбор и PS2 executable verification**: 20 полных
    node/renderable/font/atlas chains.

Три текстовых класса встречаются только в одном SMO и должны исследоваться как
единый связанный граф, хотя результаты фиксируются для каждого class ID отдельно.

Итог очереди: все 36/36 встреченных SMO-классов имеют строгий структурный
read-only decoder, corpus annotations, variants и evidence. Для классов без
экземпляров в PS2 SMO статус платформы ограничен честной независимой проверкой
class/serializer в PS2 executable.

## Условие завершения класса

Класс считается максимально разобранным на текущем материале, когда в базе
зафиксированы отдельно для PC и PS2, а затем сопоставлены:

- собственные serializer-секции и наследование;
- все наблюдаемые номера полей, occurrence, размеры и cardinality;
- подтверждённый тип значения либо честный статус opaque;
- ссылки, inline-объекты и допустимые классы-цели;
- реальные структурные подвиды и правило их различения;
- распределение по файлам и характерные крайние примеры;
- свидетельства из executable и соседних форматов;
- статус чтения и отдельный статус безопасного изменения.

Дополнительно обязательно указываются:

- `common`, `pc-only`, `ps2-only` или `platform-different` для каждого вывода;
- число уникальных ресурсов и число физических PCK-вхождений;
- прямые PC/PS2-пары и причина сопоставления без опоры только на object ID;
- отсутствие класса или поля на платформе как проверенный отрицательный
  результат, а не как пропущенные данные.

Неизвестное поле не блокирует переход дальше: оно остаётся явной записью с
неподтверждённым layout и набором примеров, а не исчезает из отчёта.

## Следующий этап

Последовательный class-by-class разбор завершён. Дальнейшая работа выполняется
поперёк классов по runtime-системам: loader/object identity, animation binding,
render states, collision/navigation, effects и GUI. Практический порядок:
[`smo-runtime-validation-plan.md`](smo-runtime-validation-plan.md).
