# Платформенный индекс ресурсов SMO

Локальная SQLite-база сохраняет результаты полного разбора PC- и PS2-корпусов,
чтобы исследование классов, вариантов и полей не требовало повторного чтения всех
SMO. Это производный артефакт в `local-data/results/smo-corpus-v2.sqlite`: он
исключён из Git вместе с игровыми данными и не публикуется.

Однокорпусная база `smo-corpus.sqlite` сохранена как контрольный и
восстанавливаемый артефакт. Основной исследовательский индекс мигрирован без
потери объектов и evidence на schema v3, scanner revision 4.

## Состояние на 28 августа 2026 года

| Корпус | Ресурсы, уникальные версии | Физические вхождения | SMO разобрано | Объекты | Прямые поля | Классы |
|---|---:|---:|---:|---:|---:|---:|
| `pc-pristine` | 4 277 | 4 277 | 416/416 | 177 369 | 1 112 916 | 36 |
| `pc-working` | 4 221 | 4 221 | 416/416 | 177 369 | 1 112 916 | 36 |
| `ps2-pristine` | 5 806 | 12 229 | 317/317 | 160 387 | 1 037 225 | 32 |

Ошибок разбора SMO нет. В 78 PS2 PCK находится 1 481 физическое вхождение SMO,
но после дедупликации одинаковых логических путей и содержимого остаётся 317
уникальных ресурсов. Поэтому статистика вариантов строится по уникальным
ресурсам, а отдельный счётчик физических вхождений показывает реальное
использование в stage-архивах.

Из 36 class ID 32 встречаются на обеих платформах. Только на PC найдены:

- `spMaterialColorController`;
- `spFont`;
- `spTextRenderable`;
- `spTextNode`.

PS2-only классов пока нет. Отсутствие класса не означает отсутствие функции:
при разборе PC-only класса обязательно ищется альтернативное PS2-представление.

После накопления анализов и миграции размер базы — 1 273 171 968 байт.
`PRAGMA integrity_check` 28 августа 2026 года возвращает `ok`. Контрольный
повторный проход без изменений пропускает все
ресурсы: около 2 секунд для каждого PC-корпуса и 2,7 секунды для всех PS2 PCK на
текущей машине; это контрольные замеры, а не универсальный benchmark.

Холодная копия после schema-v3 migration, полного `analyze-all`, `AUDIT PASS` и
`integrity_check=ok` сохранена как
`local-data/results/smo-corpus-v3-20260828-post-header.sqlite`, SHA-256
`D35FFE3C33B6F288AA47AB8471E2FB8BC7A65F5D633A0074A72E14925EBDEE09`.
Основная база затем получила первую отдельную runtime-evidence запись о точном
PC lookup имени `spNode`; её SHA-256 после импорта —
`7A021F93C1EBA0D82443EA303B678431F17989961619FDC79E7DD96BFA12A078`.
Одинаковый размер файлов не означает одинаковое содержимое SQLite pages.

## Контейнеры и платформенные метки

Managed PCK-reader проверяет строковую таблицу, границы каждой записи и равенство
`byteOffset = sectorOffset * 0x800`. SMO читаются непосредственно из диапазона
PCK без обязательного извлечения на диск. Полная инвентаризация 78 архивов дала
12 229 корректных записей и ни одной ошибки границ.

Платформа корпуса (`pc` или `ps2`) хранится отдельно от platform mask ресурса в
заголовке FFPS:

- bit `0x01` — common;
- bit `0x02` — PC;
- bit `0x08` — PS2;
- одновременно PC- и PS2-биты — mixed.

Несовпадение этих значений является диагностикой, а не ошибкой разбора и не
переписывает происхождение файла. В обоих PC-корпусах есть пять штатных
PS2-targeted menu SMO: `igmenu_diary_ps2`, `igmenu_fash_ps2`,
`igmenu_opt_ps2`, `mmenu_cont_ps2` и `mmenu_new_ps2`. PC-семантика полей к ним
автоматически не применяется.

## Обновление и запросы

Команды выполняются из `tools/SmoViewer` либо через собранный
`SmoViewer.Inspect`:

```powershell
dotnet run --project SmoViewer.Inspect -- pck-inventory <pck-directory> --parse-smo

dotnet run --project SmoViewer.Inspect -- research-db update-directory `
  <database.sqlite> pc-pristine pc pristine <Media-directory> <WinxClub.exe>

dotnet run --project SmoViewer.Inspect -- research-db update-directory `
  <database.sqlite> pc-working pc working <Media-directory> <WinxClub.exe>

dotnet run --project SmoViewer.Inspect -- research-db update-pck `
  <database.sqlite> ps2-pristine ps2 pristine <pck-directory> <SLES_532.19>

dotnet run --project SmoViewer.Inspect -- research-db summary <database.sqlite>
dotnet run --project SmoViewer.Inspect -- research-db headers <database.sqlite>
dotnet run --project SmoViewer.Inspect -- research-db classes <database.sqlite>
dotnet run --project SmoViewer.Inspect -- research-db class `
  <database.sqlite> spMaterialColorController
dotnet run --project SmoViewer.Inspect -- research-db analyze-class `
  <database.sqlite> spMaterialColorController
dotnet run --project SmoViewer.Inspect -- research-db analyze-class `
  <database.sqlite> spFog
dotnet run --project SmoViewer.Inspect -- research-db analyze-class `
  <database.sqlite> spOBBBV
dotnet run --project SmoViewer.Inspect -- research-db analyze-all <database.sqlite>
dotnet run --project SmoViewer.Inspect -- research-db resources `
  <database.sqlite> <path-substring>
dotnet run --project SmoViewer.Inspect -- research-db conflicts <database.sqlite>
dotnet run --project SmoViewer.Inspect -- research-db compare `
  <database.sqlite> pc-pristine pc-working
dotnet run --project SmoViewer.Inspect -- research-db integrity <database.sqlite>
dotnet run --project SmoViewer.Inspect -- research-db import-evidence `
  <database.sqlite> <evidence.json>
```

К каждой команде чтения можно добавить `--json`. В JSON-сравнении сохраняется
полный manifest различий; текстовый режим показывает группировку и первые 50
путей.

`import-evidence` принимает JSON-массив записей с optional class/platform/corpus
scope, видом evidence, source path/hash, locator, observation и confidence.
Повторный импорт той же комбинации class/kind/source/locator идемпотентно заменяет
строку. Tracked runtime-источники —
`docs/research/evidence/smo-name-case-20260828.json`: подтверждённый PC
case-sensitive lookup `R_Ankle/r_Ankle` с executable hash и locators двух JSONL
прогонов; и `docs/research/evidence/smo-name-missing-duplicate-20260829.json` с
подтверждённой loader-tolerance для missing parent/leaf и duplicate-key collapse
в обоих порядках.

Инкрементальный directory scan использует нормализованный путь, размер, время
изменения и ревизию scanner; SHA-256 пересчитывается для реально прочитанного
ресурса. Неизменённый PCK пропускается целиком по размеру и времени изменения,
сохраняя все зарегистрированные occurrences. Повышение scanner revision
принудительно перечитывает SMO, но не хеширует заново соседние типы файлов.

## Различия двух PC-корпусов

В `pc-pristine` и `pc-working` соответственно 4 277 и 4 221 путей: 4 217 общих,
3 192 побайтно одинаковых, 1 025 изменённых, 60 только в pristine и 4 только в
working. Среди изменённых — 29 SMO, 854 WAV, 67 MP2 и 63 M1V. Поэтому
агрегаты 416 SMO / 177 369 объектов / 1 112 916 полей служат контролем parser,
но не доказывают тождество содержимого корпусов.

## Схема v3

В `files` больше не используется ошибочное имя `ffps_version` для слова
`0x10`. Заголовок хранится в трёх отдельных столбцах:

- `serializer_version` — `0x04`, во всём корпусе `0x26`;
- `ffps_unknown08` — `0x08`, исследуемый export/session tag candidate;
- `platform_mask` — `0x10`, маска common/PC/PS2.

Миграция v2 → v3 перенесла старое значение `ffps_version` в `platform_mask`.
Fast-upgrade для неизменённых revision-3 SMO обновляет header metadata без
DELETE/INSERT уже проверенных `objects`, `direct_fields` и evidence.

| Таблица | Назначение |
|---|---|
| `platforms` | стабильные платформенные ключи и архитектуры |
| `corpora` | источник, provenance, состояние pristine/working и платформа |
| `executables` | hash, размер и архитектура связанного executable |
| `scans` | история полных и инкрементальных обновлений |
| `containers` | каталог либо PCK, его размер, время и SHA-256 |
| `files` | уникальная версия ресурса, hash, три FFPS header-поля и результат разбора |
| `file_occurrences` | обычный файл либо конкретные PCK index/offset/size |
| `classes` | class ID, engine name и статус знания |
| `objects` | объект, иерархия, имя, offsets, размер и структурная сигнатура |
| `direct_fields` | serializer-секция, номер, offsets, размер и ограниченный preview/hash |
| `class_variants` | варианты класса со scope `common`, `pc`, `ps2` или corpus |
| `object_variant_assignments` | привязка экземпляров к вариантам с уверенностью |
| `field_definitions` | семантика, layout, редактируемость и платформенный scope |
| `evidence` | источник утверждения и точный locator |
| `resource_pairs` | явные межкорпусные/межплатформенные пары ресурсов |
| `object_pairs` | сопоставления объектов с методом, уверенностью и evidence |

Полный payload не копируется в базу. Для поля хранится не более 48 первых байт;
для payload до 4096 байт дополнительно сохраняется SHA-256. Исходный SMO остаётся
единственным источником полных байтов.

Исследовательские views:

- `corpus_file_type_counts` — типы ресурсов, уникальные версии и occurrences;
- `corpus_class_totals` — число объектов, SMO и физических вхождений по корпусу;
- `platform_class_presence` — common, PC-only и PS2-only классы;
- `class_variant_candidates` — кандидаты по размеру и форме прямых полей;
- `field_shape_counts` — распределение serializer-секций, номеров и размеров.

Совпадение формы является кандидатом на вариант, но не доказательством
семантики. Названия координат, поворотов, ссылок и других изменяемых значений
добавляются в `field_definitions` лишь после проверки layout и безопасной
мутации; до этого статус остаётся `read_only_research`.

## Следующий шаг

Этап 0 и первые двадцать шесть read-only разборов — `spMaterialColorController`, `spFog`,
`spOBBBV`, `spUVController`, `spLightData`, `spBoxBV`, `spSphereBV`, `spNode` и
`spRenderNode`, а также `spTextureData`, `spMaterialData`, `spMeshData` и
`spModel`, `spStaticRenderObject`, `spSkin`, `spCollisionInfo`, `spMeshBV` и
`spPartitionRenderable`, `spPartitionNode`, `spOctreeNode`, `spPartitionSystem`,
`spZone`, `spZonePortal`, `spZonePortalNode`, `spBSPNode` и `spOcclusionVolume` — завершены. Model analyzer повторно читает
directory/PCK-источники
и строго проверяет 118 720 моделей, 705 928 прямых полей, семь вариантов
присутствия, все material/fog/base-mesh targets и 32 067 надёжно сопоставленных
PC/PS2-пар. `spStaticRenderObject` также завершён: analyzer повторно прочитал
61 851 размещение, аннотировал 185 553 поля, назначил один inline-model вариант
и подтвердил transpose-basis семантику `InvTransform`. `spSkin` также завершён:
analyzer строго прочитал 1 758 объектов, аннотировал 12 213 полей, назначил три
варианта и проверил 40 704 inverse-bind matrices. PC palette имеет 16 слотов,
PS2 — 64; ненулевой blend-influence hint 1..4 совпадает с максимумом активных
weights во всех 66 декодируемых PC-копиях. `spCollisionInfo` также завершён:
анализатор строго перечитал 10 594 объекта, аннотировал 31 286 полей, назначил
три варианта и проверил все Primitive/Group/Transform формы. Подтверждены 10 590
inline и четыре legacy ID-only primitive relationship; все targets разрешены в
10 162 `spMeshBV`, 423 `spOBBBV`, шесть `spBoxBV` и три `spSphereBV`.
`spMeshBV` теперь тоже завершён: analyzer повторно прочитал 10 513 объектов,
аннотировал 14 116 полей и назначил geometry-only/with-face-data варианты.
Подтверждены 291 065 треугольников, 245 554 вершины, 106 832 разреженные записи
`wxFaceData` и общий PC/PS2 layout. `spPartitionRenderable` также завершён:
analyzer повторно прочитал 7 208 объектов, аннотировал 27 197 полей и назначил
один общий вариант. Подтверждены один ARGB `DebugColor`, 19 989 inline
physical-child `spModel`, диапазон 1..67 моделей на объект и одно содержательное
PC/PS2-отличие в `Alfea03`. `spPartitionNode` analyzer повторно прочитал все
16 204 узла, проверил 232 035 отношений и аннотировал 248 239 direct fields.
Подтверждены один общий PC/PS2 layout и 74 324 inline physical children. Field 2
`Child` отсутствует у точного класса, но его настоящий формат найден в
унаследованных секциях `spOctreeNode`. `spOctreeNode` analyzer повторно прочитал
2 256 объектов, проверил 24 816 отношений, аннотировал 33 840 direct fields и
назначил один общий вариант. Подтверждены slots 0..7, 18 048 inline children,
`Pivot/Mins/Maxs` и битовая нумерация октантов. `spPartitionSystem` analyzer
повторно прочитал все 88 систем, проверил 5 411 отношений, аннотировал 5 587
direct fields и назначил один общий трёхсекционный variant. Подтверждены пустая
унаследованная renderable-секция, zones/portals/collisions и обязательный root:
54 inline `spBSPNode` либо 34 ссылки на `spOctreeNode`. `spZone` analyzer
повторно прочитал 369 объектов, проверил 412 owned inline local roots,
аннотировал 1 231 direct field и назначил один общий двухсекционный variant.
Подтверждены Position, optional Rotation/Static, `Animated=false`, 378 roots
`spPartitionNode` и 34 roots `spOctreeNode`. `spZonePortal` analyzer повторно
прочитал 618 объектов, проверил 618 destination relationships, 2 472 вершины и
309 portal-пар, аннотировал 1 854 поля и назначил один общий variant.
Подтверждены две ownership-формы destination, обязательные quadrilateral и
`Open=true`, а также точное обратное winding в каждой паре. `spZonePortalNode`
analyzer повторно прочитал 309 объектов, проверил 618 упорядоченных portal-ссылок,
аннотировал 1 305 полей и назначил один общий variant. Все 309 пар идут в порядке
`BackToFront`, затем `FrontToBack`, обменивают source/destination zones и имеют
обратный winding; все 103 PC/PS2-пары совпадают семантически. `spBSPNode`
analyzer повторно прочитал 324 split-узла, проверил 54 полных двоичных дерева,
648 child-полей и точную формулу `101 * split count`, аннотировал 2 268 полей и
назначил один общий variant. Подтверждены 270 inline BSP edges, 378 terminal
references, нормализованная Plane и отсутствующий в корпусе, но поддержанный
обоими executable optional Polygon. Все 108 PC/PS2-пар совпадают семантически,
91 — побайтно. `spOcclusionVolume` analyzer повторно прочитал 60 объектов,
проверил portable index/vertex buffers и planar topology, аннотировал 282 поля
и назначил один общий variant. Все 20 PC/PS2-пар побайтно совпадают; 54/60 форм
строго выпуклые, а остальные шесть повторяют один слегка вогнутый десятиугольник.
`spMeshNavigationSet` analyzer повторно прочитал 355 объектов, аннотировал 3 254
поля, назначил 351 inline-mesh и четыре PC test-world reference assignments и
записал четыре evidence rows. Подтверждены `NodeCount=triangleCount`, 23 552
reciprocal directed links, 393 496 корректных node routes, 14 371 завершающихся
portal routes и terminal/unreachable marker 3. У всех 117 PC/PS2-пар совпадают
core navigation semantics; 108 пар совпадают полностью сериализованными bytes.
После `spMeshNavigationSet` последовательно завершены все девять оставшихся
классов. `spNavigationPortal`/`spNavigationGraph` добавили полный межобъектный
navigation topology audit; `spSkyBox`, `spParticleSystem`,
`spAnimTexController`, `spLensFlare` — полные effect/render layouts;
`spFont`, `spTextRenderable`, `spTextNode` — полный четырёхуровневый text graph.
Итоговая очередь закрыта: 36/36 встреченных SMO class IDs имеют подтверждённый
read-only разбор и данные в базе.

Финальный read-only аудит воспроизводится отдельно:

```text
python research/audit_smo_corpus.py local-data/results/smo-corpus-v2.sqlite
```

Он проверяет 36 наблюдаемых known/read-only классов, отсутствие parse/field
errors, а для финальной девятки — object/assignment/variant/evidence counts и
нулевое число неаннотированных содержательных полей. Текущий результат:
1 149/1 149 SMO parsed, 40 633/40 633 полей финальной девятки аннотированы,
`AUDIT PASS`.
`research-db class` формирует воспроизводимый отчёт по ресурсам, формам, полям,
иерархическим связям и межплатформенным same-path-кандидатам. Поддерживаемый
`analyze-class` дополнительно перепроверяет executable/corpus и идемпотентно
записывает варианты, назначения и evidence. Выполненная последовательность
36 классов сохранена в [`smo-class-analysis-plan.md`](smo-class-analysis-plan.md).
Дальнейшие обновления базы приходят из поперечных runtime-проверок по
[`smo-runtime-validation-plan.md`](smo-runtime-validation-plan.md): результат
фиксируется в evidence только вместе с build, resource hash, field locator и
воспроизводимым способом запуска.

## Известный технический долг annotations

В `direct_fields` остаются 2 584 содержательных строки `spLightData` с
`is_decoded=0`. Все их реально встреченные field types входят в известный набор
`0,1,2,4,5,6,7,8`; определения fields `0..8`, payload layouts и defaults уже
подтверждены executable и class analyzer. Причина — неоднозначный выбор между
общим scope `pc_ps2` и более старым `platform=pc` определением, а не неизвестный
payload.

Исправление должно:

1. детерминированно предпочитать актуальное definition нужного scope;
2. повторно аннотировать все Light-поля;
3. оставить ненаблюдаемый field 3/attenuation со статусом
   `executable_confirmed_unobserved`, а не создавать фиктивные observations;
4. закончиться нулём неаннотированных содержательных `spLightData` fields,
   `integrity_check=ok` и повторным `AUDIT PASS`.
