# Ядра tools: состояние на 11 сентября 2026

**Все ядра ещё не закрыты.** В текущем цикле завершён 21 связанный блок.
Есть проверенные рабочие операции у всех семи приложений; это не 7/7 полностью
готовых ядер. [Журнал](tools-core-cycle-2026-09-11-1900.md) содержит конкретные
изменения и проверки. Предыдущие срезы 9/10 сентября остаются историческими.

Цель — завершить ядра к 14–15 сентября. Приоритет: общая SMO логика и её
реальные потребители, PC прежде; PS2 подтверждается отдельно, когда это
помогает текущей работе. Интерфейсы получают необходимые подключения.
Полное восстановление исполняемых файлов остаётся отдельной дальнейшей целью.

## Что уже подключено

| Приложение | Подтверждённая работа ядра | Что ещё требуется или ограничено |
|---|---|---|
| Viewer | Общие graph/Node/Model/Skin/SAN; GPU skinning и picking; все поддерживаемые material passes/stages, clocks, LightManager и weighted shader lighting; original runtime mips; alpha ordering; Text geometry/atlas; отдельный SkyBox; rigid Fog | Rigid fixed-function lighting пока editor preview. Полная particle simulation/draw, weighted Fog, default Font startup, полный visibility/frame не завершены. Некоторые из них выходят за существующие операции инструментов и не должны вытеснять текущие blockers. |
| Exporter | Actual geometry/skin/Node/placements; общий SAN; GLB/OBJ/FBX; текущий GLB/FBX контроль 207 placements с независимым SDK | Полное представление material passes/layers/controllers в целевых форматах не закрыто: текущая проекция сохраняет базовый материал/текстуру. Non-unit skin и ограничения target formats названы явно; нужен окончательный контракт export fidelity. |
| Importer | Общие writers и envelope; native SMO forest transfer по actual reference sites, включая AnimTexController; оба направления StellaX/Icy; actual window native load/plan/save; общий GPU fitting | Native preview пока geometry-only. Flat ImportedScene не выражает все игровые материалы; direct authoring сохраняет guard. Редкие cached/orphan/repeated LTS, неоднозначные palette/name mappings и cross-platform texture authoring требуют явных операций. |
| LVLcreator | Общие workspace/catalog/scene; повторные render slots адресуются occurrence key; команды transform/undo/redo/collision/export/save/reopen; общий viewer backend | SkyBox world-only edit требует передачи свежей local pose. Остаются nonuniform inverse-world, cached120/physical84 и редкие lossless-формы, ранее отложенные к плотной работе с редактором. |
| TextureTool | Общие headers/material reader/texture codecs/writers/envelope; actual PC source; извлечение PNG, fixed-size замена и resize/save/reopen в настоящем GUI | Ограничения source/platform conversion и редких legacy форм остаются; PS2 runtime/authoring не объявлен готовым. Нужна итоговая фиксация перечня поддерживаемых входов, а не новый независимый reader. |
| SanToVmd | Общий native sampler и graph; body retargeting/VMD как целевой формат;39 tests; реальный Icy/xiid51 frames/2601 keys с независимым reader | В проверенном PC профиле нового blocker миграции нет. MMD visual playback и весь корпус не проверены; входные общие guards сохраняются. Исправлено влияние legacy console encoding на result API. |
| WinxHairPatcher | Собственная файловая операция использует подтверждённые patch signatures; backup/rejection/копии EXE;38 checks | В этой операции нового blocker миграции не найдено. Визуальная проверка всех игровых комбинаций остаётся отдельной границей. |

Подробности новых визуальных операций:
[materials](tool-opengl-material-passes-2026-09-11.md),
[lighting](tool-shader-lighting-2026-09-11.md),
[alpha](tool-alpha-ordering-2026-09-11.md),
[Text](tool-text-gpu-2026-09-11.md),
[mips](tool-runtime-texture-mips-2026-09-11.md),
[Sky](tool-skybox-gpu-2026-09-11.md),
[Fog](tool-fog-gpu-2026-09-11.md).
Нативный [перенос SMO](tool-native-visual-transfer-2026-09-11.md) и
[подключение Importer](tool-native-transfer-window-2026-09-11.md) отделены
от ограничений конверсии материалов в промежуточный ImportedScene.

## Какие прежние blockers уже устранены

Общий FAT/envelope writer внедрён в потребителей; ожидание отдельного
согласования снято разрешением 11 сентября. Перенесены sort policy,
Occlusion Init/reader/world и looping Particle Init. Race_02 загружается:
7513 objects/319 Nodes/2 Occlusion. PC2 bg.smo даёт 539 original particle records.
LightManager подключён к actual scene; Text/TextNode теперь имеют CPU runtime
и GPU consumer. Старое menu.smo открывается и выводится:41 Mesh+10 Text.
Прежний отказ всех 41 meshes из-за CDCD в конце strips устранён общей
topology projection. Копии C# strip converters удалены. Эти задачи не должны
повторно попадать в очередь как отсутствующие.

Legacy pixels поддерживаются явным tool adapter, strict reader остаётся
отдельным. Это host compatibility policy, а не доказательство прежнего
source-selection path игры. Original long-word Text wrap может зацикливаться;
tool iteration bound выдаёт явный отказ. Общие lifetime/размерные guards
сохраняются: Nav повторной непустой таблицы, Occlusion re-init, Particle
capacity и прочие узкие неподтверждённые операции перечислены в досье.

## Следующая очередь для срока 14–15 сентября

1. Закрывать наблюдаемые отличия Viewer/LVL output: actual rigid lighting
   через существующие DXLight payload/SubmitLights и современный backend;
   затем точечные integration gaps. Не повторять уже восстановленный выбор
   LightManager или weighted shader. Full frame/gameplay вне этой операции
   не ставить перед рабочими ядрами.
2. Установить и внедрить полный контракт material export и external import:
   что целевой формат реально выражает, что требуется сохранить отдельно
   и где необходим явный отказ. Native SMO transfer уже сохраняет весь graph
   и не должен снова проходить через flat converter.
3. Закрыть требуемые операции редактирования LVL, включая Sky local pose;
   нужные cached/reference/lossless случаи проверять по выбранным fixtures.
   Остальную симуляцию и неиспользуемые методы исследовать после blockers.
4. Приёмку вести по операциям каждого приложения на маленьких выбранных
   наборах. Общие изменения проверять одним связанным пакетом; полный corpus
   scan оставлять только для изменения контракта с неустановленной областью.

## Учёт прогресса

Основные единицы — доступная операция, конкретный закрытый blocker и её
проверка. В этом цикле 21 блок, а последний срез охватывает 7/7 потребителей
известными сценариями. Эти числа нельзя преобразовать в процент полной
готовности ядер. Исторические 16/16 tool operations и 30/35 readers имели
другой scope; они не служат знаменателем текущей миграции.

Отдельные последние assessments EXE остаются **PC 36,032742% / PS2 26,923642%**.
За transport, UI wiring и device backend новых процентов изученности
исполняемых файлов не начислено. PS2 runtime parity в этом цикле не заявлена;
подтверждённая PS2 FontManager material ownership записана в Text досье.

[Конечный контроль потребителей](tool-final-consumer-checks-2026-09-11.md)
сохраняет raw evidence, первый SAN console failure и финальный успех.
Сборка и local commits являются контрольными точками, выпуск не проводился.

Для следующего цикла подготовлена [короткая точка возобновления](tools-core-resume-2026-09-11.md)
с нужными исходниками, уже подтверждёнными контрактами и первым недостающим
consumer. Она позволяет начать работу без повторного чтения всех 21 досье.
