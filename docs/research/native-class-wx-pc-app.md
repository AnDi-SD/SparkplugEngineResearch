# `wxPCApp`: PC bootstrap игрового слоя

Статус: registration, exact factory allocation, обе vtable, clone/destructor,
оконные overrides и верхние границы initialize/update/shutdown восстановлены.
Содержимое полного constructor скрыто защитным trampoline, а десятки manager
calls пока обозначены адресами, не придуманными именами.

Контрольный файл — `local-data/pc-pristine/WinxClub.exe`, SHA-256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Строки точного `.cpp` нет. Частичный PDB path подтверждает лишь game root
`Z:\Winx PS2\CODE\Build\PC\Release\WinxPC.pdb`; поэтому
`Winx/Code/PC/wxPCApp.*` помечен как inferred.

## Identity и allocation

Initializer `0x006D1450` регистрирует:

| Поле | Значение |
|---|---:|
| class name | `wxPCApp` @ `0x006DAEAC` |
| class ID | `0x707D09F3` |
| base | `spPCApp / 0x7635EFDE` |
| registration | `0x007552A8` |
| getter | `0x0040DE10` |
| factory | `0x0040E080` |
| property callback | `0` |
| primary/support vtable | `0x006DADD0 / 0x006DADCC` |

Factory содержит прямую константу `operator new(0x2CC)`, затем передаёт объект
в protected constructor `0x00528280`. Поэтому полный allocation extent точен,
даже несмотря на недоступный constructor body. В
`wxPCAppAllocationLayout` размечены только offsets, которые читают/пишут
видимые методы:

- `+0x84..+0x90` — четыре координатных значения из window messages;
- `+0x94/+0x95/+0x96` — byte flags; shutdown ставит `+0x96 = 1`;
- `+0x97` — начало лениво формируемой строки window title;
- `+0x160/+0x164/+0x168` — три указателя, установленные initialize;
- остальные байты остаются `opaque`, без фиктивных ролей.

Наблюдаемое начало `spPCApp` до `+0x83` и первый wx-specific access `+0x84`
согласуются с границей base `0x84`, но это пока structural inference, а не
прямая allocation инструкция абстрактного base.

## Vtable

Primary slots после общей object-части:

| Slot | Target | Наблюдаемая роль |
|---:|---:|---|
| `+0x1C` | `0x0040E250` | lazy title `Winx PC Version %s - %s` |
| `+0x20` | `0x0040E2F0` | game bootstrap/Initialize |
| `+0x24` | `0x0040E610` | frame Update |
| `+0x28` | `0x0040E180` | game shutdown, затем `spPCApp` |
| `+0x2C` | `0x004C2C70` | inherited Win32 message loop |
| `+0x30` | `0x0048EAA0` | inherited no-op pre-update |
| `+0x34` | `0x0040E1D0` | заполнить window/display configuration |
| `+0x38` | `0x004B9100` | inherited false stub |
| `+0x3C` | `0x0040E760` | обработать Win32 message, затем base hook |
| `+0x40/+0x44/+0x48` | inherited targets | пока без original signatures |

Deleting destructor `0x0040E160` вызывает non-deleting `0x0040DE20` и
освобождает `this`; support thunk `0x0040E070` корректирует `this-0x14`.
Clone `0x0040E110` создаёт новый объект factory-путём, регистрирует пару в
clone manager и вызывает inherited copy slot. Это подтверждает concrete factory
contract, но не означает копирование глобальных manager state.

## Поведение bootstrap

`0x0040E1D0` задаёт структуру `1024 x 768`, два enum-значения `2`, video-mode
byte из глобальной конфигурации, синхронно обновляет base fields `+0x60/+0x64`
и передаёт глобальный native-window handle.

`0x0040E2F0` получает/создаёт singleton managers, записывает interface pointers
в `this+0x160..+0x168`, настраивает `spPCErrorManager`, вызывает base
`spPCApp::Initialize` `0x004C3230`, связывает engine/game services и завершает
с `+0x96 = 0`. Именно этот caller дополнительно подтверждает, что factory
`0x004C33C0` принадлежит error manager, не `spPCApp`.

`0x0040E610` обслуживает OS/input/game managers и только затем вызывает
`spPCApp::Update` `0x004C2D60`; failure base-вызова возвращает false.
`0x0040E760` разбирает сообщения `3..8`, обновляет координаты/flags и всегда
делегирует неизвестный или завершённый случай base hook `0x004C2D20`.

Полные имена manager functions не назначаются до восстановления их классов.
Portable `wxPCApp` сохраняет identity/title/shutdown/clone contract, но
возвращает false из initialize/update без backend — это явная безопасная
граница, а не имитация успешно запущенной игры.

## Открыто

1. Exact translation-unit/header и source-level namespace.
2. Тело protected constructor `0x00528280` и точные роли всех `0x2CC` байт.
3. Классы/владельцы каждого global singleton в initialize/update/shutdown.
4. Полные signatures window hooks и message enum.
