# spError

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Статус: class identity, direct base, PC/PS2 vtable, PS2 allocation и exact
layout `0x28`, constructor arguments, четыре formatter-операции, factory и
необычный пустой clone подтверждены. Original header и имена методов не
найдены; portable имена с суффиксом `ForAnalysis` являются аналитическими.

## Identity, lifecycle и layout

PS2 factory непосредственно доказывает размер `0x28`; PC все наблюдаемые
accesses и следующий объектный offset согласуются с тем же prefix:

```text
+0x00  spBaseObject       0x10
+0x10  error code         u32
+0x14  severity           u32
+0x18  source-file ptr    ptr32
+0x1c  source line        u32
+0x20  message ptr        ptr32
+0x24  next error         ptr32
```

Initializer записывает аргументы как `code +0x10`, `severity +0x14`,
`source +0x18`, `line +0x1C`. Поле `+0x24` образует временную LIFO-цепочку в
`spErrorManager`; `+0x20` указывает на его фиксированный string stack.

## Formatter contract

PC targets `0x004167B0/0x004169A0/0x00416A80/0x00416A70`, PS2 targets
`0x00107250/0x00107170/0x00107040/0x00107160` согласованно дают четыре роли:

- code `0`: `No error`;
- code `1`: пользовательский текст из `+0x20`;
- code `2`: `Error manager data stack overflow`;
- прочие: class-specific error name и числовой code;
- severity `1/2/3`: `WARNING`, `ERROR`, `FATAL ERROR`; прочее — `UNKNOWN`;
- последний formatter добавляет `source(line)`, если source не null.

Базовая virtual error-name операция возвращает `error`; производные типы могут
заменить её для собственных code spaces.

## Важный clone contract

Обе реализации clone создают новый default `spError`, регистрируют пару в
`spCloneManager` и вызывают пустой inherited copy slot. Поля ошибки, message и
chain link не копируются. Portable тест это сохраняет: clone существует, но
имеет code/severity `0`, null message и не наследует runtime chain.

## Границы описания

Класс размещён вместе с доказанным `spErrorManager.cpp` в
`Sparkplug/Code/SparkBase/spErrorManager.h/.cpp`; отдельный original TU для
`spError` не утверждается. В PC/PS2 ABI добавлены layouts, IDs, vtable и
function anchors. Изолированные `SparkBaseTests` проверяют registration,
formatter, chain и blank-clone behavior.

Остаются неизвестными original header/TU, source-level enum names, точный тип
source-line и message ownership API, а также code spaces производных ошибок.
