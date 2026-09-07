# PC app → engine frame → анимация и графика

Checkpoint 5 цикла до10:00 МСК 6 сентября. Тот же pristine EXE SHA256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Это executable-call evidence и isolated replay, **не запуск игры или GPU**.
Original paths `Code/SparkplugPC/spPCApp.cpp` и `Code/Sparkplug/spEngineCore.cpp`
подтверждены; названия неизвестных операций остаются адресными.

## Вход из приложения

`spPCApp` update **4C2D60** (virtual24) вызывает pre-update virtual30,
затем **4C2D7D→41CD50** на global engine755274. После общего update:

- если `GetForegroundWindow()==app.HWND`, запускает graphics41C460;
- иначе проверяет `GetWindow(foreground,4/GW_OWNER)==app.HWND` и при совпадении
  тоже запускает graphics;
- иначе запрашивает Sleep(1ms), не вызывая graphics.

Update возвращает true, **игнорируя результат обоих engine helpers**.
Значит анимация и общий update продолжаются при background window. Это не
доказательство аналогичной политики game override или полного message-loop timing.
Исходный message loop вызывает virtual24 по4C2CE2; здесь сам Win32 pump не запускался.

## Общий update41CD50..41CE73

Порядок original calls, без short-circuit по return values зависимостей:

| Шаг | Получатель / call |
|---:|---|
| 1 | `spPCErrorManager`, virtual20, четыре нулевых аргумента |
| 2 | engine inline `spTaskTimer +54`, virtual1C |
| 3 | `spInputManager +18` platform interface, virtual04 |
| 4 | `spNetworkManager` global75DB70,451830 |
| 5 | `spPhysicsManager` global75DB80,45A0C0 |
| 6 | `spAnimationManager` engine3C,4535A0 |
| 7 | `spGUIManager` engine44,451F70 |
| 8 | `spSceneManager` global75DB90,45A7D0 |
| 9 | audio engine40,virtual28; bootstrap factory — `spDXAudioManager` |
| 10 | `spParticleSystemManager` global75DB84,virtual2C |
| 11–12 | event queues engineCC, затем110:413B40 |
| 13 | `spPCAsyncFileStreamManager` global75DB94,virtual20 |

PC async update — inherited no-op48EAA0. Lazy actual factory6BD490 и release
выполнены, file requests не отправлялись. Actor Tick внутри шага6 имеет свою
flush110, так что наблюдение содержит **14 записей** для обычного actor frame:
нельзя ошибочно слить внутренний flush с engine tail flush.

Bootstrap **41B3B0** и registration constructor412FF0 независимо доказывают:

| Engine field | Factory | Original class |
|---:|---:|---|
| `3C` | `454640` | `spAnimationManager` |
| `40` | `4C4200` | `spDXAudioManager` |
| `44` | `452380` | `spGUIManager` |
| `48` | `4533A0` | `spCinematicManager` |
| `4C` | `4512D0` | `spNetworkManager` |

Это назначение class identity, не полный разбор всех этих менеджеров. Внешняя
связь manager→actor больше не неизвестна, но scene manager→world update→skin
render palette всё ещё требует собственной проверки.

## Таймеры и deltaB8

Два inline members engine54/90 — exact `spTaskTimer`3C, не два неизвестных
контейнера. Поле engineB8 равно `secondTimer+28` (delta seconds).
[Карточка таймера](native-class-sp-task-timer.md) содержит original methods/source.
Guest replay исполняет оба original constructors и Update; source/child-list
связь между ними задана **явной fixture**, пока original core ctor не закрыт.
Таким способом verified delta.25 действительно доходит до actual actor samples
.25/.5/.75. Это не доказательство всех игровых режимов паузы/ускорения времени.

## Графический этап41C460..41C4FD

Core сначала вызывает собственный virtual40 (**base41C210**); при false сразу
выходит. Затем по global75DBA0 byte20 может вызвать452D40. Concrete identity
этого global и engine50 в этом checkpoint не назначена.

Обход engine vector24..28 **не является двумя независимыми фильтрациями**:
пропускает элементы без Node bit100. У включённого элемента читает
`element+3C ->byte25`; пока byte ненулевой, вызывает element virtual40.
Первый enabled с byte25=0 **останавливает первый цикл**. После engine50 virtual24
обход продолжается с того же элемента до конца: теперь проверяется только bit100.
Enabled элементы с byte25=1, стоящие после границы, остаются во втором проходе.
Наблюдаемая роль элементов соответствует ранее найденным cameras, но camera
render bodies здесь seams: culling/draw не исполнялись.

В конце — core virtual44 (**base41C2A0**, game override возможен). Результат
нормализуется как `AL!=0`; renderer/device ошибки не вызывают автоматического
cleanup сверх реально пройденных вызовов.

Base begin41C210:

1. target-manager45D100(defaultCamera1C);
2. renderer secondary18 slot1(0), затем slot3();
3. при успехе slot5(7,engine38,0);
4. при успехе optional engine50 slot20();
5. optional callback30 без аргументов, его return игнорируется; true.

Base end41C2A0: target-manager45CC20, optional callback34 (return ignored),
renderer slot4(); только при успехе slot6(1), с нормализацией AL. Эти renderer
slot numbers — адресные интерфейсные операции; **не присвоены имена D3D API
без проверки конкретных downstream targets**.

## Проверки, ошибки и границы

- `probe_pc_engine_frame.py`: **134/134**, actual app/core update/manager/actor/
  SAN/node, foreground/owner/background routes, async lifecycle и отдельный
  actual-inline-timer вариант;
- `probe_pc_engine_render_frame.py`: **209/209**, begin/end short-circuits,
  порядок callbacks и двухфазного vector walk, disabled/empty-vector случаи;
- `inspect_pc_engine_frame.py`: **23/23**, SHA, direct CALLs, fields, timer
  vtable boundary и original registration/factory names;
- общий source build: CTest **12/12**, включая новый timer30 и1968 differential.

Первый frame assertion пропустил **nested actor flush** и был исправлен по
actual trace; не изменялся original code. Первый static byte anchor ожидал
long displacement у LEA, тогда как original использует short8-bit54; исправлен
только inspector. Все tracked original allocations в normal probes освобождены.

`spEngineCore` constructor41C7E0 отдельно остановился по исследовательскому лимиту:
100k, затем isolated500k и bounded continuation суммарно2M instructions/20s
в guest VM. Никаких OS calls/игры не запускалось; common harness limits не
повышались. Constructor не объявляется изученным, synthetic core storage явно
отделено от actual helpers. Полный OS loader/CRT и protected-VM прогресс оставлены
отдельным фронтом, не заменены догадочным constructor.

Portable `spTaskTimer` готов как исследовательский срез. Существующий portable
`spPCApp::Update` **пока не подключает** эти engine/backend calls; source comment
исправлен, но полный app/core frame перенос ещё предстоит. Также открыты native
event dispatcher, callbacks/reentry, полная загрузка
SMO/SAN/FAT и actual renderer submission.

Продолжение [checkpoint6 — scene/world](native-pc-scene-world.md) закрыло
original scene registration/owned plain-node tree и manager→world caller.
Setup41C300 оригинально связывает два таймера, создаёт scene/camera; отдельный
app→SAN→world test больше не делает forced UpdateWorld. Typed scene registration
side effects и portable full scene/runtime ещё не завершены.
