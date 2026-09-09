# Цикл ядер tools до 07:30 МСК 10 сентября

Начало: 9 сентября 2026 около23:11 МСК. Состояние: **работа продолжается**.
Граница: 10 сентября07:30 МСК либо доступный лимит; отчёт обновляется по
проверенным блокам, чтобы пережить неожиданную остановку. Исходный checkpoint:
root3d483e0 / Viewer21b43a5. Все ядра пока не готовы.

Приоритет — законченные операции ядер на общих Sparkplug/Winx классах;
интерфейсы вторичны. Ошибки исправляются самостоятельно с доказательством
оригинального поведения для восстановленной логики. Разрешение исправлять
ошибки не отменяет согласование замены игрового алгоритма.

## Организация

- Узкое чтение известных файлов/символов, повторное использование evidence.
- Дополнительный агент только для конкретной независимой задачи; без
  пересекающихся широких аудитов и повторных сборок каждым участником.
- Адресные проверки после изменений, общая проверка связанного блока;
  успешные проверки без новой причины не повторять.
- Один накопительный журнал и компактные доказательства контрактов;
  commits после осмысленных проверенных блоков, без упаковки релиза.
- Около1ГиБ суммарной рабочей памяти; процессы пользователя не останавливать.

## Текущий блок

Font/Text metadata, Static matrix authoring и PC TextureData source перенесены
в общий код; PS2 native metadata подключена отдельно. Сохранение текущего
texture блока и следующая используемая операция ядра; material runtime
ограничен пока неподтверждёнными renderer defaults. Непроверенные реализации
не считаются результатом.

## Сохранённые ограничения

Полный список предыдущего цикла: [отчёт](tools-core-cycle-report-2026-09-09-2300.md).
FAT/envelope replacement остаётся предложением, не согласованной реализацией.
Source-less/cached TextureData, large-file memory profile и прежние runtime
границы остаются открытыми. Пользовательские отсрочки LVLcreator не отменены.
О новых нестандартных случаях сообщать сразу и продолжать независимую работу.

## Проверенные результаты

1. Около 23:35 МСК: [Font reader и Static matrix authoring](tool-font-static-shared-core-2026-09-10.md).
   Общий Font заменяет C# parser; два writer-потребителя подключены к actual
   StaticRenderObject serializer. Native 3 suites, Font original/source 3816B,
   Static original/ABI 5 случаев; managed Font 54, Static 50, общая проверка
   Viewer 609 (локальный Samples отсутствует, отдельное menu проверено).
   Viewer/Importer builds без warnings/errors. Все ядра ещё не готовы.
   Checkpoint: root `a9c15df`, Viewer `0c76126`.
2. WinxHairPatcher: проверена операция EXE patch по существующему
   [оригинальному evidence](bloom-hair-system.md). Дублирования игровой
   симуляции/serializer нет; это собственная операция изменения файла.
   Исправлена коллизия backup-имён при двух патчах в одну секунду: добавлен
   уникальный ID операции. Старый код провалил regression; исправленный прошёл
   **38 assertions**, build без warnings/errors. Игровой EXE только читался,
   запись проверялась на временных synthetic файлах. Визуальная проверка всех
   игровых комбинаций остаётся прежним ограничением, не результатом этого блока.
   [Локальные команды и evidence](../../local-data/results/tools-core-cycle-20260910-0730/hair-core/notes.md),
   [manifest](../../local-data/results/tools-core-cycle-20260910-0730/hair-core/manifest.json).
   Checkpoint: root `ca604e7`.
3. [Text metadata inspection](tool-text-shared-inspection-2026-09-10.md):
   удалены отдельные C# Text/inherited Renderable parsers, исправлены
   byte-string/UInt32 wrap, сохранены raw bytes и явная граница layout unavailable.
   TextNode делегирует общей RenderNode metadata-проекции; полного runtime
   TextNode reader это не означает. Native TextInspection/RenderNode прошли;
   managed 59 проверок с 20 Text/TextNode объектов menu; общий Viewer 609,
   Viewer и FormatTests builds без warnings/errors. Документация исправлена.
   Checkpoint: root `57a5218`, Viewer `00407c3`.
4. [Память ResourceGraph](tool-resource-graph-memory-2026-09-10.md): удалена
   полная временная копия входного SMO; один общий read-only host stream.
   На Alfea02 пик working set 44 359 680 → 37 113 856 байт (−16,33%).
   Объекты, узлы, trace и выбранная texture совпали, включая чтение после
   освобождения входа. Время одной пары 0,06326 → 0,07645с: ускорение не заявлено.
   Native BorrowedInput44/FullLoader/ReferenceReadTrace и managed Text59 прошли.
   Ограничения 64МиБ и числа объектов не повышались.
   Checkpoint: root `199f8b2`.
5. [TextureTool header и SanToVmd output](tool-texture-header-vmd-output-2026-09-10.md):
   удалён локальный header parser; подтверждённый OverflowException заменён
   отказом общего reader (19/19). На двух SMO прошла 71 проверка, три PNG
   совпали с исходной сборкой. Новый raw material snapshot не считается
   before/after DTO comparison. Legacy material path остаётся неполным.
   SanToVmd получил отдельный временный файл на операцию: два новых теста
   воспроизвели прежнюю коллизию и прошли после исправления; старые 18 tests прошли.
   TextureTool checkpoint: `e449096`.
   Root checkpoint: `7b6c76e`.
6. [Material reader TextureTool](tool-texture-material-inspection-2026-09-10.md):
   удалён второй parser состояний/pass/layer, общий native snapshot исправляет
   legacy field8 и пустые LTS getters. Старый DLL: 8 failures из70, новый70/70;
   отдельный NULL reassignment guard14/14. Focused71 и три PNG неизменны.
   UI получил только вывод MaterialIssue. Viewer/FormatTests собраны чисто;
   TextureTool GUI собран с двумя прежними CS9057 analyzer/toolchain warnings.
   Checkpoints: Viewer `1dc252a`, TextureTool `46e3cf2`, root `55160c9`.
7. SanToVmd: один полный сценарий Icy/xiid → Miku_Hatsune_Ver2 VMD
   проверен существующим независимым reader: **51 кадр, 51 дорожка, 2601 ключ**,
   три позы и десять сегментов тела. Максимальная ошибка направления
   `1,72e-6` при границе `0,002`. Конверсия и проверка заняли 0,242с,
   пик working set 28 028 928 байт; исходные и staged файлы неизменны.
   [Manifest](../../local-data/results/tools-core-cycle-20260910-0730/vmd-smoke/manifest.json),
   [отчёт](../../local-data/results/tools-core-cycle-20260910-0730/vmd-smoke/report.json).
   Это один адресный acceptance; визуальное воспроизведение MMD не проверялось.
8. [PC TextureData source](tool-pc-texture-source-inspection-2026-09-10.md):
   C# source dispatch заменён наблюдением actual DX reader. Сохраняются
   порядок, repeated fields, выбранное представление, stored/runtime mips
   и opaque skipped fields. Новый cache повторно использует только metadata.
   Native TextureSerialization1219/FullLoader213 PASS; архивный original icebat
   совпал по5460 mip bytes. Managed58: четыре реальные текстуры и десять
   synthetic source-форм; Viewer610. TextureTool71 при экспорте3PNG и2052
   assertions для6замен одного Bloom_body; прежние PNG побайтно совпали.
   Исправлен новый XRGB preview defect на повторных field5 до checkpoint.
   Запись неподтверждённых source форм явно закрыта; FAT остаётся предложением.
9. [PS2 native metadata](tool-ps2-texture-native-inspection-2026-09-10.md):
   удалён C# native header/palette/mip parser. Общий inspector сохраняет raw
   flag, descriptors и wire dataSize; нулевой flag не скрывает данные.
   Native204, real fixture15, managed15 PASS на noisesm из redf03.
   Mip dimensions помечены неизвестными; PS2 source/runtime/swizzle не закрыты.
   Исправлено старое ошибочное описание влияния field6 в class dossier.
   Legacy book остаётся metadata-only: original bool-success без Init
   не является доказательством созданной текстуры; [граница](tool-legacy-texture-source-boundary-2026-09-10.md).

## Новые границы и исправления

- Font baseline отсутствует до reader assignment: nullable, не выдуманный ноль.
- Text metadata inspector готов; full runtime Text остаётся незавершённым.
  TextNode использует общий metadata adapter, не полный runtime loader.
- Original null-Font setter и TextNode teardown упёрлись в недостающую среду;
  успешный setter проверен отдельно с настоящим Font. Остатки общей
  инфраструктуры не выдаются за успешное полное освобождение.
- Новые host-дефекты Font (foreign entry и diagnostic pointer) исправлены
  до checkpoint и покрыты адресными проверками.
- Коллизия backup в WinxHairPatcher исправлена и проверена; новых спорных
  изменений игровой логики этот блок не потребовал.
- Три новых texture test ожидания исправлены по существующим контрактам:
  cap одной секции, допустимый нулевой header02 и normalization1→2.
  Production readers не подгонялись под тесты. Новый PC source observer
  не закрывает пустой cached TextureData; combined TextureData Corpus profile
  остановлен девятым guard до записи БД.
- Попытка подключить animated textures к одному renderer pass выявила
  несовпадение выбранного примера: actual BloomX Material89 имеет три passes.
  [Проверены пять входов](tool-animated-texture-preview-boundary-2026-09-10.md):
  найденный однопроходный material принадлежит ParticleSystem, остальные
  нуждаются в реальных нескольких passes. Незавершённый patch сохранён
  отдельно и убран из рабочего кода. Нужен подтверждённый multipass backend;
  его blend/depth/default границы исследуются точечно.
  [Начальные renderer states](tool-pc-renderer-startup-states-2026-09-10.md):
  original tail подтвердил пять desired states в двух exact-slice случаях.
  Полный state-init остановлен на protected selector; BLENDOP171 и начальные
  texture-stage args остаются unknown. Actual CreateDevice-цепочка найдена,
  но не заменяет наблюдение этих значений. Multipass пока не подключён.
- Публикация Viewer остановлена автоматической проверкой дважды. Владелец
  `AnDi-SD` и admin/push права подтверждены read-only GitHub API; origin публичный.
  Проверка требует отдельного согласия на 47 commits до `00407c3` в
  `AnDi-SD/SmoViewer:master`. Такой запрос отправлен пользователю; ответа пока
  нет, другие способы публикации не применялись. Локальные commits сохранены.

Время остановки ещё не наступило; цикл продолжается.
