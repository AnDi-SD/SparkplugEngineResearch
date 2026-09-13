# Прямая камера через Remix bridge

Собственный транспорт камеры между x86 client и x64 server Remix. Параметры сборки и patch hashes находятся в [manifest.json](manifest.json); журнал испытаний хранится отдельно.

## Протокол


`camera-v1.patch` применяется к точному base
`b81a7b566b1eeb9edb4dc2b3c9d3972e0f253ad4`. Редактируемая копия находится в
`local-data/rtx-remix/direct-camera-bridge-work`. Это reference v1.5.2;
исходник installed `68edea01` в локальном shallow repository отсутствует.

В публичный interface client подключён `SetupCamera`; единственная новая
command `371` добавлена в конец enum. Все 372 существующих имени/значения
(включая специальный `Bridge_Terminate=65535`) сохранены. Она отправляется
по существующей DeviceBridge queue, без новой очереди или нового Present.

Используется настоящий общий `bridge_util::Serializable<CameraInfo,true>`
в `util_remixapi.h/.cpp`. Wire packet содержит ровно 140 bytes:

| Смещение | Содержимое |
|---:|---|
| 0 | uint32 declared byteSize = 140 |
| 4 | uint32 CameraInfo sType |
| 8 | uint32 camera type: WORLD / SKY / VIEW_MODEL |
| 12 | 16 исходных float32 view, 64 bytes |
| 76 | 16 исходных float32 projection, 64 bytes |

Pointers и native padding не передаются. Client отклоняет null info,
неизвестные sType/type, non-null pNext и NaN/Inf до отправки. Parametrized
extension не поддерживается. Server сначала потребляет один полный data item,
проверяет оба размера, затем использует общий serializer; output остаётся
нетронутым при отказе. Поддерживается unaligned wire address.

Сервер вызывает свой `g_remix.SetupCamera` и **всегда** возвращает его error
code через UID-matched `Bridge_Response`, независимо от опции
`sendAllServerResponses`. Client ждёт этот response с существующим ack timeout.
Если server API отсутствует — NOT_INITIALIZED; malformed camera — INVALID_ARGUMENTS.
SUCCESS означает возврат actual renderer API, который сам ставит работу в CS queue;
это ещё не доказательство точных GPU camera values. Потеря transport response
после отправки — **terminal fault: client пишет UID/result и завершает весь
host process через TerminateProcess с кодом `0xE052CA01`**, затем abort как
защита от неожиданного возврата TerminateProcess. Это не raster fallback и
не восстановление соединения. Поздний ответ UID N иначе остаётся в голове
очереди и блокирует следующие sync-команды N+1. Обычный server API error по
корректному UID потребляется и возвращается без termination; local invalid
input отклоняется до отправки. Политика ограничена новой camera-командой.

Оба компонента получили version `remix-main-camera-v1+b81a7b56`. Существующий
server startup сравнивает version аргумент client до установления канала и
отклоняет смешанную пару. SDK interface/stock renderer не менялись. Это
проверка протокольной пары, а не доказательство полной runtime совместимости.

Два Meson изменения относятся к сборке: отдельное имя версии и возможность
собирать client/server вместе с test targets, чтобы повторно использовать util
library. Wrapper выбирает только x86 client / x64 server и camera test каждого
типа; launcher, остальные configurations и весь renderer не собираются.


## Сборка

`Prepare-Bridge.ps1` подготавливает отдельную рабочую копию по закреплённой ревизии и проверяет патч. `Build-Bridge.ps1` собирает выбранные компоненты. Игровую совместимость пары и GPU-путь необходимо оценивать отдельно от успешной сборки.
