# `wxTrixAttackState`

`wxTrixAttackState` (Class ID `27590633`) — физический потомок [wxCharacterState](wx-character-state.md), selector `3`, native size `3C`, без дополнительных полей. [Реализация](../../../Winx/Code/wxTrixAttackState.cpp) сохраняет собственные update, permission и event hooks. Переносимый объект не задаёт native ABI.

| Операция | PC | PS2 |
| --- | ---: | ---: |
| Update, PC slot `30` | `520330` | `313050` |
| Permission, PC slot `34` | `520100` | `313620` |
| Event, PC slot `3C` | `520150` | `313170` |

Update задаёт `(key & F007FF81) | 1`, выполняет lookup и при changed handle вызывает mode-zero queue с interrupt → store pending. Старый pending здесь не освобождается; control word не очищается. Базовый entry освобождает pending перед виртуальным update. Mode-zero queue очищает matching completion records.

Permission для кодов `1C`, `11`, `A` и любого null pending возвращает `true` без completion query. В остальных случаях consuming query определяет результат и потребляет matching completion records. Сам hook не освобождает pending.

Event читает borrowed имя `event+1C -> tag+10`, выполняет полные case-sensitive сравнения в следующем порядке и при совпадении читает borrowed receiver `owner+24`. Null receiver пропускает dispatch.

| Имя | Код | Payload |
| --- | --- | --- |
| `event_lightning` | `2778` | `0,0` |
| `event_spiral` | `2777` | `0,0` |
| `event_iceshard` | `2779` | `0,0` |
| `event_thunder` | `277A` | `0,0` |
| `event_icemine` | `277C` | `0,0` |
| `event_circles_begin` | `277D` | known byte `1`, second word `0` |
| `event_circles_end` | `277D` | known byte `0`, second word `0` |
| `event_shieldbubble` | `277E` | `0,0` |
| `icy_freeze` | `2781` | `0,0` |

Packet: `code,0,0,0,source,0,payload0,payload1`. У обоих circles events оригинал задаёт только младший byte `payload0`; три старших padding bytes не инициализированы. Их значение неизвестно. PC использует inline builder для lightning и общий `435D30` для остальных событий; PS2 строит packets inline с переносом слов через FPU registers, без арифметики.

Обязательный [wxTrixAttackStateHost](../../../Winx/Analysis/Host/wxTrixAttackStateHost.h) отделяет borrowed event/receiver и доставку. Для circles он передаёт известный bool, а не придуманный полный native word. Constructor/RTTI/factory, default Clone, пустой Copy, Reset и остальные hooks используют общий протокол.

[Проверка](../../../Winx/Tests/wxTrixAttackStateTests.cpp) охватывает entry, completion-reset/consumption/bypass, все девять событий, null receiver и несовпадения по suffix, truncation и регистру. Borrowed delivery, неизвестный padding и полный игровой runtime остаются открытыми.
