# wxDefendingState

Class ID `38E64587`, физическая и регистрационная база `wxCharacterState`.
Native размер PC/PS2 — `54`, selector — `8`. Конструктор обнуляет собственные
поля; padding не считается полем. Clone получает constructor defaults,
базовые Copy и Reset сохраняют собственные поля. Исходники —
`Winx/Code/wxDefendingState`, проверка — `WinxDefendingStateTests`.

| Поле | Наблюдаемое назначение |
| --- | --- |
| `3C` | Заимствованный node `shield_master` |
| `40` | Заимствованный child `shield_end` |
| `44` | Заимствованный child `shield_hit` |
| `48` | Накопленное float32 время выхода |
| `4C` | Uint32 срок отключения hit node |
| `50` | Byte, удерживающий анимацию до completion |

Невиртуальная инициализация ищет `shield_master` через owner `18`, затем
два child через master; поиск получает `(name,true,false)`. Нулевой master
сохраняет прежние child pointers. При найденном master отключает его вызовом
node virtual `(false,true)` и вызывает particle service для `ptc`. Original
имена методов неизвестны; `InitializeNodesForAnalysis`, `SetNodesEnabledForAnalysis`
и `AdvanceExitNodesForAnalysis` — аналитические имена, не исходные символы.

Включение master вызывает его node virtual `(true,true)`, затем end и hit
`(false,false)`. При low nibble owner word `140` PC / `14C` PS2, равном `0`
или `2`, position master становится `(0,75,-40)`. Иначе ищет `SubMaster` и
копирует его position; отсутствие этого node задаёт `(0,75,0)`. Position
находится в `20/24/28`; dirty word (`B0` PC / `B4` PS2) получает bit `1`.
Затем owner `24` получает сообщение `27D1` с payload `(7,0)`. Master требует
валидных end/hit при включении; успешная обработка нулевых child не придумана.

Отключение master с silent=false передаёт `27D2 (7,0)` и `27D1 (8,0)`, затем
node virtual `(false,true)` и particle lookup/invoke для `ptc`. Silent=true
пропускает сообщения. Нулевой master пропускает всю операцию.

Первый вход `1C` включает узлы, обнуляет `50` и `4C`, задаёт
`(key & F0207F8F) | 00200000`, выполняет lookup, ставит consumer speed `2.0f`
и запускает mode=false, interrupt=true. Старый pending здесь не освобождается.
После записи handle и очистки flag `1C` обнуляет прямой control word `4` и
возвращает false. Повторный вход потребляет completion; при завершении ставит
speed `1.0f` и вызывает базовый вход с виртуальным update. При незавершении
также обнуляет control word `4` и возвращает false.

Update `30` сначала захватывает прямой control (`owner12C` PC / `owner138` PS2),
применяет `F007FF8F` и делает lookup. При совпадении handle playback пропускает.
Изменённый handle при ненулевом `50` ждёт consuming completion. После разрешения
записывает `50 = ((key & 78000) == 8000)`. Если прежний `50` был нулевым либо
новый стал нулевым, освобождает pending с forceStop=true. Запускает новый handle
с mode, обратным новому `50`, и interrupt=true. Удержание ненулевого byte
сохраняется при незавершении; не нормализуется до `1` без смены animation.

При ненулевом hit node и control byte `5C` update передаёт `27D1 (23,0)`,
включает hit `(true,true)`, передаёт `2719 (0,0)`, очищает захваченный byte,
сохраняет `4C = clock + 300` с uint32 переполнением и вызывает внешний scalar
service `(0,1,1,0.5)`. При нулевом byte, ненулевом сроке и `clock > 4C`
отключает hit `(false,true)`. Срок после этого не обнуляет, поэтому последующие
update повторяют отключение. В конце перечитывает control владельца и обнуляет
его word `4`; пропуск playback не отменяет эту операцию.

Первый выход `20` освобождает pending, обнуляет `48`, задаёт
`(key & F040018F) | 00400180`, делает lookup, ставит speed `2.0f`, запускает
mode=false, interrupt=true, сохраняет handle и очищает flag `1E`. Пока exit
animation не завершена, продвигает узлы выхода, очищает control word `4` и
возвращает false. При completion отключает master с silent=false, ставит
speed `1.0f` и возвращает базовый выход.

Продвижение выхода использует **общий для экземпляров** uint32 stage.
При прежнем `48 == 0`, включая negative zero, обнуляет stage. Прибавляет
clock delta к `48`; значение ниже float32 `0.17` пропускает дальнейшие действия.
Stage `0` включает end `(true,true)`, задаёт scale `30/34/38` равным `1`,
помечает dirty и увеличивает общий stage. Stage `1` отключает end при его bit
`200`; если bit отсутствует, включает end и увеличивает stage. Stage `2`
отключает master с silent=true только при его bit `200`. Другие stage ничего
не меняют; значения не ограничиваются диапазоном `0..2`.

PC сравнивает сумму до её float32 spill, используя x87 nearest64. Узкий helper
`Analysis/PC/wxDefendingNumeric.h` сохраняет решение около `0.17`, включая
half-ULP tie; это не общий эмулятор x87. PS2 сначала округляет `ADD.S`, затем
сравнивает. PC подтверждённый quiet NaN не выполняет ветвь «меньше»; профиль
PS2 здесь ограничен конечными scalar inputs. Для PC stage `0` dirty записывается
перед увеличением stage, для PS2 — после.

`wxDefendingStateHost` явно отделяет owner/control/consumer/node, общий stage,
clock, particle и scalar services. Объекты заимствованы; layout не выдаётся за
native ABI. Подключение этих служб и полного игрового жизненного цикла остаётся
открытым. Permission `34` возвращает true; собственные методы подтверждены
в описанном объёме, полное игровое соответствие класса ещё не закрыто.
