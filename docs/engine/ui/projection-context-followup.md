# Уточнение оставшегося объекта при проверке Projection

Оставшиеся 56 байт с vtable 006DC3B8 принадлежат **spDebugManager**.
Пятый слот этой таблицы — getter 0041D400 с точными байтами возврата record
0075DC08. PC каталог связывает этот record с spDebugManager и factory
0041E380. Оба прежних lifetime traces Box/Pyramid содержат исполнение этой
factory и единственную оставшуюся allocation указанного размера и vtable.
