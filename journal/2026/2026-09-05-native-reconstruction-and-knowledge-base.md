# 2026-09-05 — native-реконструкция Sparkplug и постоянная база знаний

## Вопрос

Как перестать восстанавливать поведение движка набором удобных собственных
эвристик и перейти к проверяемой реконструкции исходных подсистем, классов и
путей Sparkplug — при этом не теряя накопленные знания между исследовательскими
сессиями и не смешивая движок с логикой Winx Club?

## Корпус и исходное состояние Git

- родительский commit основного репозитория: `f22e745`;
- исходный commit SmoViewer: `70a2a38`;
- исходный commit SMOTextureTool: `5a9ee7d`;
- pristine PC executable SHA-256:
  `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`;
- pristine PS2 executable SHA-256:
  `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE`;
- рабочая SQLite-база: `local-data/results/smo-corpus-v2.sqlite`, schema v5.

Исполняемые файлы, игровые ресурсы, IDA-базы, временные сборки и сама SQLite-БД
остаются локальными и исключены из Git. В репозиторий входят только схема,
манивесты, воспроизводимые инспекторы, реконструированный код и документация.

## Метод

1. Из registration graph PC и PS2 извлечены оригинальные class names, class/base
   hashes, registration objects и граница `sp...`/`wx...`.
2. Строки исходных путей, vtable, factories, clone/copy/destructor bodies и
   consumers полей сопоставлялись с SMO/SAN-корпусом и runtime-наблюдениями.
3. Подтверждённые layouts вынесены отдельно в `Analysis/PC` и `Analysis/PS2`;
   переносимые классы создавались под найденными именами и исходными каталогами.
4. Для каждого восстановленного участка добавлялся read-only inspector с
   закреплёнными executable SHA/body hashes либо portable unit test.
5. В corpus SQLite добавлен native research layer. Первичное `sync` строит
   каталог из EXE, а последующие `import-manifest` и `report` работают только с
   сохранённой БД и не повторяют дорогой анализ executable.
6. После первых циклов стратегия изменена с выбора самых лёгких классов на
   логический dependency front. Приоритет — классы прямого SMO/SAN closure и
   зависимости, необходимые importer/exporter; PS2 откладывается, если PC-код
   не создаёт неоднозначность.

## Наблюдение

### Архитектура и дерево исходников

- PC регистрирует 733 типа, PS2 — 681; объединённый каталог содержит 784 типа:
  373 относятся к Sparkplug, 411 — к application-слою Winx.
- Созданы раздельные корни `Sparkplug/` и `Winx/`. На этом срезе имеются 112
  native-карточек, 248 `.h/.cpp` файлов Sparkplug и шесть файлов Winx.
- Подтверждены исходные модули `SparkBase`, `SparkBasePC`, `SparkBasePS2`,
  `Sparkplug`, `SparkplugDX`, `SparkplugPC`, `SparkplugPS2`.
- Engine RTTI нельзя механически считать физическим C++ inheritance. Например,
  `spAnimation` объявляет `spController` базой в registration graph, но имеет
  несовместимую семислотовую vtable и завершается через `spBaseObject` body.

### Общие подсистемы

Восстановлены проверяемые срезы object/RTTI/property system, stream и file/PCK
layer, serializer manager/FAT/data-block layer, resource cache, application и
engine lifecycle, scene graph, camera/light/material, mesh/texture buffers,
platform renderer boundaries и соответствующие concrete serializers. Полный
перечень, степень уверенности и неизвестные поля находятся в
[`native-reconstruction-plan.md`](../../docs/research/native-reconstruction-plan.md)
и [`native-open-questions.md`](../../docs/research/native-open-questions.md).

### Ближайший SMO/SAN-фронт

- `spSkin`: PC extent `0x70`, bone palette и inverse-bind matrices, lifetime,
  copy/remap и передача palette renderer-у; `spSkinSerializer` совпал с corpus
  grammar чтения/записи.
- `spAnimation`: подтверждены track stride `0x44`, track/tag ownership и
  lifetime. Native serializer читает поля `0..12` и control field `64`, поэтому
  существующий SAN decoder полей `0..4` является корректным core subset, но не
  полной заменой native serializer.
- `spController`/`spSubController`: подтверждены абстрактные vtable и описанная
  выше граница engine RTTI против физического layout.
- `spTransformTrackEval`: factory выделяет `0x78`; `+0x10` хранит binding slot,
  `+0x14` — число входов, далее находятся два блока по `0x30`. Полное тело
  `0x005FEBB0` вычисляет/смешивает PRS, но активный игровой tick этим маршрутом
  пока не пойман.

### Постоянная оценка покрытия

Schema v5 хранит `native_types`, scopes, progress, атомарное evidence, immutable
imports и coverage snapshots. После baseline и четырёх incremental manifests:

| Область | Оценка | Интервал | Числитель / знаменатель |
|---|---:|---:|---:|
| Весь executable | 13,10% | 11,09–15,06% | 102,705 / 784 |
| Sparkplug | 24,78% | 21,78–27,78% | 92,430 / 373 |
| Winx | 2,50% | 1,50–3,50% | 10,275 / 411 |
| Прямые SMO/SAN-типы | 44,54% | 39,57–49,57% | 16,480 / 37 |

Это экспертно взвешенная оценка изученности классов, а не процент
декомпилированных байтов. Runtime-only `spTransformTrackEval` меняет общий и
engine-показатель, но не искусственно расширяет прямой знаменатель 37
сериализованных SMO/SAN-типов.

## Проверка перед фиксацией

- полный локальный аудит документации: 273 файла и 619 локальных ссылок;
- UTF-8, ссылки, tabs/trailing whitespace: ошибок нет;
- C++ `SparkBaseTests` и `SparkplugEngineTests`: 2/2;
- Winx tests: 1/1;
- на SDK 8.0.420 успешно собраны 27 из 28 .NET-проектов; единственная
  ожидаемая граница — GUI `SMOTextureTool` с генераторами Avalonia 12.1,
  требующими SDK 9.0.300 или новее; это требование исправлено в README;
- WinxHairPatcher: 28 assertions; SmoLVLcreator: 1 682 assertions;
- SmoNativeValidator: 296 assertions; SmoViewer FormatTests: 579 synthetic и
  1 465 assertions на локальном SMO;
- SmoExporter: 4 проверки native FBX path и 59 проверок экспорта;
- SmoImporter: normal/resource safety, alpha, rigid-level, quality guard и
  статическая замена прошли; неподходящий fixture для отдельного
  shared-texture сценария не считается результатом формата;
- native inspectors: `spSkin` 21/21, animation/controller 27/27,
  `spTransformTrackEval` 14/14;
- SQLite: schema 5, 5 imports, 1452 evidence rows, 20 snapshots, ноль битых
  coverage rows и ноль orphan evidence.

## Вывод

Проект перешёл от коллекции отдельных догадок о форматах к воспроизводимой
реконструкции исходной архитектуры. Знания теперь имеют два независимых слоя:
факты о ресурсах/корпусе и факты о native-классах/коде. История evidence и
проценты сохраняются в БД, а неизвестные имена и поля не замещаются удобными
догадками.

## Что не подтвердилось или не закончено

- У игры не найден внешний родственный движок, способный заменить реверс
  имеющихся executable.
- Ещё нет полного end-to-end пути `logical request → draw` с provenance одного
  объекта во всех промежуточных caches/managers.
- Не пойман caller/scheduler активного `spTransformTrackEval` tick и перенос
  его local PRS в world matrix `spNode`.
- Расширенные SAN fields `5..12`, часть original names `spBaseObject` property
  ABI и многие Winx application-классы остаются открытыми.
- Отложенный PS2-анализ не считается отрицательным результатом: он подключается,
  когда нужен для общего ABI или снятия PC-неоднозначности.

## Следующий эксперимент

На PC найти caller/scheduler `spTransformTrackEval::Evaluate`, активировать
штатный animation tick, записать PRS до и после применения к `spNode`, затем
проверить заполнение уже восстановленной `spSkin` palette. После этого вернуться
к полям SAN `5..12` и к следующим P1-контроллерам из DB-backed очереди.
