# PC: чтение ссылок, inline materialization и кэш

## Адреса и точный порядок

Имена API здесь аналитические; exact TU — `spSerializer.cpp`. Вторичный
interface пока без исходного имени. Обёртка4678B0 сначала вызывает actual
ErrorManager gate, читает u32 ID из **второго** аргумента; первый аргумент,
expected class ID, вообще не используется. Третий аргумент передаётся в
resolver и обслуживает size/header/payload. Обычно callers передают один
stream дважды, но тест с разными cursor-ами подтвердил разделение.

Нулевой ID возвращает null, не читая size. Ненулевой ID:

Resolver НЕ использует FAT offset/size/fileID для fresh inline object.
Направленный fileID42 case читает inline, не вызывает file lookup466490.
Это не отменяет отдельную fileID-ветвь внешнего manager materialization422940.
Inline-size1 и999 при фактическом объекте55 bytes оба принимаются: новый
объект читается до завершения его serializer-а, размер не ограничивает read.
ExpectedClassDEADBEEF тоже не мешает получить реальный Animation.

Возвращаемые pointers **не retain-ятся**. Две FAT entries могут ссылаться
на один живой объект. Entry clear удаляет записи/имена, не сами объекты.
Заимствование кэша и владение newly created объектами — разные обязанности
вызывающего кода, их нельзя свести к «каждый pointer удалить один раз».

## Направленные отказы и границы

`spSerializer.*` теперь содержит generic read protocol и explicit host context. Animation reader использует прежний field core, но наполняет уже созданный объект, не заменяя опубликованный адрес. Optional name manager даёт owned leases. `--inspect-reference` в portable tests идёт через FAT/dispatch/factory, а не напрямую в field reader.

Host guards намеренно строже: проверяются size/read/seek/extents, missing
serializer/factory и depth64/object4096. Error poisons context; retry больше
не потребляет bytes. Частично созданные объекты живут до context teardown,
который обнуляет owned FAT aliases и освобождает их; cache pointers остаются
borrowed. Менеджеры и borrowed ресурсы должны переживать context. Это host
lifetime policy, не доказательство общей native rollback/cyclic-graph схемы.

Открыто: полные malformed/cache-error/tell/allocation/header failure ветви,
native error gate states, source-level interface/signatures, произвольное
cyclic ownership SMO, whole file save, все
concrete payload adapters/unknown preservation и scene-to-PC backend.
Raw direct-call search не нашёл отдельного FFPS save caller для467260;
это **не доказательство отсутствия** whole save в protected/indirect коде.
