# Game flow: навигация HUD и переназначение ресурсов PC/PS2

Продолжение [базового `wxGameFlowState`](native-class-wx-game-flow-state-2026-09-10.md),
10 сентября 2026. Оба pristine SHA сохранены в предыдущем dossier и probes.
Этот блок уточняет активные методы и один метод `wxGameFlowController`.
Данные и callbacks стенда не выдаются за готовые HUD/widget factories.

## Навигация

PC `5D6E70..5D7134`, PS2 `33B910..33C228` — обработка навигации между
элементами HUD. В PC selection вынесен в helper`5D6BE0..5D6CAF`; PS2 встраивает
его ветви в четыре направления. Таблицу 56 STX имён этот метод не читает.
Связь с ней предполагалась в промежуточном сообщении, но не подтвердилась.

Подтверждённый порядок:

1. При ненулевом timer`2C` вызвать его Update. Прочитать float32 magnitude по
   `[borrowed30+4]`. Для обычных конечных значений `<=0.5` поставить byte`38=1`
   и выйти. На PC unordered x87 comparison также ведёт сюда; PS2 NaN/Inf/EE FPU
   поведение не проверялось и не переносится с обычного MIPS guest.
2. При byte`38=0`, ненулевом deadline`34` и unsigned timerCurrent`1C <=34`
   закончить без перемещения. Иначе можно выбирать элемент. На общем выходе
   активного пути byte`38=0`, включая отсутствующую ссылку/направление.
3. `wxHUDManager` возвращает страницу по переданному индексу. PC getter
   `5593A0` читает `manager+18+4*index`, только если signed index `<74`;
   нижней проверки в оригинале нет. Probe использует валидный index3.
   Текущий элемент PC page`134`, fallback`138`; PS2 page`13C /140`.
4. Угол `[borrowed30+8]` выбирает одну из четырёх строк ссылки. Порядок
   проверок и исходная точность floating point существенны — см. ниже.
5. Если `strstr(link,"empty")` ненулевой, закончить. PE IAT`6D932C` — именно
   **MSVCR71 `strstr`**, не `strcmp`/`stricmp`: строка `xemptyx` тоже останавливает
   переход. Затем selection helper проверяет точное `empty`, ищет target и
   возвращает stop=true также при отсутствии target.
6. Найденный target с byte`28=0` пропускается. При byte`28!=0` отправляется
   сообщение`27D1`, sender=this, args`10,0`, receiver=`wxPlayerProfile+2B4`;
   затем меняется выбор страницы, deadline=`timerCurrent+500` modulo2^32.
7. После пропущенного target ссылка разрешается ещё раз и счётчик увеличивается.
   Повторить, если `next != startingElement OR count <20`. Это **не жёсткий
   лимит 20 итераций**. Для self-cycle выполнено ровно20; цикл, из которого
   startingElement недостижим, этим условием не ограничен. Такой бесконечный
   runtime-сценарий не запускался и не заменялся нашим ограничителем.

PC lookup`55D3F0` проходит `[page128,page12C)` pointers и сравнивает name bytes
по getter`435C40` (object`54`) до NUL. Selection`55D370` снимает старый state
virtual`30(0)`, если старый pointer отличается, запоминает target в`138`;
target word`50==1` обнуляет`134`, иначе делает его текущим. Ненулевому текущему
посылается virtual`30(2)`, затем каждому элементу virtual`2C()`.
PS2 counterpart`37E3D0` имеет иные offsets/slots (`13C/140`, target`4C`,
state slot`38`) и тот же исследованный порядок. Свойства самих виджетов
и результат этих callbacks отдельно не реконструированы.

## Направления: нельзя безусловно объединять PC и PS2

Номера0..3 здесь означают ветви кода; экранные «вверх/вниз» не присвоены без
producer угла. При pristine constants первая ветвь включает `[-q,+q]`,
вторая — внешние области около ±π, затем отрицательная и положительная боковые.

| Ветвь | PC link offset | PS2 link offset |
|---:|---:|---:|
| 0 | `A4` | `A0` |
| 1 | `E4` | `E0` |
| 2 | `124` | `120` |
| 3 | `164` | `160` |

PC central bound — float global`742EC4`; остальные helpers`524380 /5243C0 /
524400` используют отдельный global`740AC4`. В pristine обоих raw=`3F490FDB`
(`q≈π/4`), но их независимые адреса нельзя терять. PC вычисляет `3*q` в x87
без промежуточного float32 store. PS2 сравнивает с literal raw=`4016CBE4`.

Из15 одинаковых конечных float32 входов **13 направлений совпадают, 2 отличаются**:

| Raw angle | PC | PS2 |
|---|---:|---:|
| `4016CBE4` | 3 | 1 |
| `C016CBE4` | 2 | 1 |

Округлённый float32 `3*q` чуть ближе к нулю, чем PC x87 произведение. Поэтому
указанные два значения ещё попадают в боковые области PC, но уже во внешнюю
ветвь PS2. Проверены соседние float32 значения по обе стороны границы и ±q.
Это platform behavior, не ошибка, которую можно «исправить» унификацией.

## Переназначение пяти числовых кодов

PC `5D6CB0` (protected entry), PS2 `33B430`. Сначала получить код текущего
нижнего состояния `s` через `wxGameFlowController`; `p` — **signed** word
`wxPlayerProfile+514`. Значение/сюжетное имя `p` пока не присвоено.

| Input | Условие | Output |
|---:|---|---:|
| 24 | `s==3` | 53 |
| 44 | `s==3` | 52 |
| 49 | `s==17` | 55 |
| 43 | `p<7 && s!=40` | 5 |
| 5 | `s==39` | 54 |
| 5 | иначе `p<7` | 43 |

Во всех остальных случаях вернуть исходный code. Порядок двух правил для5
сохранён. Обе платформы дали одинаковые результаты для30 наборов, включая
raw signed-negative progress и unchanged `FFFFFFFF /80000000` inputs.
Это подтверждает таблицу решений; автоматическое сюжетное толкование имён
ресурсов или перенос на другие code domains не делается.

## `wxGameFlowController`: выбор нижнего состояния

Class ID `078B20E8`, direct base `spBaseObject`. PC getter`5954E0..595516`,
PS2`339BA0..339BF4`: взять signed top index из`1AC`, просматривать pointers
от`15C+4*index` назад. Пропускать состояния со signed member`10 >50`.
Первое member`10 <=50` вернуть; при index<0 или отсутствии такого вернуть0.
Проверки null state pointers в этом leaf нет. Не вводить её как правило игры.

Семь одинаковых directed stacks прошли **original return на обеих платформах**:
пустой, один, наложенные состояния>50, полностью пропущенные, граница50 и два
raw negative codes. Negative codes в этих fixtures проверяют signed comparison;
их нормальная достижимость через игровой producer не утверждается.
Остальные lifecycle/transition/stack mutation методы controller остаются открыты.

## Доказательства и пределы

- `probe_pc_game_flow_navigation.py`: **29 frames за1,245 с**, original
  GameFlowState+timer factory и teardown,2/2 owners freed. Literal borrowed
  HUD/profile/input/widget records; supplied CRT strstr и callbacks виджетов
  только фиксируют эффекты. Original HUD lookup/selection и null-target message
  dispatch выполняются. Проверены пороги, repeat gate999/1000/1001, fresh input,
  нулевой deadline, wrap до244, substring sentinel, missing target и self-cycle20.
- `probe_ps2_game_flow_direction.py`: **15 fresh guests за0,319 с**, ordinary
  FPU/integer prefix`33B9D4..33BB08` до выбранной ветви. Каждый guest отброшен
  на явной границе; full PS2 frame/EE/MMI не запускались.
- `probe_game_flow_resource_remap.py`: **30 paired remaps +7 paired getters
  за1,286 с**. PC whole method с original getter; PS2 remap — original decision
  prefix`33B458` с явно заданным результатом getter, заканчивается до epilogue.
  PS2 getter отдельно исполняется целиком. Полная PS2 цепочка не подменяется
  утверждением о совместном запуске этих двух проверок.
- Первый remap probe сохранил7 getters и17 remaps, затем остановился на
  непредоставленном PS2 stack restore в delay slot passthrough epilogue.
  Original bytes не менялись: исправлена граница стенда, fresh guests повторили
  связный пакет. Failed run и его script сохранены; это не ошибка игры.

Артефакты в `local-data/results/native-cycle-20260910-1900/`:
`game-flow-active/{windows*,pc-navigation-run1.json,ps2-direction-run1.json,
paired-directions.json}` и `game-flow-remap/{windows*,paired-run1/2.json}`.
Каждый probe содержит input/code/script hashes, ограничения и явно названные
boundaries. Два общих класса пока не закрыты и не превращены в выдуманные
успешные runtime factories. Portable implementation следует после установления
необходимых object/input boundaries; текущий результат — original contracts.

Обновление экспертных оценок знаний: `wxGameFlowState` PC35→60, PS230→50;
первый `wxGameFlowController` PC10/PS210. GameFlowState — partial, controller —
scouted; все четыре assessments — new_research.
Это не показатели реализации исходников или готовности инструмента.
