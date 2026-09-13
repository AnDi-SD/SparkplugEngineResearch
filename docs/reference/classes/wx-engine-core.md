# spEngineCore / wxEngineCore

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spEngineCore](../../../Sparkplug/Code/Sparkplug/spEngineCore.h), [wxEngineCore](../../../Winx/Code/wxEngineCore.h).

Статус: identity, inheritance, registration, factory/clone, обе vtable и
единственный class-local override подтверждены на PC и PS2. PS2 доказывает
точный размер `0x150` без добавленных полей. На PC защищённый factory скрывает
allocation, поэтому доказан полный inherited prefix `0x158`, но не заявлен
отдельный `sizeof` класса.

В доступных строках нет исходного пути `wxEngineCore.cpp` или header. Поэтому
`Winx/Code/wxEngineCore.*` — явно inferred размещение по уже доказанной границе
между библиотекой Sparkplug и кодом игры.

## Identity и layout

PS2 factory выделяет ровно `0x150`, а constructor `0x00285480` вызывает `spEngineCore` constructor `0x00133380`, заменяет две vtable и не пишет ни в одно новое поле. Это exact совпадение с размером base. PC factory entry уходит через защищённый `.rld` pointer `0x013B2548`; destructor и единственный override также не обращаются за границу base `0x158`.

PS2 constructor дополнительно вызывает `0x004088B0` со строкой
`"engine constructor"`. Это game-side instrumentation, а не поле класса.

## Vtable и lifetime

Primary/support vtable находятся по `0x007005F8/0x007005F4` на PC и
`0x00495AF0/0x00495B14` на PS2. Destructor `0x005739E0`/`0x00285410` только
устанавливает derived vptr и передаёт разрушение в `spEngineCore`. Support
thunk — `0x00573A20`/`0x003F9B10`; PC deleting wrapper — `0x00573A30`.

Сравнение всех 18 engine-core operations показывает ровно одну замену —
ordinal 10:

| Платформа | Base target | Derived target |
| --- | ---: | ---: |
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
