# wxSpiderHurtState

Состояние повреждения паука с selector `10`, Class ID `54F716CC`.
Физическая база — `wxCharacterState`: собственный constructor вызывает
constructor базы перед установкой vtable и новых runtime-полей.
Native размер — `50` байт на PC и PS2.
[Общая реализация](../../../Winx/Code/wxSpiderHurtState.h) хранит два byte-флага
`3C/3D`, word `40`, delay `44`, deadline `48`, permission byte `4C`.
Constructor устанавливает `false, false, 0, 10000, 0, true` соответственно.
Исходные имена этих полей неизвестны.

## Вход, update и выход

Первый entry по once-флагу `1C` сначала очищает key bits `23..27`, затем
вызывает consumer rate service с `1.0f`. Он очищает flag `3C`, переписывает
key `(key & FFA08000) | 208000`, выполняет lookup и mode-zero запуск,
сохраняет новый pending, очищает permission `4C` и once-флаг `1C`.
Предыдущий pending перед этим не освобождается. В конце action control
word владельца очищается, entry возвращает false.

Повторный entry выполняет consuming completion query, включая null pending.
Если completion есть, flag `3D` условно устанавливает flag `3C`, затем
вызывается общий entry: release pending, виртуальный update, true.
При незавершённой animation очищается action control и возвращается false.

Update сначала очищает action control. Первый update по флагу `1D`
переписывает key `(key & FF808000) | 8000`, выполняет lookup и mode-one
запуск без release старого pending, сохраняет новый pending, устанавливает
`deadline48 = clock + delay44` с unsigned wrap и очищает once-флаг `1D`.
Следующие update при строгом unsigned `deadline48 < clock` устанавливают
permission `4C` и flag `3C`. Равенство оставляет оба byte прежними.

Первый exit по флагу `1E` использует key `(key & FFC08000) | 408000`.
Lookup выполняется до release предыдущего pending. Затем mode-zero запуск,
запись pending, очистка `1E`, очистка action control и false.
Повторный exit при consuming completion вызывает общий release и true;
эта завершённая ветвь сама action control не очищает.

Release использует общий stop/fade выбор по predicate owner; запуск сохраняет
interrupt=true и fade selector `0` либо `2`. Lookup, consumer rate, playback
и часы принадлежат заимствованным службам и отделены через обязательный
[host](../../../Winx/Analysis/Host/wxSpiderHurtStateHost.h).

## Permission и жизненный цикл

Hook `34` читает owner graph: PC `owner124 -> object138 -> object124`,
PS2 `owner130 -> object144 -> object130`. Если последний указатель null,
возвращается true; иначе — текущий permission `4C`. Промежуточные объекты
должны существовать по исходному контракту. Hook игнорирует входной code.
Унаследованный `38` возвращает false; собственного event handler нет.

Clone создаёт constructor defaults, регистрирует пару и вызывает пустой Copy,
runtime-состояние не переносится. Общий reset сохраняет все собственные поля.
Остальные inherited hooks используют общие release/update операции.

Проверка — `WinxSpiderHurtStateTests`, оригинальные PC hooks/common helpers
и consuming completion, с явными fixtures внешних служб. PS2 проверена
отдельными scalar prefixes/suffixes, включая собственный permission return.
Полная живая сцена и связанный PS2 animation graph остаются вне проверки.
Отсутствующий adapter явно отклоняется.
