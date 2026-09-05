# `spMemoryStream`: конкретный поток в памяти

Статус: глубокий статический разбор PC и PS2 выполнен 4 сентября 2026 года;
portable-реконструкция собрана и проверена автоматическими тестами. Имена
класса и PC translation unit точные, путь заголовка и имена трёх невиртуальных
helpers остаются inferred/analytical.

## Контрольные бинарники

| Платформа | Файл | SHA-256 |
|---|---|---|
| PC | `local-data/pc-pristine/WinxClub.exe` | `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` |
| PS2 | `local-data/Winx Club the game PS2/SLES_532.19` | `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE` |

PC хранит точную строку
`Z:\Sparkplug\Code\SparkBase\spMemoryStream.cpp`. В PS2 присутствует короткая
строка `spMemoryStream.cpp`; обе сборки регистрируют имя `spMemoryStream`.

## RTTI и создание

| Свойство | PC | PS2 |
|---|---:|---:|
| class ID | `0x57177DB5` | `0x57177DB5` |
| base class ID | `spStream / 0x6CC80D8A` | `spStream / 0x6CC80D8A` |
| registration | `0x00760280` | `0x004A2670` |
| factory | `0x00465560` | `0x00112D40` |
| vtable | `0x006E7E50` | `0x0048C9F0` |

PS2 factory непосредственно выделяет `0x38` байт и записывает все новые поля.
Начало PC factory защищено SecuROM, но доступные методы используют те же offsets,
а vtable и двенадцать callers невиртуального allocation helper дают независимую
проверку контракта. PC размер `0x38` таким образом сильно подтверждён общей
32-битной раскладкой, но прямой читаемой инструкции `allocate 0x38` в PC image
пока нет.

## Native layout

Обе сборки используют одну раскладку после `spStream = 0x1C`:

| Offset | Размер | Наблюдаемая роль | Начальное значение |
|---:|---:|---|---:|
| `+0x1C` | 4 | ёмкость выделенного буфера | `0` |
| `+0x20` | 4 | логический размер данных | `0` |
| `+0x24` | 4 | квант роста и размер первого `Open` allocation | `5000 / 0x1388` |
| `+0x28` | 1 | разрешено расширение | `1` |
| `+0x29` | 3 | alignment padding | — |
| `+0x2C` | 4 | абсолютная текущая позиция | `0` |
| `+0x30` | 4 | указатель на буфер | `0` |
| `+0x34` | 1 | буфер принадлежит потоку | `1` |
| `+0x35` | 3 | alignment padding | — |

Имена полей в таблице аналитические. Byte-exact структуры находятся раздельно
в `Sparkplug/Analysis/PC/SparkBaseAbi.h` и
`Sparkplug/Analysis/PS2/SparkBaseAbi.h`.

## Vtable

PS2 имеет два начальных ABI-слова, поэтому её offsets на восемь байт больше.
Как и у `spStream`, порядок двух write-overloads различается между платформами.

| Роль | PC slot/target | PS2 slot/target |
|---|---|---|
| deleting destructor | `+0x00 -> 0x00465730` | `+0x08 -> 0x00112BD0` |
| notification | `+0x04 -> 0x005B7A00` | `+0x0C -> 0x00100810` |
| clone | `+0x08 -> 0x004655E0` | `+0x10 -> 0x00112C50` |
| clone-copy | `+0x0C -> 0x00413120` | `+0x14 -> 0x00105DC0` |
| registration getter | `+0x10 -> 0x00465480` | `+0x18 -> 0x00112250` |
| exact/kind checks | `+0x14/+0x18` | `+0x1C/+0x20` |
| `Open(name)` | `+0x1C -> 0x004654A0` | `+0x24 -> 0x00112B50` |
| `Open(mode,name)` | `+0x20 -> 0x00465490` | `+0x28 -> 0x00112BA0` |
| `Close()` | `+0x24 -> 0x004654D0` | `+0x2C -> 0x00112B00` |
| `Seek` | `+0x28 -> 0x00465750` | `+0x30 -> 0x00112940` |
| current position | `+0x2C -> 0x00465B60` | `+0x34 -> 0x001122C0` |
| `ReadData` | `+0x30 -> 0x00465820` | `+0x38 -> 0x001127B0` |
| raw `WriteData` | `+0x38 -> 0x00465A50` | `+0x3C -> 0x00112500` |
| stream-to-stream write | `+0x34 -> 0x00465AA0` | `+0x40 -> 0x00112470` |
| `GetSize` | `+0x3C -> 0x00465AE0` | `+0x44 -> 0x001123A0` |
| `GetBuffer` | `+0x40 -> 0x004CF2F0` | `+0x48 -> 0x00112BC0` |

Clone создаёт новый default `spMemoryStream` и вызывает у source унаследованный
clone-copy slot. Поэтому переносится `spNamedObject` name, но не копируются
буфер, размеры, позиция и diagnostic stream name из `spStream +0x18`.

## Точный state machine

### Открытие и закрытие

`Open(name)` сохраняет diagnostic name, выделяет `growthQuantum` байт, ставит
capacity равной этому кванту и обнуляет size/position. Mode-overload полностью
игнорирует mode. Оригинал не закрывает предыдущий буфер и не проверяет результат
allocation, поэтому повторный `Open` способен утечь; reconstruction сохраняет
видимые переходы, не выдавая этот путь за хороший API.

`Close` освобождает буфер только при `ownsBuffer != 0`, затем всегда обнуляет
pointer. Capacity, size, position, оба флага и diagnostic name намеренно остаются
прежними. Повторный `Close` возвращает true.

### Чтение и позиция

`ReadData` сначала сравнивает `position + count` с logical size. При успехе
копирует байты и увеличивает position. Равенство разрешено. Из-за порядка
проверок нулевое чтение закрытого default stream проходит, хотя обычное чтение
неоткрытого потока завершается ошибкой.

`GetCurrentPosition` возвращает `absolutePosition - spStream::logicalOrigin`, а
`GetSize` — `logicalSize`; оба требуют ненулевой buffer pointer.

`Seek` вычисляет:

```text
essStart   -> logicalOrigin + offset
essEnd     -> capacity - offset - 1
essCurrent -> absolutePosition + offset
```

и допускает только диапазон `0..logicalSize`. Проверка buffer pointer находится
только в error path, поэтому `Seek(essStart, 0)` у закрытого пустого stream
возвращает true. Ветка `essEnd` действительно опирается на capacity, а не на
logical size; это независимо совпадает на PC и PS2.

### Запись и рост

Общий helper — PC `0x00465910`, PS2 `0x00112580` — работает так:

1. null buffer даёт ошибку unopened;
2. если `position + count < logicalSize`, размер не меняется;
3. если результат строго меньше capacity, logicalSize становится результатом;
4. при результате, равном capacity, управление уже идёт в resize path;
5. при запрещённом resize возвращается ошибка `0x00020006` и текст
   `Memory buffer is not big enough...`;
6. иначе capacity увеличивается по `growthQuantum`, пока не станет не меньше
   результата; выделяется новый buffer, копируются старые logical bytes, старый
   buffer освобождается, size расширяется.

Следствие строгих сравнений: запись ровно до capacity перевыделяет буфер того же
размера. Growth helper освобождает старый pointer без проверки ownership flag;
flag используется только `Close`. Это опасный, но подтверждённый контракт.

Raw write после preflight копирует caller bytes и сдвигает position.
Stream-to-stream overload вызывает source `ReadData` прямо в destination buffer.
Если source read провален, position не меняется, но уже расширенный preflight
logical size не откатывается.

## Невиртуальные helpers

| Аналитическая роль | PC | PS2 | Поведение |
|---|---:|---:|---|
| `ResizeAndSetSize` | `0x00465500` | `0x00112260` | free старого buffer, allocate ровно N, capacity=size=N, position=0 |
| `ReleaseBuffer` | `0x00465540` | не найден отдельным linked body | flags `owns=0`, `resize=0`, вернуть pointer, не очищая его в stream |
| `Reset` | `0x00465550` | не найден отдельным linked body | size=0, position=0, capacity/buffer прежние |

Первый helper на PC вызывается двенадцать раз. Типичная последовательность:
получить размер file stream, создать memory stream, выделить точный размер,
получить `GetBuffer` и прочитать туда файл. `ReleaseBuffer` имеет один особенно
ясный caller `0x004D0A30`: после полного чтения он передаёт pointer наружу,
уничтожает stream и возвращает pointer вызывающему коду. `Reset` используется
для повторного заполнения scratch stream без нового allocation.

Original spellings этих трёх методов не доказаны. В коде выбраны описательные
portable names, а evidence-таблицы сохраняют адреса.

## Реализованный срез и проверки

Добавлены `Sparkplug/Code/SparkBase/spMemoryStream.h/.cpp`, RTTI factory,
регистрация и отдельные PC/PS2 layout/function constants. Автотест проверяет:

- factory, inheritance и class ID;
- default/open/close state;
- чтение, overwrite без усечения и fixed-quantum growth;
- equality boundary capacity;
- необычные закрытые zero-byte fast paths и capacity-relative end seek;
- exact-size allocation, reset и ownership transfer;
- сохранение enlarged size после неудачной stream-to-stream записи;
- clone, который переносит только унаследованное object name.

Portable-код безопасно отклоняет arithmetic overflow, null data pointer при
ненулевом размере и allocation failure в data operations. Native в этих случаях
может переполнить арифметику либо обратиться по неверному адресу; это явно
ограниченная safety-разница, а не новое утверждение об оригинале.

## Открытые вопросы

1. Реальный header path и исходные имена полей.
2. Original spellings и qualifiers трёх невиртуальных helpers.
3. Точный source type размеров/offsets: машинные сравнения signed, хотя многие
   caller strings называют значения `uSize`.
4. Где задаются отличные от default growth quantum и resize flag; отдельный
   setter в двух linked slices не найден.
5. Были ли PS2 варианты `ReleaseBuffer`/`Reset` inlined, discarded linker-ом
   либо отсутствовали в этой platform branch.
6. Полная связь local error reasons с глобальными stream operation codes.
7. Прямое подтверждение PC allocation size после снятия SecuROM trampoline.
