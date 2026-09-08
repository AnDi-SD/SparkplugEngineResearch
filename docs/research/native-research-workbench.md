# PC-first workbench и независимый учёт PC/PS2

Текущая цель: [необходимые контракты для инструментов](tool-driven-research-scope.md).
Постоянные правила: [манифест исследования](research-manifesto.md).
По уточнению 8 сентября default queue показывает активные операции софта;
полное исследование движка сохранено отдельно. Реализован отдельный профиль
`pc-skin-lit-generation-128k`; [результаты опыта](native-research-strategy-2026-09-07.md).
Общий разрешённый бюджет RAM — около 1 ГиБ на все исследовательские процессы;
существующие ограничения отдельных утилит описаны ниже и сами не меняются.

PC остаётся главным направлением. Активный PS2-срез — serializer-вопросы
нужной операции; spatial/collision/navigation runtime и симуляция частиц
сохранены в deferred backlog. Связи `companionPcItems` указывают PC-задачи, которым полезно
сопоставление. Они не являются `dependsOn` и не переносят evidence или scores.
Планирование двух платформ и подбор коротких выборок описаны в манифесте;
автоматического планировщика PC/PS2 и универсального cache результатов нет.
У новых PS2-задач пока пустые `testProfiles`: сначала требуется выбрать или
создать подходящие bounded проверки. Добавление задачи не означает её выполнение.

Обновление CP26–27: каждый CLI `run` сохраняет отдельный generated JSON в
`local-data/results/bounded-native-runs/`, обновляя его после каждого child.
Статусы passed/failed/deadline различают полный успех и остановку; записаны
состав, коды завершения, время, хэши запускаемых скриптов. Report schema2
фиксирует hash **переданной конфигурации на старте**, а не меняющегося файла
очереди после каждого child. Два ранних schema1-отчёта имели `workItemsSha256`
текущего файла на момент записи; этот hash не доказывает startup configuration.
Отчёт не начисляет coverage, не доказывает полную binary provenance и не
повторяет failed/capped probes. Сами источники evidence и class manifests
сохраняются отдельно по прежним правилам.

Введён в тестовом цикле6 сентября2026 до12:00 МСК. Цель — меньше повторять
поиск известных фактов и проверять общие зависимости SMO/SAN. Это оснастка
исследования, не ускоренный режим игры, не новый импортёр и не обещание
фиксированного ускорения реверса в несколько раз.

## Паспорт и очередь

```powershell
python research/native_workbench.py dossier spOctreeNode --platform pc
python research/native_workbench.py dossier spOctreeNode --platform ps2 --json
python research/native_workbench.py queue --platform pc
python research/native_workbench.py queue --platform ps2
python research/native_workbench.py queue --platform pc --include-deferred
```

Паспорт берёт из SQLite уже зарегистрированные ID/base/registration locator,
scope counts, доказательства и независимую platform assessment. EXE и игровые
payload для этого **не сканируются**. Историческая mixed оценка подписана
отдельно. Evidence `common` показаны как контекст, не как автоматически
доказанное поведение конкретной версии. Новые platform manifest refs входят
в паспорт даже без записи в старой mixed progress table.
`scopes` object/resource counts унаследованы от общей проектной базы:
`scopeCountMode=shared_project_inventory_not_platform_filtered` явно запрещает
читать их как число файлов только PC или только PS2. Независимые platform
assessments и знаменатели каталога считаются отдельно от этих corpus counts.

[native-work-items.json](../../research/native-work-items.json) — reviewed
22 участка: 19 PC и 3 PS2. Активны 6 PC и 1 PS2; остальные 15 отложены.
`planningStatus` управляет default queue; `status` сохраняет степень исследования.
У активных участков указаны инструменты, `requiredParts`, `deferredParts` и
`doneWhen`. Прежние scheduling dependencies и широкие profile lists сохранены
как исторический контекст. Профили не удалены и запускаются только явной командой.
Активный участок не может зависеть от скрытой deferred-задачи: validator
отклоняет такую конфигурацию. Список всех задач доступен через `--include-deferred`.
Первоначальные четыре: plane storage, concrete spatial
membership, occluder topology, native mesh submission. Очередь хранит ссылки,
проверенные факты, отдельные неизвестные **behavior/name/path/ABI**, наборы
проверок и исследовательский порядок. `dependsOn` здесь не означает доказанный
прямой CALL и не запрещает независимую проверку следующего участка.

Это пока не автоматический полный граф784 классов. Если work item отсутствует,
backlog ещё не структурирован; это не «неизвестного нет». Не создаются
догаданные vtables/fields/source paths или доказательства из одного совпадения
имени. Наличие оригинального пути allocator header не назначает его классу
плоскостей. Автоматическое создание C++ по декомпиляции не включено.

## Слои проверок

```powershell
python research/native_workbench.py run plane-storage-native --deadline-utc 2026-09-06T09:00:00Z
python research/native_workbench.py run plane-storage-differential --deadline-utc 2026-09-06T09:00:00Z
python research/native_workbench.py run spatial-static
python research/native_workbench.py run visibility-native
python research/native_workbench.py run membership-native
python research/native_workbench.py run spatial-differential
```

Runner по умолчанию запускает последовательные **fresh Python children**,30s
каждый, без shell и запуска игры. Original guest вызовы сохраняют явно выбранные
micro100k/2s или file1M/8s limits. [CP110](native-parallel-profiles.md) добавляет
`--workers 4` для reviewed `parallelSafe` профилей: после failed group следующая
не запускается, результаты сохраняются после группы. Sequential failure/timeout
останавливает профиль сразу. Deadline требует31s до запуска child/group.
Stopped guest state не продолжается; пределы не повышаются ради результата.

После изменения storage запускаются его directed checks и source differential;
затем соседние visibility/registration tests. Полный C++ CTest выполняется
после связного блока, изменения общего контракта или при закрытии цикла;
успешные проверки без новой причины не повторяются.
`spatial-differential` объединяет прежние Octree/BSP query, polygon clip и
visibility selection comparisons в9 независимых children. Это регрессия
соседних доказанных контрактов, не новый reverse credit. Для BSP ray значений
сохраняется допуск2e-6, для координат clipping3e-6; логические результаты
сравниваются точно. Эти допуски не распространяются на raw plane-storage bits.
Empty/null, resize/reuse, growth, raw flags, NaN/signed-zero, mask/Zone ветки
выбраны по различиям поведения. Это не доказательство покрытия всех SMO/SAN
файлов. Подбор representative **файлов** и synthetic branch cases следует
отчитывать отдельно.

Первый trial: [plane storage](native-pc-visibility-plane-storage.md) дал
resize/enable/release/copy-construction и outer-stack append,190 directed native
checks и17569 differential fields в64 последовательностях/576 операциях.
Общие45E870/46C0F0 при этом остались
честно незакрытыми. Число проверенных полей — не число классов и не процент
инструкций EXE.

Representative file pilot:

```powershell
python research/native_specimens.py select
python research/native_specimens.py verify --manifest research/native-specimens-pc-spatial-2026-09-06.json
python research/native_workbench.py run san-real-files
```

[Зафиксированный набор](../../research/native-specimens-pc-spatial-2026-09-06.json):
16 variant witnesses в11 pristine PC SMO,159 проверок file/object/field hashes;
не более32 hashed fields на witness. Это проверка неизменности и структуры,
**не вызов native SMO loader и не игра**.5 вариантов не выбраны:4 запроса
упёрлись в независимый SQLite budget,1 не имеет witness при текущих ограничениях.
Они не помечены проверенными/отсутствующими во всей игре.

Selector берёт существующие indexes, не создаёт новые;2s/2M VDBE budget
на variant query, file cap4MiB, verify16 files/32MiB memory. Первый type-first
join давал caps даже для обычных mesh witnesses; file-first indexed join
вернул16 witnesses без увеличения ограничений. Подбор по discriminator
полезнее случайных файлов, но не покрывает все форматы автоматически.
32MiB здесь — предел кэша исходных байтов, не всего Python-процесса.
Размер следующего файла проверяется до чтения; само чтение ограничено меньшим
из file/remaining aggregate budgets плюс один sentinel byte. Поэтому рост
файла после `stat` не превращается в неограниченный `read`. Verify, как и
selector, допускает не более32 field samples на witness.

Отдельно4 реальных SAN (`bbush`, `bflower`, `barrel`, `bw` из Animations)
прошли original native object reader + portable PRS comparisons:
234+780+909+909 = **2832** fields. Различаются число tracks, количество
position/rotation keys и pool/reserve hints. Это не полный FAT/file manager:
из проверенного single-object envelope в reader передаются исходные поля.

## Новые таблицы и метод процентов

После запроса на пересчёт основной отчёт расширен:
[аудит и рабочий контур v1](native-coverage-recalculation-2026-09-06.md).
`python research/native_goal_coverage.py report` заново считает all/engine/game,
исторические direct37 и новый SMO/SAN workflow, отдельно PC/PS2, и показывает
обязательные completion gates. Добавлены companion scope/member/snapshot tables.
Ниже сохранено описание прежнего учёта; его direct37 не является целью 100%.

Additive companion schema:

- `native_platform_imports`: immutable manifest ID/hash;
- `native_platform_assessments`: отдельная запись `(native_type_id,pc|ps2)`;
- `native_platform_snapshots` и `latest_native_platform_coverage`:4 scope ×2 platforms.

Схема ресурсной базы остаётсяv5. Старые31 research imports/124 snapshots
не переписываются, старые mixed проценты сохраняются как исторический ряд.
`native_knowledge.py report` показывает разницу; новый расчёт:

```powershell
python research/native_platform_knowledge.py report
python research/native_platform_knowledge.py report --json
python research/native_platform_knowledge.py import --manifest research/native-platform-wire-migration-2026-09-06.json
```

Для платформы и scope: **creditedPercent = сумма подтверждённых оценок / число
классов каталога**. Одному классу соответствует100 points. Это эвристическая
оценка изученности поведения, не byte/instruction coverage, не готовность tools.
Wire/serializer baseline25 сохранён из прежней методики; не означает, что
четверть runtime всех таких классов исполнена.

| Scope | PC denominator | PS2 denominator |
|---|---:|---:|
| все зарегистрированные типы |733|681|
| engine (`sp` prefix, прежнее правило классификации) |329|275|
| game (остаток каталога) |404|406|
| direct project SMO/SAN class set |37|37|

Последняя строка — общий исследуемый asset-related набор, пересечённый с
platform registration. Это **не утверждение**, что все37 типов встречены
в PS2 файлах: в частности, PS2 MaterialColorController в корпусе не обнаружен,
а полный PS2 SAN runtime пока не исследуется.

Пропущенная assessment остаётся **UNRATED**, а не «исследовано0%». Всегда
показываются `assessedCount`, `unratedCount`, denominator. При0 assessed CLI
пишет UNRATED вместо0%. Для неполной исторической миграции creditedPercent —
только накопленный зачёт по внесённым классам, **не полная историческая оценка
платформы**. `recordedLowerPercent/possibleUpperPercent` учитывают границы
оценок; unrecorded history даёт диапазон0..100, а не ложную узкую уверенность.

Нельзя делить прежние mixed15,4458% пополам или копировать их на каждую
платформу. Старые агрегаты включают знания, ещё не разложенные по классам/
платформам; отсюда меньший новый общий зачёт, а не потеря исследования.
На исходном этапе игровая логика отдельно ещё не была разнесена. Аудит 6 сентября
добавил две reviewed bootstrap-оценки на платформу; остальная неоценённая история
не превращается автоматически в 0% знания или в прежние mixed 2,5%.

### Миграция, не новый прогресс

- [Bootstrap](../../research/native-platform-bootstrap-2026-09-06.json):48
  latest PC-only class records C1–C20 + отдельный review3 прежних двуплатформенных
  dossiers (`spCrossPlatform`, `spStream`, `spResource`), всего54 assessments.
- [Wire migration](../../research/native-platform-wire-migration-2026-09-06.json):
 20 оставшихся PC direct records и35 **независимых PS2 serializer/wire** baselines.
 [Фиксированный экспорт evidence](../../research/native-platform-wire-evidence-2026-09-06.json)
 хранит IDs/locator/observation старой базы. PC runtime scores не переносились наPS2.
- PS2 MaterialColorController registration/vocabulary не повышен до wire25;
  spAnimation тоже остаётся unrated. Новых PS2 executable runs в этом цикле нет.

Перед новым исследовательским checkpoint: PC71 assessed, PS2 38 assessed;
PC direct37/37 =59,027027%; PS2 direct35/37 =23,648649% recorded credit,
2 unrated. Общая PC/PS2/game миграция пока неполна. Прирост от этих55/54
перенесённых записей не выдаётся за новые найденные функции.

Immutable imports, явный platform evidence, допустимые score bounds, порядок
timestamps, atomic rollback и независимость PC/PS2 проверяются unit tests.
Неизвестная версия manifest schema или пустой ID отвергаются до записей.
Для будущих checkpoints использовать `assessmentOrigin:new_research`; новое
поведение и изменение эвристического score должны иметь собственные ссылки.

## Проверка перед публикацией

```powershell
python research/audit_repository_docs.py
python -m unittest discover -s research -p 'test_*.py'
python research/audit_native_scope_dependencies.py
```

Первая команда проверяет UTF-8, JSON и относительные ссылки Markdown в составе
публикации, включая файлы закреплённых submodules. Remote URLs, heading anchors
и локальные evidence-файлы не проверяются. Путь в `local-data/` или
`.codex-tmp/` обозначает локальный артефакт, отсутствующий в GitHub checkout.
Native original/source profiles дополнительно требуют собственных pristine
игровых файлов и собранных source fixtures; они не являются частью Python
unit tests. Сборка и CTest описаны в [Sparkplug README](../../Sparkplug/README.md).
