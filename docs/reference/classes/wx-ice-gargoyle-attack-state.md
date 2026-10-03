# `wxIceGargoyleAttackState`

`wxIceGargoyleAttackState` (Class ID `1DE24CB8`) — физический потомок [wxCharacterState](wx-character-state.md), selector `3`, native size `40`. [Реализация](../../../Winx/Code/wxIceGargoyleAttackState.cpp) содержит подтверждённый собственный byte `3C`; соседние native bytes `3D..3F` — padding, не переносимые поля. Constructor обеих платформ задаёт `3C=0`. Переносимый объект не задаёт native ABI.

| Операция | PC | PS2 |
| --- | ---: | ---: |
| Update, PC slot `30` | `519DA0` | `2EE890` |
| Permission, PC slot `34` | `520E20` | `2EEB50` |
| Event, PC slot `3C` | `519D00` | `2EE9C0` |

Первый update при `1D=true` обнуляет собственный byte `3C` и очищает `1D` перед преобразованием запроса. Каждый update задаёт `(key & F007FFD1) | 51`, делает lookup и при changed handle выполняет mode-zero queue с interrupt → store pending, **без release старого pending**. Control word не сбрасывается. Базовый entry освобождает pending перед виртуальным update; mode-zero queue очищает matching completion records.

Permission для кода `A` или null pending возвращает `true` без completion query. В остальных случаях consuming query определяет результат и потребляет matching records. Pending этим hook не освобождается.

Event читает borrowed имя `event+1C -> tag+10` и сравнивает целиком, case-sensitive:

| Имя | Действие |
| --- | --- |
| `event_shoot` | Если controller ненулевой, virtual PC slot `38` (PS2 `40`) с аргументом `0`; затем `3C=1`, включая null controller |
| `event_damage_begin` | Receiver `owner+24`, если ненулевой: `271F`, name `hand_right`, flag byte `1` |
| `event_damage_end` | То же сообщение с flag byte `0` |

Controller получается по `owner+124 -> entity+140` на PC, `owner+130 -> entity+14C` на PS2. Вызов controller наблюдает прежний byte `3C`; установка единицы происходит после callback. Damage events не меняют этот byte.

Damage packet: `271F,0,0,0,source,0,name,flagWord`. Name — стабильный literal `hand_right` (PC `6F58C8`, PS2 `463F88`). Из flagWord известен только младший byte; три старших padding bytes оригинал не инициализирует. Обязательный [wxIceGargoyleAttackStateHost](../../../Winx/Analysis/Host/wxIceGargoyleAttackStateHost.h) передаёт известный bool и borrowed name без утверждения о неизвестных bits.

Reset базового класса снова задаёт `1D=true`, но сам `3C` не меняет; byte очищается последующим update. Copy пустой и не переносит собственный byte. Clone создаёт constructor default `3C=0`. Остальные hooks унаследованы.

[Проверка](../../../Winx/Tests/wxIceGargoyleAttackStateTests.cpp) охватывает callback → flag, null controller, update/Reset/Clone, damage notifications и несовпадения имён. Неизвестный payload padding, реальные borrowed adapters и полный игровой runtime остаются открытыми.
