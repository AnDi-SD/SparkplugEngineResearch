# `spError`: базовая запись диагностической цепочки

Статус: class identity, direct base, PC/PS2 vtable, PS2 allocation и exact
layout `0x28`, constructor arguments, четыре formatter-операции, factory и
необычный пустой clone подтверждены. Original header и имена методов не
найдены; portable имена с суффиксом `ForAnalysis` являются аналитическими.

## Контрольные бинарники

| Платформа | Файл | SHA-256 |
|---|---|---|
| PC | `local-data/pc-pristine/WinxClub.exe` | `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` |
| PS2 | `local-data/Winx Club the game PS2/SLES_532.19` | `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE` |

## Identity, lifecycle и layout

| Поле | PC | PS2 |
|---|---:|---:|
| Class ID | `0x789B29B9` | `0x789B29B9` |
| Base | `spBaseObject / 0x415352A1` | `spBaseObject / 0x415352A1` |
| Registration | `0x0075A9A8` | `0x004A0350` |
| Initializer | `0x006D1620` | `0x0047F500` |
| Getter | `0x00416BB0` | `0x00107030` |
| Factory | protected entry `0x00416BC0` | `0x001076A0`, alloc `0x28` |
| Constructor | `0x00416750` | `0x00107560` |
| Field initializer | `0x00416790` | `0x001074E0` |
| Destructor | `0x00416780` | `0x00107500` |
| Clone | `0x00416C40` | `0x001075B0` |
| Vtable | `0x006DB7BC` | `0x0048C6E8` |

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

## Реконструкция и открытое

Класс размещён вместе с доказанным `spErrorManager.cpp` в
`Sparkplug/Code/SparkBase/spErrorManager.h/.cpp`; отдельный original TU для
`spError` не утверждается. В PC/PS2 ABI добавлены layouts, IDs, vtable и
function anchors. Изолированные `SparkBaseTests` проверяют registration,
formatter, chain и blank-clone behavior.

Остаются неизвестными original header/TU, source-level enum names, точный тип
source-line и message ownership API, а также code spaces производных ошибок.
