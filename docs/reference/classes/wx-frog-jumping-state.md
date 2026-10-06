# `wxFrogJumpingState`

`wxFrogJumpingState` (Class ID `47507873`) физически наследует
[wxCharacterState](wx-character-state.md). Native размер PC и PS2 — `44`,
selector — `1`. [Общие исходники](../../../Winx/Code/wxFrogJumpingState.h)
задают переносимое представление и не заявляют совместимость с native ABI.

| Собственная операция | PC | PS2 |
| --- | ---: | ---: |
| Entry | `520F10` | `2F4480` |
| Update | `520E50` | `2F4450` |
| Permission | `520E20` | `2F4590` |
| Event | `520E90` | `2F45E0` |

Собственные поля: byte `3C` и float `40`. Constructor записывает `40=375`,
но **не инициализирует `3C`**. Переносимый объект хранит для этого байта
`nullopt`; update до entry требует явно предоставленного значения и не
подменяет неопределённые байты native allocation нулём. Entry и оба события
могут установить значение. Clone создаёт начальные поля; унаследованные
пустой Copy и Reset собственные поля не переносят и не сбрасывают.

Entry обнуляет `3C`, задаёт `(key & F01F805F) | 50`, получает animation handle
и **всегда** выполняет mode-zero queue с interrupt, затем сохраняет pending.
Same-handle и null-handle не пропускают queue. Освобождения старого pending
и вызова update в собственном entry нет. Queue очищает matching completion
records и выбирает fade selector `2` либо `0` через предикат owner.

Update обнуляет word `4` прямого owner action control только при нулевом
`3C`; любой ненулевой байт оставляет word. Permission для code `A` либо
null pending сразу возвращает true. Остальные случаи вызывают consuming
completion query. Slot `38` возвращает false. Exit и остальные hooks
унаследованы от базового состояния.

Event получает имя из `event+1C -> tag+10`. Оригинал вызывает **`strstr`**:
совпадение `event_jump_begin` или `event_jump_end` возможно в любой позиции
строки, регистр учитывается. Begin проверяется первым и имеет приоритет,
если присутствуют обе подстроки. Begin записывает `(0,field40,0)` в control
`owner+124 -> entity+12C` на PC (`owner+130 -> entity+138` на PS2), включает
его byte flag и устанавливает `3C=1`. End только записывает `3C=0`;
скорость и flag control сохраняются. Несовпадение не меняет поля.

На PC velocity расположена в `1C8/1CC/1D0`, flag — `1D4`; на PS2 это
`1D4/1D8/1DC` и `1E0`. PC пишет Y, затем X и Z; PS2 — X, Y и Z.
[Обязательный host](../../../Winx/Analysis/Host/wxFrogJumpingStateHost.h)
предоставляет borrowed проекцию control и строку события. Это наша граница
объектного графа; реализация внешнего control и animation service не
восстановлена этим классом.

[Проверки](../../../Winx/Tests/wxFrogJumpingStateTests.cpp) охватывают полный
цикл entry → begin → update → end → update, повторный queue, completion
consumption, embedded/suffix matches, приоритет begin, clone/copy/reset и
неопределённое начальное поле. Численное сопоставление event квалифицировано
для конечных float32, включая signed zero и минимальный subnormal. Поведение
PC x87 для signaling NaN и полный игровой runtime остаются открытыми.
