# Оставшиеся игровые классы: независимый проход PC и PS2

## Логические группы

| Группа | Классы выбранного блока |
| --- | --- |
| Организация игры | AIManager, AlphaManager, AnimationManager, AppHelper, AssetManager, BossFightManager, CharacterPlacementManager, CharacterRegistry, CinematicManager, DebugManager |
| Данные и игровой прогресс | PlayerProfile, PlayerTraits, SideQuestManager, LocalizedStrings, FaceData, VectorWrapper |
| Сцена и события | BreakableObject, BreakableObjectFactory, BreakableStatue, FairyStar, FairyStarsManager, LevelChangeTrigger, OneLinerTimeTrigger, ParticleManager, Perception, PerceptionTrigger, RollingPin, StableLamp, StarEmitterTarget, TargetManager |
| Представление и взаимодействие | HUDManager, DialogWindow, DialogueBoxGameFlowState, OptionMenuGameFlowState, TextController, UserInput, VibrationManager, InputDeviceInterface и PC/PS2 реализации |
| Платформенные сервисы | MoviePlayer и PC/PS2 реализации, ProcessBuffer и PC/PS2 реализации, Profiler, LoadSavePS2Manager, MemoryManagerConfigPS2, TRCManagerPS2 |

Все имена имеют префикс `wx`. Группы организуют реестр; они не объявляют
все функции этих менеджеров восстановленными.

## Измеренный проход

Первый пакет: **38 PC factory attempts, три процесса, 16,576 с**.
Окончательно **28 полных lifetimes** (140 операций) и четыре случая с
успешными factory/RTTI/Clone (12 операций), но остановленным удалением.
Еще шесть случаев остановлены в factory. Суммарно 152 завершенные class
operations, а не 38 закрытых классов.

PS2: **41 независимо сопоставленная factory/construction identity**,
46 оригинальных getters и пять null Clone (0,875 с). У всех 41 factory
прослежены allocation size, вызов собственного или базового constructor и
запись своей vtable; отдельно сохранены девять inline construction paths.
Исполнение getters не считается исполнением всех PS2 конструкторов: их
SQ/LQ, FPU, hardware и остальные зависимости остаются за границей проб.

Три прежние оценки не меняются: FaceData PC80, DialogueBox и OptionMenu PS215.
Новые 87 строк — 43 PC и 44 PS2. После этого у всех зарегистрированных игровых
классов есть явная оценка, включая низкие оценки неготовых/неинициализированных
объектов. Это полнота учета имен, а не полнота игровой логики.

## Физические родители

Два дополнительных расхождения ancestry установлены на обеих платформах:

| Класс | Registered parent | Реальный construction parent | PC / PS2 |
| --- | --- | --- | --- |
| wxMoviePlayer | wxEntity | spBaseObject | 005FA2D0 →0040E910 /003823E0 →00102BF0 |
| wxParticleManager | spBaseObject | spParticleSystemManager | parent ctor 004A2E40 /001BC410 |

PC MoviePlayerPC factory действительно посещает base 005FA2D0. PC
ParticleManager factory посещает 004A2E40; тот записывает table 006E70F8,
getter 0045A430 возвращает registration spParticleSystemManager. PS2
parent table 004906D0/getter 001BC150 независимо подтверждает тот же тип.
Зарегистрированные родители сохраняются; нельзя механически получать
physical layout из RTTI ancestry.

## Незавершенные lifetimes

| Затронутые классы | Точная остановка и остающаяся зависимость |
| --- | --- |
| HUDManager | factory, 0055C217: original global 0075DB68 отсутствует при построении зависимого HUD объекта |
| InputDevicePC, UserInput, OptionMenuGameFlowState | factory, 004BE693: сброс игрового дерева с неинициализированным receiver/head |
| VibrationManager | factory/RTTI/Clone прошли; delete требует ту же input tree dependency |
| PlayerProfile, TargetManager | factory, 00592C71: чтение null AssetManager root string по +14 |
| StableLamp | factory/RTTI/Clone прошли; delete приходит к тому же asset-name consumer |
| LocalizedStrings | factory/RTTI/Clone прошли; 005601D3 читает отсутствующий массив +18 |
| FairyStar | factory/RTTI/Clone прошли; после protected traversal delete приходит к null dependency 0040FB60 |

Начальная scout-подпись `pc-input-os-dependency` не является доказательством
OS-вызова: 004BE690 — игровой tree reset, а не import. Root string, tree head,
HUD callbacks и ресурсы не заполнены придуманными успешными ответами.
Затронутые lifetimes отложены; остальное семейство исследовалось независимо.

## Оценки и ограничения

Базовая новая оценка concrete PC lifetime —20, partial Clone —15,
незавершенный constructor —10. Дополнительные методы: VectorWrapper45,
Perception40, MoviePlayer35; ParticleManager25 с physical-parent proof.
No-factory базы: InputDeviceInterface10, ProcessBuffer15, PerceptionTrigger15,
DialogueBox10. PS2 constructor identities —15; VectorWrapper35,
Perception30, MoviePlayer30, ParticleManager20; no-factory Input10,
PerceptionTrigger15, ProcessBuffer15. Это оценки объема поведения с явными
границами, не доли байтов executable.
