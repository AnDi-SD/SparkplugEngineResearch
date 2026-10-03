# wxDroidHurtState

Состояние повреждения дроида, selector `10`, Class ID `497B5830`.
Физическая база — `wxCharacterState`: constructor вызывает constructor базы,
после чего устанавливает собственную vtable и два нулевых word `3C/40`.
Native размер на PC и PS2 — `44` байта. Переносимая
[реализация](../../../Winx/Code/wxDroidHurtState.h) хранит эти runtime-поля
в самом состоянии; точное совпадение ABI с современным C++ не заявляется.

## Вход и разрешение перехода

Каждый entry переписывает request key `(key & FF808000) | 8000`.
Затем поле key `23..27` получает значение из byte `60` action control
владельца: PC использует `byte != 0`, PS2 — `byte & 31`.
Byte очищается до lookup animation. Различие платформ сохраняется через
выбор numeric profile в [host](../../../Winx/Analysis/Host/wxDroidHurtStateHost.h).

Entry запускает animation с mode=false, interrupt=true, сохраняет новый
pending и устанавливает word `3C = clock + 1500` с unsigned wrap,
word `40 = 0`, once-флаг `1C = false`. Старый pending перед этим
не освобождается; once-флаг не ограничивает повторный entry.

Если у owner есть заимствованный receiver `24`, ему отправляется сообщение
из восьми native word: `27D1, 0, 0, 0, this, 0, 6C, 0`.
Portable record передаёт указатель source отдельно, не обрезая его на x64.
В конце action control word `4` очищается, entry возвращает true.
Часы и receiver — внешние службы, а создание сообщения и изменения
runtime-полей выполняет само состояние.

Permission `34` всегда вызывает consuming completion query, в том числе
при null pending. Он не освобождает pending и игнорирует входной code.
Унаследованный `38` возвращает false. Собственного event handler нет.

## Update, выход и жизненный цикл

Update сначала очищает action control word. При once-флаге `1D` он
очищает флаг, вызывает общий prepare movement с clearCache=false,
а затем всегда вызывает общий finish movement с verticalOnly=false.
Узел `movement_tracker`, cache и предыдущая позиция принадлежат общей базе;
host предоставляет заимствованные узлы и layout соответствующей платформы.
При отсутствии узла finish завершается без изменения target.

Exit сначала очищает action control, затем вызывает общий exit с release
pending и true. Остальные inherited hooks используют общие release/update
операции, reset очищает поля базы и сохраняет собственные word `3C/40`.
Clone создаёт новый экземпляр, регистрирует пару и вызывает пустой Copy;
runtime-поля и привязки в clone остаются constructor defaults.

Проверка — `WinxDroidHurtStateTests`: жизненный цикл, записи состояния,
message words, ключи, completion и movement. PC сравнение исполняет
собственные оригинальные hooks и общие movement/matrix helpers; внешние
animation, owner predicate, finder и message receiver имеют явные fixtures.
PS2 проверена отдельными scalar prefixes/suffixes вокруг внешних вызовов.
Полная живая сцена и связанный PS2 animation/movement graph не проверены.
Отсутствующий host явно отклоняется.
