# wxAnimationController и spActor: команды и подготовка запуска

186 успешных исполнений: PC 95 (59 полных возвратов,36 остановок на настоящем
callee),PS2 91 (49 полных возвратов,42 явно ограниченных компонента).
Пилот6 случаев —0,825s;три общих пакета180 случаев —18,946s суммарного времени
внутри процессов. Первые два пакета выполнялись параллельно. Это не замер
ускорения относительно последовательного режима. Отказов нет.

Исследуются оригинальные инструкции над явно заданными borrowed полями.
Ни callback,ни binder,ни очередь событий не заменяются успешной заглушкой.
Все3000h bytes borrowed области сравниваются после каждого исполнения.
Адреса и смещения ниже шестнадцатеричные; количества и обычные значения —
десятичные. Новые C++ реализации здесь не добавлялись.

## Игровые команды

`wxAnimationController` уже установлен по factory/RTTI: PC allocation188h,
PS2 allocation1A0h. Владелец хранит `spActor` по128h/134h соответственно.
PC bind4FB6E0 создаёт54h bytes через5A3500;PS2 message branch2A6DF0 создаёт
те же54h bytes через1184B0. Зарегистрированный actor factory118800 устанавливает
PS2 vtable48CC00;PC actor vtable703F80. Проверка lifetime/bind этого блока
ограничена статическими caller accesses,полное подключение дерева не заявляется.

| Операция | PC игровой вход → движок | PS2 игровой вход → движок |
|---|---|---|
| Start | 4FB620 →5A1E30 | 2A6CF0 →1164A0 |
| Stop | 4FB2D0 →5A20A0 | 2A6CE0 →115FA0 |
| Fade-stop | 4FB2F0 →5A16D0 | 2A6CD0 →116930 |

PC protected entry исполняется как есть. PS2 Start начинается после последнего
SQ по2A6D10 с S3=newAnimation,S2=mode;это значения настоящих MOV до SQ.
A0=this,A3=fadeMode,T0=interruptPrevious. Prefix заканчивается на настоящем
child Stop или Start,полная SQ/LQ транзакция не засчитана.

Start сначала вызывает Stop(old,0),если interruptPrevious!=0 и old!=0.
До этого вызова собственные поля и actor ещё не меняются. Если остановка
не нужна,то old=recent,recent=newAnimation;в request записываются animation,
mode,reverse byte0,fadeMode. Actor bytes1C/24 становятся1. Затем вызывается
Start с указателем на встроенный request:

| Поле | PC | PS2 |
|---|---|---|
| request начало / animation | 12C | 138 |
| request mode | 130 | 13C |
| request reverse byte | 134 | 140 |
| request fadeMode | 13C | 148 |
| old / recent | 178 /17C | 184 /188 |
| счётчик после вызова | 180 | 18C |

Проверены interrupt0/1/255,old0/nonnull,mode0/1/FFFFFFFF,fade0/2/4.
Новые и старые handles в этом наборе сравниваются как непрозрачные значения;
они не загружают animation ресурс.

Четыре полных PC вызова с capacity0 либо двумя занятыми другими записями
подтвердили: отказ actor Start из-за отсутствия слота не откатывает историю,
request или включение actor. Счётчик всё равно увеличивается;FFFFFFFF→0.
В двух этих случаях предварительный Stop старого отсутствующего handle также
исполняется полностью. Это поведение wrapper,не утверждение о нормальной
игровой достижимости такого набора занятости. PS2 increment после child виден
статически2A6D6C..74,но совместное выполнение этой ветви здесь не проверено.

Stop всегда передаёт suppressEvent=0. Полные PC missing Stop возвращаются;
PS2 wrapper доходит до настоящего115FA0. Отдельный PS2 component115FCC
с S3=handle,S5=suppress,A3=0 до116130 подтверждает missing no-op без SQ/LQ.

## Запросы и отложенное затухание actor

Массив по actor28,число элементов2C,stride60h совпадают PC/PS2.
`Find`5A14C0/1168D0 возвращает первую matching запись независимо от counter48
и running4C. `Used`5A1500/116870 возвращает true при любом matching pointer
с ненулевым counter48,даже при running0. Нулевой pointer может найти пустую
запись. Проверены пустой массив,промах,duplicates,вторая запись,нулевые и
ненулевые counters включая80000000;все эти запросы исполнялись полностью.

Fade-stop5A16D0/116930 меняет только первую matching запись:
rate20=1/duration при duration>0,иначе raw fallback;fadeMode10=3,
threshold58=0,stopAfterFade byte3C=1. Прочие bytes,в том числе соседние bytes
3D..3F,сохраняются. Running/counter не блокируют операцию. Промах —no-op.
Игровой wrapper всегда передаёт fallback0;PS2 делает это настоящим MTC1.
Полные цепочки wrapper→actor проверены с durations−2/0/0,25/0,4/2/3,
промахом,inactive и duplicates;отдельный actor fallback−0,375 также проверен.
Exceptional float,denormal и все случаи округления PS2 FPU не квалифицированы.

## Подготовка Start до события

PC5A1E30 иPS2 component1164C4 проходят настоящий выбор слота и все записи
до queued event entry40FA10/1003C0. PS2 вход: A0/S2=actor,A1=request,V0/T3=0,
как после его собственного prologue. При отсутствии слота PC возвращает
FFFFFFFF,PS2 достигает116854 перед epilogue с тем же V0 и без изменений данных.

Выбирается первый slot с counter48=0,но matching animation с counter!=0
замещает выбранный индекс. При нескольких активных совпадениях побеждает
последнее;это проверка явного массива,не доказательство его допустимости
после нормального loader. Такой активный restart сохраняет текущий weight.

Request fadeMode0/3 принудительно делает request weight1;2/4 —0;прочие
проверенные1/5/FFFFFFFF сохраняют0,25. При новом слоте weight переносится,
при restart остаётся прежним0,625. Mode,reverse byteFF,fadeMode,callback/cookie,
time multiplier,transition duration переносятся по оригинальным offsets.
Положительные fade durations превращаются в reciprocal rates,неположительные
выбирают request rates. threshold=animationDuration−fadeOutDuration;
normalizedProgress=initialTime/animationDuration;sampleTime34 сохраняется.

Priority50=(animation18<<24)|(managerFrame10&FFFFFF),modulo32bit.
Running byte4C=1;status40=0 только приfadeMode2,иначе1;elapsed5C=0.
StopAfterFade byte3C сохраняется. Проверен точный payload первого события:
code2,queue engine110h/108h,broadcastFFFFFFFF,selected slot,animation.
Начало очереди задано ненулевым borrowed engine global;его lazy factory не
исполнялась. Event enqueue,возможное последующее fade event6,binder,flush и
полный успешный Start остаются вне этого PS2 компонента.

## Учёт и повторное использование

PC spActor85 сохраняется:его контролы и Start уже были исследованы ранее.
Новый PS2 пакет поднимает spActor20→35. Игровые команды wxAnimationController
добавляют PC20→35 иPS2 15→30;это три assessment updates,не новые полные классы.
Смежные CharacterState не получают повторного credit за эту общую зависимость.

Контракт: [character-animation-playback-contracts](../../research/character-animation-playback-contracts-2026-09-11.json).
Проба: [probe_character_animation_playback.py](../../research/probe_character_animation_playback.py).
Точные версии probes/wrappers,линейные captures,входы и результаты закреплены
в локальной `local-data/results/native-cycle-20260911-0730/character-animation-playback/`.
Эта папка отсутствует в обычном Git clone. Первые три PC exploratory boundary
calls сохранены отдельно;они не входят в186 учитываемых проверок.
