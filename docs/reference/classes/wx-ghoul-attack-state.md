# `wxGhoulAttackState`

`wxGhoulAttackState` (Class ID `08A40E12`) — физический потомок [wxCharacterState](wx-character-state.md), selector `13` (19 в десятичной записи), native size `3C`, без дополнительных полей. [Реализация](../../../Winx/Code/wxGhoulAttackState.cpp) сохраняет собственные entry, permission и event hooks. Переносимый объект не задаёт native ABI.

| Операция | PC | PS2 |
| --- | ---: | ---: |
| Entry, PC slot `1C` | `517B90` | `2E4430` |
| Permission, PC slot `34` | `5179C0` | `2E4870` |
| Event, PC slot `3C` | `517A10` | `2E4540` |

Entry задаёт `key & F007FF8F`, выполняет lookup → mode-zero queue с interrupt → store pending → `true`. Он **не освобождает старый pending и не сравнивает handles**, поэтому повторный вход с тем же handle снова сбрасывает completion records и запускает playback. Control word и once-флаги не меняются. Update унаследован и пустой; exit использует базовый release.

Permission для кодов `9`, `A` или null pending возвращает `true` без query. В остальных случаях consuming completion query определяет результат и потребляет matching records. Сам hook не освобождает pending.

Event читает borrowed имя `event+1C -> tag+10` и сравнивает целиком, case-sensitive, в следующем порядке:

| Имя | Действие |
| --- | --- |
| `event_impact_begin` | `271F`, name `hand_left`, flag byte `1` |
| `event_impact_end` | `271F`, name `hand_left`, flag byte `0` |
| `event_kick_begin` | `271F`, name `L_Toe`, flag byte `1` |
| `event_kick_end` | `271F`, name `L_Toe`, flag byte `0` |
| `event_air_begin` | Controller float service `513A30` с аргументом `200.0f` |
| `event_throw` | Controller virtual PC slot `38` (PS2 `40`) с аргументом `0`, затем packet `2754,0,0,0,source,0,0,0` |

Все сообщения адресованы borrowed receiver `owner+24`, если он ненулевой. У throw receiver читается **после** controller callback. Named packet: `271F,0,0,0,source,0,name,flagWord`. Из flagWord известен только младший byte; три старших padding bytes не инициализированы. Names — стабильные literals, `L_Toe` сохраняет исходный регистр.

Air controller получается по `owner+124 -> entity+12C` на PC, `owner+130 -> entity+138` на PS2. PC `513A30` записывает words `1C8/1CC/1D0=(0,value,0)` и byte `1D4=1`. PS2 делает это inline в words `1D4/1D8/1DC` и byte `1E0`. Остальные bytes, включая старшие bytes слова флага, не меняются. Throw controller получается по `entity+140` на PC, `entity+14C` на PS2; эта ветвь не имеет null guard и требует действительный внешний controller.

Обязательный [wxGhoulAttackStateHost](../../../Winx/Analysis/Host/wxGhoulAttackStateHost.h) отделяет borrowed objects, службу float, virtual slot и доставку сообщений. Он передаёт известный bool, не придумывая полный flagWord. Constructor/RTTI/factory, default Clone, пустой Copy, Reset и прочие hooks используют общий протокол.

[Проверка](../../../Winx/Tests/wxGhoulAttackStateTests.cpp) охватывает повторный same-handle entry, consuming/bypass permission, шесть событий, null receiver и порядок throw callback → receiver read → dispatch. Неизвестный padding, реальные borrowed adapters и полный игровой runtime остаются открытыми.
