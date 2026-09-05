# `wxEngineCore`: тонкая игровая надстройка над `spEngineCore`

Статус: identity, inheritance, registration, factory/clone, обе vtable и
единственный class-local override подтверждены на PC и PS2. PS2 доказывает
точный размер `0x150` без добавленных полей. На PC защищённый factory скрывает
allocation, поэтому доказан полный inherited prefix `0x158`, но не заявлен
отдельный `sizeof` класса.

## Контрольные бинарники

| Платформа | Файл | SHA-256 |
|---|---|---|
| PC | `local-data/pc-pristine/WinxClub.exe` | `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` |
| PS2 | `local-data/Winx Club the game PS2/SLES_532.19` | `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE` |

В доступных строках нет исходного пути `wxEngineCore.cpp` или header. Поэтому
`Winx/Code/wxEngineCore.*` — явно inferred размещение по уже доказанной границе
между библиотекой Sparkplug и кодом игры.

## Identity и layout

| Поле | PC | PS2 |
|---|---:|---:|
| Class ID | `0x34B85918` | `0x34B85918` |
| Base | `spEngineCore / 0x0E9F6B8C` | `spEngineCore / 0x0E9F6B8C` |
| Registration | `0x007545A8` | `0x00472778` |
| Initializer | `0x006D0DF0` | `0x0048B7F0` |
| Factory | entry `0x004073A0` | `0x003E7D60` |
| Registration getter | `0x005739D0` | `0x003F88B0` |
| Property callback | `0` | `0` |

PS2 factory выделяет ровно `0x150`, а constructor `0x00285480` вызывает
`spEngineCore` constructor `0x00133380`, заменяет две vtable и не пишет ни в
одно новое поле. Это exact совпадение с размером base. PC factory entry уходит
через защищённый `.rld` pointer `0x013B2548`; destructor и единственный override
также не обращаются за границу base `0x158`. Для PC это сильное свидетельство
отсутствия state, но не прямое доказательство allocation size.

PS2 constructor дополнительно вызывает `0x004088B0` со строкой
`"engine constructor"`. Это game-side instrumentation, а не поле класса.

## Vtable и lifetime

Primary/support vtable находятся по `0x007005F8/0x007005F4` на PC и
`0x00495AF0/0x00495B14` на PS2. Destructor `0x005739E0`/`0x00285410` только
устанавливает derived vptr и передаёт разрушение в `spEngineCore`. Support
thunk — `0x00573A20`/`0x003F9B10`; PC deleting wrapper — `0x00573A30`.

Clone `0x0040D300`/`0x003E7CA0` создаёт concrete `wxEngineCore`, регистрирует
пару в clone manager и выполняет пустой inherited base-copy. Собственного
состояния для переноса нет.

Сравнение всех 18 engine-core operations показывает ровно одну замену —
ordinal 10:

| Платформа | Base target | Derived target |
|---|---:|---:|
| PC | `0x0041C2A0` | `0x00573A50` |
| PS2 | `0x00131750` | `0x00285200` |

Остальные 17 targets наследуются буквально. Это отделяет game frame-boundary
от общего bootstrap: `wxEngineCore` не переопределяет создание renderer,
scene/camera, initialize или teardown.

## Единственный override

Обе реализации временно заменяют renderer/global field `+0x28` значением из
game manager `+0x40`, проверяют game-flow states `0x36` и `0x4E`, обрабатывают
текущий элемент через offsets `+0x1AC/+0x15C`, вызывают последовательность
Winx-specific managers, затем вызывают точный base slot и восстанавливают
исходное поле renderer. Возвращается результат base-вызова.

Исходное имя метода, concrete типы globals и точные сигнатуры внутренних
вызовов пока неизвестны. Portable seam `RunFrameBoundaryForAnalysis` поэтому
фиксирует лишь доказанный порядок «game actions -> base boundary» и не выдаёт
аналитические названия за native API.

## Реконструкция и проверка

Добавлены `Winx/Code/wxEngineCore.h/.cpp`, PC/PS2 evidence-константы и layouts,
регистрация/clone и тест порядка вызовов. `WinxBootstrap` теперь зависит от
отдельного target `SparkplugEngine`, отражая реальное наследование.

Изолированная Windows x64 сборка проходит `WinxAppTests`, `SparkBaseTests` и
`SparkplugEngineTests` (3/3). Она не запускает игру и не изменяет local data.

## Явно открыто

- original source/header path, namespace и имя единственного override;
- прямой PC allocation size и защищённое constructor body;
- concrete registration/имена Winx manager globals в override;
- тип временно заменяемого renderer field `+0x28`;
- условия и семантика states `0x36/0x4E` и offsets `+0x1AC/+0x15C`;
- точный secondary-this adjustment на обеих платформах.

Следующий последовательный слой — managers, которые создаёт общий
`spEngineCore` и вызывает единственный `wxEngineCore` override. Первым
выбирается небольшой общий `spErrorManager`, уже имеющий точный PC source-path
anchor и прямые связи со stream/bootstrap ветками.
