# Текущий срез ядер tools — 10 сентября 2026

**Все ядра ещё не готовы.** Ниже учтены Font, Static matrix authoring,
Text metadata, WinxHairPatcher, уменьшение памяти ResourceGraph, общий
TextureTool header и исправление временных файлов SanToVmd. Цикл до 07:30 МСК продолжается;
[его журнал](tools-core-cycle-2026-09-10-0730.md) фиксирует следующие результаты.
PC TextureData теперь использует actual source reader; PS2 native metadata
перенесена отдельно, без заявления готовности PS2 runtime. Legacy source
оболочки и подключение material clock остаются в работе.
Material reader TextureTool теперь использует общий snapshot, включая field8
и constructor defaults; [70+14 адресных checks](tool-texture-material-inspection-2026-09-10.md).
Первый узкий renderer-срез не подтверждён: выбранный BloomX требует нескольких
passes, а однопроходный Darcy материал принадлежит ParticleSystem.

[GPU skinning и picking](tool-gpu-skinning-shared-picking-2026-09-10.md)
исправлены по original Fixed.rfx: удалены две CPU-копии деформации; Viewer и
LVLcreator читают позиции общего shader. Viewer сохраняет occurrence identity;
27 GPU checks и43 позы/419 независимых сравнений прошли. GLB теперь явно
отказывает для неподтверждённой конверсии существенно non-unit весов,
сохраняя поддерживаемые файлы побайтно. Это не закрывает multipass/material
output; [PC fallback producer](tool-renderer-fallback-material-boundary-2026-09-10.md)
ещё не установлен, отдельный PS2 producer не подставляется вместо него.
[Предпросмотр подгонки Importer](tool-importer-fitting-gpu-2026-09-10.md)
тоже использует общий GPU shader и native spSkin composition; отдельный CPU
skin evaluator удалён.24 адресные GPU проверки и Core provider contract прошли.
Неподтверждённая конверсия non-unit весов теперь явно отклоняется также
в [FBX](tool-fbx-skin-weight-boundary-2026-09-10.md); обычный output сохранён.

Таблица описывает операции ядер семи приложений. Общие native bridges не
считаются отдельными приложениями; готовность UI и выпуск учитываются отдельно.

| Приложение | Проверенные операции ядра | Конкретный остаток или граница проверки |
| --- | --- | --- |
| Viewer | Общий SMO graph и SAN; actual Node/Model/Skin/material/texture связи и render slots; spatial, Light, Sphere/Box/OBB; общие Font и Text/TextNode metadata inspectors; PC TextureData source dispatch с выбранным представлением и PS2 native metadata | Legacy-common/PS2 source оболочки и material clock ещё в работе; специальные render passes не завершены. Полные Text/TextNode runtime readers и layout не реализованы; metadata inspection их не заменяет. Сохраняются общие границы загрузки и runtime ниже. |
| Exporter | Поза и parents из actual Node; общая geometry, Model variants и placements в GLB/FBX/OBJ; общий SAN | Полная проекция material passes/layers в целевые форматы ещё не закрыта. Ограничения общего graph loader распространяются на экспорт; новый полный acceptance Exporter в этом цикле не заявлен. |
| Importer | Actual Model/Material selection; перенос ID по подтверждённому reader trace; общие mesh/texture writers, Material scalar/LTS, Renderable sort/priority, Skin palette с настоящими Node owners; Static matrix writer | FAT/envelope writer остаётся приостановленным предложением. Multipass donor отклоняется как `MATERIAL_IMPORT_SHAPE`; неизвестный DX power мешает полному Material writer. Legacy/orphan/repeated LTS и неоднозначные scalar/palette assignments требуют отдельного определения операции. |
| LVLcreator | Workspace сохраняет actual support slots; общие scene/mesh/collision/pose операции, reference trace и Static matrix authoring; worker передаёт общий каталог один раз | Собственная сборка контейнера ещё не перенесена. Команды отклоняют повторные slots как `REPEATED_RENDERABLE_AUTHORING`; нужна адресация конкретного слота. Пользовательские отсрочки inverse-world и lossless-форм перечислены ниже. |
| TextureTool | Общие texture sections, codecs, writers, header reader и material snapshot; PC preview использует actual source selection. Проверены три PNG и шесть вариантов замены на одном Bloom_body; stale material reference диагностируется | FAT/envelope остаётся отдельным writer. Legacy-common/PS2 source оболочки и полный PS2 runtime не завершены. Реальный barelegacy TextureData с pixels без source-wrapper ещё не поддерживается whole graph. Выбранные acceptance не являются проверкой всего корпуса. |
| SanToVmd | Общие SMO/SAN graph и native pose sampler; VMD/PMD conversion остаётся кодом целевого формата. Исправлена коллизия временных файлов, два новых и 18 существующих converter tests прошли. Один полный Icy/xiid acceptance: независимый reader подтвердил 51 кадр и 2601 ключ, включая позы | Ограничения входного ResourceGraph сохраняются; один acceptance не является полной проверкой корпуса или визуальным воспроизведением в MMD. |
| WinxHairPatcher | Операция сигнатурной EXE patch проверена по прежнему original evidence; коллизия backup-имён исправлена, 38 assertions и build прошли | Для проверенной файловой операции нового блокера не найдено. Визуальная проверка всех игровых комбинаций остаётся прежней границей. Patch bytes и файловый IO являются собственной операцией инструмента, а не копией игровой симуляции. |

## Общие незавершённые случаи

- **Запись контейнера:** три FAT/envelope writers ещё отдельные. Предложение
  [общего host encoder](tool-container-writer-boundary-2026-09-10.md)
  ожидает решения пользователя; существующий общий producer меняет lossless
  контракт и не может быть молча подставлен вместо них.
- **Платформа импорта текстур:** создание PC TextureData в legacy-common/PS2
  контейнере теперь явно отклоняется до записи. [Обычный PC путь проверен](tool-importer-texture-destination-2026-09-10.md);
  преобразование контейнера в другую платформу остаётся отдельным предложением.
- **Входной graph:** прежняя формулировка о доказанном пустом source-less
  TextureData была неточной. У проверенного `blooming_flower` pixels есть,
  отсутствует source-wrapper; original caller возвращает неинициализированный
  объект и читает32 байта родительской секции. [Platform dispatch исправлен](tool-texture-platform-dispatch-2026-09-10.md),
  но whole-file принятие original игрой не установлено и guards сохранены.
  Реальный fixture SourceNone с действительно пустой local section не найден.
  Cached1848 `marble` в Alfea01 содержит полноценные pixels и
  совпадает с ранее прочитанным695; независимый source inspector это подтвердил.
  [Остаток1848 относится к authoring coverage](tool-reference-inspector-cached-texture-2026-09-10.md),
  а не к отсутствующему pixel reader. Reader trace не объявляет skipped payload
  прочитанным. Palette authoring требует подтверждённого покрытия нужных диапазонов.
- **Память:** [временная копия файла удалена](tool-resource-graph-memory-2026-09-10.md).
  На выбранном Alfea02 пик уменьшился на 16,33%; пределы 64 МиБ и числа
  объектов сохранены. Поддержка файлов свыше 64 МиБ этим не доказана.
  [Запрос существующего индекса](tool-indexed-loader-boundaries-2026-09-10.md)
  не выявил входа свыше 10,9 МБ; выбранный Domino04 загрузился с пиком
  42,55 МиБ. Максимальный по object count race_02 требует OcclusionVolume,
  увеличение лимитов этот отказ не исправит.
- **Runtime:** полный Occlusion Init, защищённый PC MaterialColor constructor,
  looping particle initialization и повторная непустая NavigationGraph table
  остаются прежними ограничениями. Исследовать их только для нужной операции;
  работа над material clock сама по себе эти контракты не закрывает.
- **Отложено пользователем до LVLcreator:** cached 120/physical 84, редкие
  lossless headers и inverse-world при nonuniform parent. Эти решения не
  отменены переносом отдельных writers.
- **PC/PS2 Corpus:** девять старых combined rerun-профилей остановлены guard
  до записи БД. Нужны раздельные operations/evidence; новый PC reader не
  доказывает PS2 equivalence, исторические результаты сохранены.

## Что больше не является прежней очередью

[Spatial](tool-spatial-inspection-shared-core-2026-09-10.md),
[Light](tool-light-inspection-shared-core-2026-09-10.md),
[Sphere/Box](tool-simple-bv-shared-core-2026-09-10.md) и
[OBB](tool-obb-inspection-shared-core-2026-09-10.md) уже подключены к общим
классам. [Font и Static matrix writer](tool-font-static-shared-core-2026-09-10.md)
и [Text metadata](tool-text-shared-inspection-2026-09-10.md) также перенесены.
Они не должны повторно попадать в очередь как отсутствующие операции.
Полный runtime Text остаётся отдельной незавершённой функцией.
PC TextureData source и PS2 native-section inspection также не следует
повторно считать отсутствующими; остающиеся source/runtime границы перечислены выше.

Число **30 из 35** относится только к историческому reader-срезу предыдущего
цикла. После новых блоков полный пересчёт этого набора здесь не выполнялся;
общий процент готовности ядер не вводится. Подробные прежние ограничения:
[итог к 23:00 9 сентября](tools-core-cycle-report-2026-09-09-2300.md).
