# Оставшиеся игровые классы: независимый проход PC и PS2

[Контракт](../../research/game-remainder-contracts-2026-09-10.json) содержит
50 имен, 44 PC и 46 PS2 регистрации, точные таблицы/исходники проверок и
неудачные промежуточные runs. Результаты:
`local-data/results/native-cycle-20260910-1900/game-remainder/`.

Это первичное расширение исследования всего executable. Попадание последнего
класса в реестр оценок не означает, что его методы полностью восстановлены.

## Логические группы

| Группа | Классы выбранного блока |
|---|---|
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

AlphaManager (880 байт) во время Clone исчерпал 64 KiB guest arena.
После измерения применен отдельный 128 KiB профиль: новый полный проход
0,708 с. Его внутренние allocations вместе с контекстом суммарно запрашивают
89 720 байт, 1012 выделений. Это не размер самого класса. Ограничение одного
запроса 32 KiB сохранено; игровые количества объектов не уменьшались.

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

## Подтвержденные общие методы

`probe_game_remainder_copy.py`: **13 пар, 2,851 с**, плюс отдельная NaN diagnostic.

**VectorWrapper.** PC Copy 00596DA0 и PS2 002A4F20 вызывают BaseObject Copy,
переносят координаты +10/+14/+18, затем вызывают оригинальную no-op completion
CloneManager с ID 1F793E0D. PC использует integer move для X и FLD/FSTP для Y/Z;
PS2 — LWC1/SWC1 для всех трех. Шесть подтвержденных пар охватывают обычные
числа, оба нуля, denormals, infinities, quiet NaN и signaling NaN в integer X.
Сравниваются весь объект и неизменность источника. PC объекты реально созданы
и удалены; PS2 начинается после SQ prologue, исполняет настоящее Base Copy
00100320 и completion 00104F00, заканчивается перед LQ epilogue.

Седьмой случай с signaling NaN в Y/Z отделен от golden cases. Первоначальное
ожидание quieting не совпало с Unicorn: тот сохранил исходные биты. Это
диагностика ограничений стенда. Режим x87/исключений самой игры и результат
на аппаратуре не установлены; NaN правило PC и равенство платформ не
объявляются. PS2 LWC1/SWC1 bit transfer проверен отдельно от арифметики R5900.

**Perception.** PC Copy 004F20D0 сначала вызывает virtual release назначения,
затем Base Copy и копирование дерева; далее переносит primitive поля.
Три PC опыта выполняют полный Copy между реально созданными объектами с
пустыми деревьями. PS2 имеет отдельную container implementation и в этой
проверке исполняется только tail 0036EE18 после нее. Совпадают word fields
+24/+28/+2C/+30/+34/+38/+3C/+44/+4C/+50/+54/+74 и bytes +40/+48/+70.
Pointer fields +68/+6C оставлены null. Сравнивается весь источник и назначение;
перед PC cleanup восстанавливаются только введенные primitive test fields.
Удаление произвольного populated runtime graph этим не подтверждается.

**MoviePlayer.** Четыре пары direct methods 005FA110 /00382320 копируют
input +40/+44 в receiver +1C/+20, обнуляют +14 и переносят raw byte +4C в +24,
возвращая true. Проверены high-bit слова и bytes 0/1/2/255 с полными guards.
PC использует настоящий MoviePlayerPC, PS2 — явно заданную storage record;
она не выдается за исполненную PS2 factory или ее allocation size.
Декодирование видео, звук и графический вывод остаются открытыми.

## Физические родители

Два дополнительных расхождения ancestry установлены на обеих платформах:

| Класс | Registered parent | Реальный construction parent | PC / PS2 |
|---|---|---|---|
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
|---|---|
| HUDManager | factory, 0055C217: original global 0075DB68 отсутствует при построении зависимого HUD объекта |
| InputDevicePC, UserInput, OptionMenuGameFlowState | factory, 004BE693: сброс игрового дерева с неинициализированным receiver/head |
| VibrationManager | factory/RTTI/Clone прошли; delete требует ту же input tree dependency |
| PlayerProfile, TargetManager | factory, 00592C71: чтение null AssetManager root string по +14 |
| StableLamp | factory/RTTI/Clone прошли; delete приходит к тому же asset-name consumer |
| LocalizedStrings | factory/RTTI/Clone прошли; 005601D3 читает отсутствующий массив +18 |
| FairyStar | factory/RTTI/Clone прошли; после protected traversal delete приходит к null dependency 0040FB60 |

LocalizedStrings (PC2144/PS22132) в cold factory оставляет storage pointers
+18/+28 null, а destructor проходит четыре/два элемента. Необходимый этап
инициализации еще не восстановлен. У FairyStar micro и file profiles сначала
достигали instruction cap в protector; отдельный protected-block проход
дошел до настоящей отсутствующей зависимости. Результат не подменялся успехом.

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

Полных новых закрытых классов нет. Открыты игровые Tick/queries, populated
containers, загрузка ресурсов, profile progression, input/window bootstrap,
видео/звук/rendering, соответствующие PS2 методы и x87 NaN diagnostic.
Первый copy probe исправлен из-за обращения к несуществующему helper `put`;
все три версии и failed reports сохранены. Ошибки стенда не исправлялись
изменением игровых инструкций.
