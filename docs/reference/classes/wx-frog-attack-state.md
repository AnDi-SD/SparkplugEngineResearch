# wxFrogAttackState

Class ID `4EE03D15`, selector `3`, физическая база `wxCharacterState`.
PC и PS2 constructor вызывают общую базу, затем задают собственную vtable,
selector и byte `3C = 0`. Native размер — `40` байт. Portable
[реализация](../../../Winx/Code/wxFrogAttackState.h) и отдельные
[PC](../../../Winx/Analysis/PC/wxFrogAttackStateAbi.h)/
[PS2](../../../Winx/Analysis/PS2/wxFrogAttackStateAbi.h) layout не смешиваются.

## Вход и completion

Каждый вход сначала заново читает borrowed receiver `owner24`. Ненулевой
receiver получает packet `271F,0,0,0,state,0,0,flag` с low flag byte `1`.
Только затем request key получает `(key & F0000400) | 400`, выполняется
animation lookup и очередь с mode=false, interrupt=true. Очередь вызывается
даже для null либо того же handle, прежний pending не освобождается.
После записи pending вход вызывает собственный виртуальный update,
который очищает action control word `4`, и возвращает true.

Permission `34` всегда вызывает consuming completion query, включая null
pending, и не зависит от code. Permission `38` возвращает false.

## Уведомления и animation tags

Объектное уведомление обрабатывает code `275C` только при ненулевом pending.
Оно устанавливает byte `3C = 1`, передаёт текущий consumer/handle внешнему
reverse-restart helper PC `4FB3A0` / PS2 `2A6BA0`, затем заново читает receiver
`owner24` и при его наличии отправляет packet `271F` с low flag byte `0`.
Чужой callback может изменить receiver; чтение выполняется после callback.
Исходный смысл поля `3C` сверх этих записей не установлен.

Animation event делает полное сравнение tag с `event_damage_end`. Только
совпадение отправляет тому же receiver packet `271F` с low flag byte `0`.
Оно не меняет собственный byte. Верхние 24 бита flag word исходник не
инициализирует: normalized
[host](../../../Winx/Analysis/Host/wxFrogAttackStateHost.h) передаёт лишь bool,
не приписывая padding нулевые значения. Original diagnostic/source имена
для packet, receiver и reverse helper остаются неизвестными.

Analytical hook `spBaseObject::vfunc_0C` обозначает notification. Его actual
PC table offset — `04`, PS2 — `0C`; actual PC offset `0C` принадлежит Copy.
Это различие не переносится на physical table layout.

Copy унаследован и не переносит поля. Clone создаёт свежие defaults,
общий Reset сохраняет собственный `3C`. Release/exit и остальные базовые
hooks используют общую реализацию состояния.

[WinxFrogAttackStateTests](../../../Winx/Tests/wxFrogAttackStateTests.cpp)
проверяет RTTI, Clone/Copy/Reset, ключи, completion, уведомления и порядок
callback. PC сравнение исполняет собственные state hooks и consuming
completion; внешние playback/predicate/receiver имеют явные fixtures.
PS2 проверена scalar prefixes/suffixes, собственными stores, packet padding,
virtual call arguments и границами foreign calls. Живая сцена и полный
связанный PS2 animation graph остаются открытыми.
