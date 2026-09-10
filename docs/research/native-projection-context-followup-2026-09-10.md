# Уточнение оставшегося объекта при проверке Projection

Дополнение к неизменяемому [досье проекций](native-projection-family-2026-09-10.md).
[Отдельный контракт](../../research/projection-context-followup-2026-09-10.json)
сохраняет новую проверку; прежние sealed sources не изменены.

Оставшиеся 56 байт с vtable 006DC3B8 принадлежат **spDebugManager**.
Пятый слот этой таблицы — getter 0041D400 с точными байтами возврата record
0075DC08. PC каталог связывает этот record с spDebugManager и factory
0041E380. Оба прежних lifetime traces Box/Pyramid содержат исполнение этой
factory и единственную оставшуюся allocation указанного размера и vtable.

Прежняя независимая animation-проверка уже подтверждала lazy global 0075526C,
реальную 56-byte allocation DebugManager и reuse при втором объекте.
Теперь тип оставшегося Projection context установлен. Закрытие этого global
в конце игры по-прежнему не исполнено; оставшаяся allocation не объявляется
утечкой. Дополнительные проценты за эту идентификацию не начисляются.
