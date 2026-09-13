# spMemoryStream

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spMemoryStream](../../../Sparkplug/Code/SparkBase/spMemoryStream.h).

## RTTI и создание

| Свойство | PC | PS2 |
| --- | ---: | ---: |
| class ID | `0x57177DB5` | `0x57177DB5` |
| base class ID | `spStream / 0x6CC80D8A` | `spStream / 0x6CC80D8A` |

## Native layout

| Offset | Размер | Наблюдаемая роль | Начальное значение |
| ---: | ---: | --- | ---: |
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
| --- | --- | --- |
| notification | `+0x04 -> 0x005B7A00` | `+0x0C -> 0x00100810` |
| clone-copy | `+0x0C -> 0x00413120` | `+0x14 -> 0x00105DC0` |
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

### Открытие и закрытие

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

Raw write после preflight копирует caller bytes и сдвигает position.
Stream-to-stream overload вызывает source `ReadData` прямо в destination buffer.
Если source read провален, position не меняется, но уже расширенный preflight
logical size не откатывается.

## Невиртуальные helpers

| Аналитическая роль | Поведение |
| --- | --- |
| `ResizeAndSetSize` | free старого buffer, allocate ровно N, capacity=size=N, position=0 |
| `ReleaseBuffer` | flags `owns=0`, `resize=0`, вернуть pointer, не очищая его в stream |
| `Reset` | size=0, position=0, capacity/buffer прежние |

Первый helper на PC вызывается двенадцать раз. Типичная последовательность:
получить размер file stream, создать memory stream, выделить точный размер,
получить `GetBuffer` и прочитать туда файл. `ReleaseBuffer` имеет один особенно
ясный caller `0x004D0A30`: после полного чтения он передаёт pointer наружу,
уничтожает stream и возвращает pointer вызывающему коду. `Reset` используется
для повторного заполнения scratch stream без нового allocation.

Portable-код безопасно отклоняет arithmetic overflow, null data pointer при
ненулевом размере и allocation failure в data operations. Native в этих случаях
может переполнить арифметику либо обратиться по неверному адресу; это явно
ограниченная safety-разница, а не новое утверждение об оригинале.

## Открытые вопросы

1. Реальный header path и исходные имена полей. 2. Original spellings и qualifiers трёх невиртуальных helpers. 3. Точный source type размеров/offsets: машинные сравнения signed, хотя многие    caller strings называют значения `uSize`. 4. Где задаются отличные от default growth quantum и resize flag; отдельный    setter в двух linked slices не найден. 5. Были ли PS2 варианты `ReleaseBuffer`/`Reset` inlined, discarded linker-ом    либо отсутствовали в этой platform branch. 6. Полная связь local error reasons с глобальными stream operation codes. 7.
