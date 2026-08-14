# Свидетельства из executable и модов

Статус: статический triage и выборочная runtime-проверка без изменения исходных
бинарников. Адреса нужны как точки входа для последующей разметки и не являются
завершённой декомпиляцией.

## Зафиксированные бинарники

| Файл | Тип | Размер, байт | SHA-256 |
|---|---|---:|---|
| `SLES_532.19` | PS2 ELF32 little-endian MIPS | 3 799 600 | `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE` |
| baseline `WinxClub.exe` | PE32 x86 native | 22 065 152 | `D8D0AD112D46F7229C7227B2D15338195D54957AB408E75C1E566895EF0988C0` |
| `WinxClubWithDebugMenu/WinxClub.exe` | PE32 x86 patch | 22 065 152 | `C27EA9DB4228781A12A90AE808807D4AF1397A7E40DD8F5FFF28F3C87CC62CDB` |
| `Winx Club Resolution Changer.exe` | managed .NET PE32 | 26 624 | `384B8A029F9FF32404196192B7224472CCF8051168C72C285B42E11519BE6FE8` |
| `Winx Club Tweak Center.exe` | managed .NET PE32 | 27 648 | `15AD37CD5D5D33DB9DB79AABAB2B6B82F885736ED9AFF1B846E4265881746C32` |
| `WinxClubTweakCenter2.exe` | managed .NET PE32 | 48 640 | `FF352456512B9A4F40C07EA7E01651F2EBFBEBF7CC1D2BE849FAA7396207977A` |

Полные пути и сами файлы остаются только в игнорируемом `local-data/`.
Текущий PC baseline удобен для сравнения с debug-menu patch, но не считается pristine-сборкой.

## Эквивалентные FFPS-проверки — подтверждено

PC-функция около RVA `0x00022260` сравнивает первое слово потока с little-endian `0x53504646` (`FFPS`), а около RVA `0x000222D6` проверяет `0x26` по смещению `+4`.

PS2-функция около VA `0x00181D80` выполняет эквивалентную последовательность MIPS: формирует константу `0x53504646`, сравнивает первое слово, затем слово `+4` с `0x26`.

Это независимое статическое evidence из runtime-кода: обе версии распознают заголовок `FFPS` + `0x26`. Оно не доказывает идентичность исходной функции, но подтверждает, что сигнатура не придумана исследовательским parser.

## PS2 ELF

- ELF32 LE, MIPS, `ET_EXEC`;
- entry point `0x003E4828`, основной image base `0x00100000`;
- строка компилятора `MW MIPS C Compiler (2.4.1.01) PlayStation2`;
- `.symtab`/`.strtab` очищены;
- сохранены имена классов, RTTI-like/registration strings, токены полей serializer и диагностика;
- присутствует баннер `Sparkplug Engine v1.0`.

## Следы исходного дерева в PC executable

Найдены 53 пути вида `Z:\Sparkplug\Code\...`, среди них:

- `spDataBlockSerializer.cpp`;
- `spSerializer.cpp`, `spSerializerManager.cpp`;
- `spModelSerializer.cpp`, `spMeshDataSerializer.cpp`;
- `spPS2MeshDataSerializer.cpp`;
- `spTextureDataSerializer.cpp`, `spPS2TextureDataSerializer.cpp`.

CodeView сохраняет ссылку на `Z:\Winx PS2\CODE\Build\PC\Release\WinxPC.pdb`, GUID `762A83CB-9F00-4B65-B56E-D1979A28956A`, age `1`. PC executable защищён SecuROM 7, поэтому PS2 ELF удобнее как основная статическая опора.

## Словарь serializer

| Признак | PC | PS2 |
|---|---:|---:|
| уникальные имена классов `sp...` | 347 | 291 |
| общие имена классов | 244 | 244 |
| строки `DataBlockSerializer.*` | 314 | 298 |
| уникальные токены с шаблоном имён serializer fields | 197 | 188 |
| упоминания SMO/SAN/SPT/SPL/ANM/PCK | 436 | 426 |

Среди найденных токенов присутствуют `esfMeshDataPlatformSpecific`, `esfTextureDataCrossPlatform`, `esfTextureDataPlatformSpecific`, `esfPS2TextureData`, `esfNodeChild`, `esfSkin`; среди size codes — `escSize8`, `escExtended8`, `escExtended16`, `escExtended32`. Их точная роль должна быть подтверждена xrefs. Прямой строки `SBOO` нет: путь к ней нужно восстанавливать от FFPS-проверки и кандидата `BeginObject`.

## Debug Menu patch

Patch сохраняет размер и entry point baseline PE и меняет небольшое число диапазонов. В свободной области `.data` около RVA `0x00BC0000` находится x86-hook; переход на него установлен около RVA `0x001CBB90`.

Hook динамически получает `USER32.dll!GetAsyncKeyState`, проверяет virtual key
`0x70` (**F1**) и вызывает функции RVA `0x001CCEC0`/`0x001CDB90`, открывающие и
закрывающие debug menu. Навигация меню использует стрелки и Enter. Handler
`LOAD LEVEL` передаёт выбранный 1-based ID штатному level manager; текстовый путь
ресурса он не принимает.

Шесть NOP-патчей оставляют включёнными пункты `PROFILER`, `DUMP LEVEL BLOCKS`,
`DUMP ALL BLOCKS`, `DUMP MEMORY STATS`, `SHOW STATISTICS` и `SHOW FPS`. Изменения
двух ранних значений `0 -> 1` относятся к ненулевым defaults ширины/высоты до
инициализации renderer, а не к debug-флагам или стартовому уровню.

## Скрытые параметры запуска

Gameplay/debug-ключей командной строки в нативной игре не найдено. Собственный
парсер читает `winx.ini`; подтверждены `fullScreen`, `startLevel`,
`testCinematic`, `cinematicToTest`, `firstPCKLevel`, `loadFromCD`, `buildPCK`,
`loadFromPCK`, `enableDialog`, `displayType` и `territory`.

`fullScreen=false` включает штатный оконный режим без патча EXE. Ненулевой
`startLevel` передаётся менеджеру смены уровня и позволяет пропустить меню.
Runtime-проба `startLevel=2` дала переход `0 -> 54 -> 2 -> 71` и естественный
запрос модели Bloom. Уровни имеют ID 1–50; 38–40 содержат пустые descriptors,
а `Gardenia04` (50) не имеет соответствующего level SMO.

Игра берёт ресурсы из `HKLM\Software\Konami\Winx Club\MediaPath` (в 32-битном
registry view). Поэтому validator может использовать отдельный рабочий каталог с
частными `winx.ini`/`config.ini` и копией `Shaders`, не меняя установку или
пользовательские настройки.

## Tweak tools

Три tweak/resolution utility являются managed .NET PE и пригодны для прямой
декомпиляции через ILSpy. Resolution Changer переписывает фиксированные пары
ширины/высоты и строку разрешения в EXE. Tweak Center меняет camera/flight
инструкции, `Player.spt`, challenge SPT и INI; его аргументы командной строки
принадлежат самому patcher, а не игре. Для оконного режима EXE-патч не нужен:
utility также использует штатный `fullScreen=false`.

Эти программы остаются картой patch points, а не источником истины формата;
native validator намеренно не запускает их и ничего не записывает в EXE/SPT.

## Дополнительные подтверждения 2026-08-10

- PC executable содержит отдельные serializer/optimizer paths `spMeshDataSerializer`, `spDXMeshDataSerializer`, `spPS2MeshDataSerializer`, `spTriangleStripper`, `spDXSceneGraphOptimizer`, `spSceneGraphOptimizer` и `spDXMeshCombiner`.
- Диагностика `GetIndexBuffer()->Is32Bit()` показывает, что serializer различает 16- и 32-битные index buffers; наблюдаемые `ushort` indices не доказывают общий лимит 65 535.
- Registration data подтверждает GUI class ID: `0x52E86EFE` (`spTextNode`), `0x19A745D7` (`spTextRenderable`) и `0x4693490A` (`spFont`).
- Имена IK/controller/helper nodes согласуются с полноценным DCC rig. 3ds Max остаётся рабочей гипотезой; прямой exporter/plugin след не найден.

## Следующие шаги

1. Импортировать `SLES_532.19` в Ghidra как MIPS little-endian с base `0x00100000` и разметить `0x00181D80` как кандидата на проверку FFPS-заголовка.
2. Автоматически выгрузить имена классов, токены полей и size codes в версионируемый словарь без игровых бинарников.
3. По xrefs восстановить `spDataBlockSerializer`, `spSerializerManager` и platform-specific mesh/texture serializers.
4. Восстановить generic per-object serializer checkpoint, чтобы нативный журнал
   показывал конкретный object index/class, а не только FFPS/resource-level этапы.
5. Исследовать безопасный late attach для SecuROM-сборок, отвергающих
   launch-under-debug.
