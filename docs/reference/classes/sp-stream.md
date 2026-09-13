# spStream

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spStream](../../../Sparkplug/Code/SparkBase/spStream.h).

Статус: глубокий статический срез PC/PS2 завершён; регистрация, наследование,
размер, два собственных поля, полный виртуальный контракт и основная группа
невиртуальных обёрток восстановлены. Имена, которых нет в бинарных строках,
остаются аналитическими.

## Краткий итог

`spStream` — первый после `spCrossPlatform` содержательный интерфейс в выбранной
ветви SparkBase:

```text
spBaseObject
  -> spNamedObject
    -> spCrossPlatform
      -> spStream                    abstract, 0x1C на PC и PS2
        -> spMemoryStream            concrete
        -> spFileStream              abstract
          -> spPCFileStream          concrete, PC
          -> spPS2FileStream         concrete, PS2
        -> spSocketStream            concrete, только PC executable
```

Подтверждено:

Все адреса ниже относятся только к этим контрольным образам.

## Исходный модуль и путь

Класс однозначно относится к engine-модулю `SparkBase` по иерархии и по прямым потомкам.

- `Z:\Sparkplug\Code\SparkBase\spMemoryStream.cpp`;
- `Z:\Sparkplug\Code\SparkBase\spSocketStream.cpp`;
- `Z:\Sparkplug\Code\SparkBasePC\spPCFileStream.cpp`.

В PS2 сохранилась короткая строка `spMemoryStream.cpp` по `0x00444550`.
Отдельной строки `spStream.cpp` или исходного header path не найдено ни в одном
образе. Поэтому `Sparkplug/Code/SparkBase/spStream.h` и `.cpp` имеют статус
`inferred`, а не `exact`. Это рабочее разбиение реконструкции, не утверждение об
оригинальном translation unit.

## RTTI registration

| Факт | PC | PS2 |
| --- | ---: | ---: |
| class ID | `0x6CC80D8A` | `0x6CC80D8A` |
| base | `spCrossPlatform` | `spCrossPlatform` |
| static initializer | RVA `0x002D166D` | VA `0x0047F9BC` |
| property callback | null | null |

| Класс | ID | Платформа | registration |
| --- | ---: | --- | ---: |
| `spMemoryStream` | `0x57177DB5` | PC | `0x00760280` |
| `spMemoryStream` | `0x57177DB5` | PS2 | `0x004A2670` |
| `spFileStream` | `0x5E0623EC` | PC | `0x0084D590` |
| `spFileStream` | `0x5E0623EC` | PS2 | `0x004A2610` |
| `spSocketStream` | `0x1ED8677D` | PC | `0x00762C88` |
| `spPCFileStream` | `0x5EDF341C` | PC | `0x0084D470` |
| `spPS2FileStream` | `0x12FDDAB3` | PS2 | `0x004AD2D0` |

`spFileStream` остаётся абстрактной промежуточной базой: factory отсутствует, а
её vtable реализует только первый `Open`-wrapper и наследуемый `GetBuffer()`.
Этот wrapper на обеих платформах передаёт второй перегрузке mode `1`.

### Режим второй перегрузки `Open`

| Бит | Наблюдаемое поведение PC |
| ---: | --- |
| `1` | `GENERIC_READ`, существующий файл |
| `2` | `GENERIC_WRITE` |
| `8` | модификатор append: open/create без усечения и затем `Seek(essEnd, 0)` |

Это не четыре произвольно комбинируемых режима. Обе реализации проверяют
основные биты с приоритетом `1`, затем `2`, затем `4`; `8` используется как
append-модификатор. PS2 дополнительно принимает одиночный `8` как backend mode
`3`, тогда как PC без одного из основных битов формирует недопустимый вызов
`CreateFile`. Поэтому portable-реконструкция намеренно не объявляет более
строгий enum до обнаружения исходной декларации и её правил валидности.

## Layout

Обе платформы используют одинаковое расположение данных:

```text
+0x00  spCrossPlatform base (0x14 bytes)
+0x14  u32 logical stream origin / base offset   original name unknown
+0x18  char* owned diagnostic stream name       original name unknown
+0x1C  end of spStream / first derived field
```

### PS2 proof

Конструктор `0x00114F50`:

1. вызывает `spCrossPlatform` constructor `0x00105D60`;
2. ставит vtable `0x0048CA90`;
3. обнуляет `+0x14` и `+0x18`.

Деструктор `0x00114ED0` ставит собственную vtable, освобождает ненулевой `+0x18`,
обнуляет его и вызывает `spCrossPlatform` destructor `0x00105D00`.

### PC proof

Несмотря на SecuROM-преобразование начала конструктора, читаемый хвост по
`0x0040502B` обнуляет `+0x14/+0x18`, затем ставит vtable `0x006DB868`.
Non-deleting destructor `0x00416D40` освобождает `+0x18` и передаёт управление
base destructor. Реализации `spMemoryStream` начинают собственные поля точно с
`+0x1C`, что закрывает размер базы `0x1C`.

### Поле `+0x14`

Это не обычная текущая позиция:

- `spMemoryStream::GetCurrentPosition` PS2 `0x001122C0` и PC `0x00465B60`
  возвращают `field_2C - field_14`;
- start-relative `Seek` PS2 `0x00112940` вычисляет `field_14 + offset`;
- `spPS2FileStream::GetCurrentPosition` `0x001DAAC0` возвращает
  `field_34 - field_14`;
- при открытии вложенного/упакованного участка `spPS2FileStream::Open`
  `0x001DC020` прибавляет физическое смещение к `+0x14`.

Поэтому `logicalOrigin` — доказанная роль поля. Исходное имя неизвестно.

### Поле `+0x18`

Helper PS2 `0x001148C0` и PC `0x00416F90`:

- переиспользует буфер, если старой длины хватает;
- иначе освобождает его и выделяет `strlen(name) + 1`;
- копирует нуль-терминированную строку;
- возвращает true.

Дочерние потоки передают это значение в объект ошибки. Рядом в PS2 находятся
строки `stream error`, `Can't open stream`, `Stream not opened`,
`Stream reached EOF`, `Invalid seek in stream` и `spStreamError`. Следовательно,
это owned stream name/path для диагностики; точное имя поля не восстановлено.

Часть error-code table уже сопоставляется по непосредственным веткам:

| Код | Доказанный trigger |
| ---: | --- |
| `0x00020002` | операция над неоткрытым потоком |
| `0x00020003` | чтение за logical end / EOF |
| `0x00020004` | повторный `Open` уже открытого file stream |
| `0x00020005` | недопустимый результат `Seek` |
| `0x00020006` | запись за capacity при выключенном resize `spMemoryStream` |

Это значения глобального канала состояния операций, а не поле причины внутри
`spStreamError`. У самого error-объекта функция форматирования PC `0x004905E0`
и PS2 `0x00107DB0` читает `+0x10` и независимо использует такую таблицу:

| Причина `spStreamError` | Текст |
| ---: | --- |
| `4` | `Can't open stream` |
| `5` | `Stream not opened` |
| `6` | `Stream reached EOF` |
| `7` | `Stream already opened` |
| `8` | `Invalid seek in stream` |

Обе платформы дают тот же порядок. Причина `4` имеет особый вариант
`Can't open stream (NULL)`, когда diagnostic name в `+0x20` error-объекта
отсутствует; иначе имя дописывается к сообщению. Полное соответствие между
внутренними причинами, глобальными кодами, platform OS codes и исходными enum
types пока не объявляется закрытым.

## Vtable и абстрактность

PC vtable начинается по `0x006DB868`. В девяти слотах `+0x1C..+0x3C` стоит один адрес `_purecall` `0x0060DB76`. PS2 vtable начинается по `0x0048CA90`, имеет два служебных ABI-слова в начале и девять нулей по `+0x24..+0x44`.

| Роль | PC slot/target | PS2 slot/target | Статус имени |
| --- | --- | --- | --- |
| notification | `+0x04 -> 0x005B7A00` | `+0x0C -> 0x00100810` | inherited |
| clone-copy | `+0x0C -> 0x00413120` | `+0x14 -> 0x00105DC0` | inherited |
| exact type | `+0x14 -> 0x00408350` | `+0x1C -> 0x00100010` | inherited |
| kind-of | `+0x18 -> 0x00408370` | `+0x20 -> 0x00100050` | inherited |
| `Open(name)` | `+0x1C` pure | `+0x24` pure | смысл доказан; spelling inferred |
| `Open(mode, name)` | `+0x20` pure | `+0x28` pure | смысл/порядок аргументов доказаны; enum неизвестен |
| `Close()` | `+0x24` pure | `+0x2C` pure | смысл доказан; spelling inferred |
| `Seek(source, offset)` | `+0x28` pure | `+0x30` pure | `Seek` и `ess*` подтверждены строками |
| `GetCurrentPosition(u32&)` | `+0x2C` pure | `+0x34` pure | spelling и reference-style подтверждены |
| `ReadData(void*, u32)` | `+0x30` pure | `+0x38` pure | spelling подтверждено |
| `WriteData(const void*, u32)` | `+0x38` pure | `+0x3C` pure | spelling подтверждено; ABI order различается |
| stream-to-stream write | `+0x34` pure | `+0x40` pure | поведение доказано, исходное имя неизвестно; ABI order различается |
| `GetSize(u32*)` | `+0x3C` pure | `+0x44` pure | spelling и pointer-style подтверждены |
| `GetBuffer()` | `+0x40 -> 0x004A1BF0` | `+0x48 -> 0x00114E50` | spelling подтверждено |

Асимметрия `GetCurrentPosition(u32&)` и `GetSize(u32*)` намеренно сохранена:
строки PC буквально содержат `GetCurrentPosition(uCurrentPos)` и
`GetSize(&uFileSize)`.

Два write-слота нельзя механически сопоставить правилом «PS2 offset = PC offset
+ 8». Их порядок действительно различается:

- PC `spMemoryStream` target `0x00465A50` копирует сырой указатель в собственный
  buffer, а `0x00465AA0` вызывает у source stream виртуальный `ReadData`;
- PS2 target `0x00112500` копирует сырой указатель, а `0x00112470` вызывает
  source stream `ReadData`;
- PC whole-stream helper `0x00416D70` вызывает у destination slot `+0x34`,
  тогда как PS2 helper `0x00114E60` вызывает slot `+0x40`.

Полная vtable `spMemoryStream` закрывает смысл каждого stream-слота:

| Операция | PC target | PS2 target | Наблюдаемое действие |
| --- | ---: | ---: | --- |
| Open #1 | `0x004654A0` | `0x00112B50` | имя, буфер, capacity/size/position |
| Open #2 | `0x00465490` | `0x00112BA0` | игнорирует mode и вызывает Open #1 |
| Close | `0x004654D0` | `0x00112B00` | освобождает owned buffer |
| Seek | `0x00465750` | `0x00112940` | start/end/current switch |
| GetCurrentPosition | `0x00465B60` | `0x001122C0` | position minus logical origin |
| ReadData | `0x00465820` | `0x001127B0` | bounds check, copy, advance |
| WriteData | `0x00465A50` | `0x00112500` | resize/check, raw copy, advance |
| stream-to-stream | `0x00465AA0` | `0x00112470` | вызывает source `ReadData` прямо в destination buffer |
| GetSize | `0x00465AE0` | `0x001123A0` | возвращает logical data size |
| GetBuffer | `0x004CF2F0` | `0x00112BC0` | возвращает buffer field |

Для `Seek` подтверждены значения:

```text
essStart   = 1
essEnd     = 2
essCurrent = 4
```

## Невиртуальные `Read/Write`

Участок PS2 `0x00114990..0x00114E40` содержит последовательность тонких wrappers:

- `WriteData` с фиксированными размерами `0x10`, `0x08`, `0x0C`, `0x40`, `0x24`,
  `0x04`, `0x01`, `0x01`, затем string helper, `0x02`, `0x04`;
- `ReadData` с фиксированными размерами `0x10`, `0x08`, `0x0C`, `0x40`, `0x04`,
  `0x01`, `0x04`, `0x01`, `0x01`, затем string helper, `0x02`, `0x04`.

PC даёт независимое подтверждение по `0x00416DA0..0x00416F8C`: read wrappers
обращаются к raw slot `+0x30`, write wrappers — к raw slot `+0x38`, и повторяют
размеры `1/2/4/8/12/16/36/64`. Несколько одинаковых scalar overloads на PC,
вероятно, сведены linker identical-code folding к общим телам; конкретные
исходные типы только из этого не выводятся.

### C-строки

PS2 `0x00114AF0` и PC `0x00416E80` подтверждают формат записи:

1. при включённом prefix записывается little-endian `u16(strlen + 1)`;
2. затем пишется строка вместе с `\0`;
3. без prefix пишется ровно `strlen`, без `\0`;
4. null с prefix кодируется нулевым `u16`;
5. null без prefix считается успешной пустой операцией.

PS2 `0x00114D80` и PC `0x00416DC0` читают `u16`, при ненулевом значении
выделяют ровно столько байт и вызывают `ReadData`. Portable
`ReadString(std::string&)` сохраняет wire format, но делает ownership и
обработку отсутствующего терминатора безопасными.

PS2 `0x00114E60` и PC `0x00416D70` вызываются как метод source stream:

1. получает полный размер source через slot PS2 `+0x44` / PC `+0x3C`;
2. при ошибке возвращает false;
3. вызывает у destination stream slot PS2 `+0x40` / PC `+0x34`, передавая
   source и размер;
4. после самого вызова возвращает true, не проверяя его результат.

Portable-имя `CopyTo` аналитическое. Странное игнорирование результата
destination сохранено, потому что это доказанное native behavior.

Добавлены:

Portable-класс не выдаётся за host-ABI clone оригинала. В частности:

## Что остаётся неизвестным

1. Реальный header path и наличие отдельного `spStream.cpp`.
2. Исходные имена полей `+0x14/+0x18`.
3. Имя и полный состав enum режима `Open`; PS2 показывает используемые биты
   `1/2/4/8`, но этого недостаточно для исходных enumerator names.
4. Исходное имя и точные source-level qualifiers stream-to-stream слота PC
   `+0x34` / PS2 `+0x40`, а также причина различного порядка двух write-overloads.
5. Точный pointer type результата `GetBuffer()`.
6. Имена math types для overloaded wrappers фиксированных размеров.
7. Полный stream-error enum и его связь с кодами `0x00020002..0x00020006`.
8. Начало PC constructor, преобразованное SecuROM; layout уже закрыт по
   читаемому хвосту, destructor и `spMemoryStream`, но entry address не объявлен.

Результат текущего среза: `1/1` test passed.
