# wxIceWormAttackState

Class ID `709E322B`, регистрационная и физическая база —
[wxCharacterState](wx-character-state.md). Native размер PC/PS2 — `0x3C`,
selector — `3`, собственных полей нет. Constructor и clone задают базовые
defaults; унаследованный Copy пуст. Исходники —
[wxIceWormAttackState.h](../../../Winx/Code/wxIceWormAttackState.h) и
[wxIceWormAttackState.cpp](../../../Winx/Code/wxIceWormAttackState.cpp).
Точные раскладки и адреса методов отделены в
[PC ABI](../../../Winx/Analysis/PC/wxIceWormAttackStateAbi.h) и
[PS2 ABI](../../../Winx/Analysis/PS2/wxIceWormAttackStateAbi.h).

## Переходы и анимация

Вход `1C` и выход `20` сначала проверяют `(key >> 7) & FF`. Значения `8` и
`10` сразу вызывают соответствующую базовую операцию; одноразовые флаги
перехода в этом случае не читаются и не очищаются.

Первый обычный вход передаёт заимствованному получателю `owner24` directed
notification `27D1` с payload words `(6F,0)`. Нулевой получатель пропускает
доставку. Затем задаёт `(key & F0387FDF) | 00200050`, делает lookup,
без освобождения старого pending запускает mode=false, interrupt=true,
сохраняет новый handle, очищает flag `1C` и возвращает false.

Повторный вход потребляет completion-записи текущего handle, включая null.
При незавершении возвращает false; при завершении вызывает базовый вход:
release pending, виртуальное обновление `30`, true.

Первый обычный выход освобождает pending до изменения caller key, задаёт
`(key & F0587FDF) | 00400050`, делает lookup и запускает mode=false,
interrupt=true. Сохраняет handle, очищает flag `1E` и возвращает false.
Повторный выход потребляет completion. При завершении сначала передаёт
`owner24` directed `27D2 (6F,0)`, затем вызывает базовый выход с release.

Update `30` задаёт `(key & FF987FDF) | 50` и делает lookup. Совпавший с
pending handle ничего больше не меняет. При другом handle освобождает pending,
повторно читает caller key после внешних callbacks и выбирает mode=false
для селекторов `8/10`, mode=true для остальных. Interrupt всегда true.
Запись нового pending происходит после запуска анимации.

## Разрешение и событие

Hook `34` читает сырой packed word владельца: PC `+140`, PS2 `+14C`.
При `(word & 7F80) == 400/500` возвращает результат consuming completion
query, включая null handle; иначе true. На PS2 слово проходит через
LWC1/SWC1 без float arithmetic: это перенос bits, а не численное сравнение.
Hook `38` унаследован и возвращает `0`.

Точное C-string имя `event_shoot` вызывает directed notification `2758`
получателю `owner24`. Восемь packet words — `(2758,0,0,0,state,0,0,0)`.
Иные имена не вызывают уведомлений. Переходные packets имеют те же нулевые
metadata, source=state и payload `(6F,0)`.

## Границы

[wxIceWormAttackStateHost](../../../Winx/Analysis/Host/wxIceWormAttackStateHost.h)
— наш обязательный adapter для borrowed owner graph, C-string имени события
и доставки notifications. Он не передаёт владение внешними объектами состоянию.
Lookup, completion и playback используют общий host базового состояния;
успешных реализаций внешних служб по умолчанию нет.

Собственные ветви PC исполняются с оригинальными базовыми переходами и
completion consumer. Для PS2 отдельно проверены скалярные окна масок,
селекторов, packets, consuming query и порядка записей. Это не полная
эмуляция EE и не замыкание внешней системы анимации. Исходные имена hooks и
семантика payload `6F` остаются неизвестными.
