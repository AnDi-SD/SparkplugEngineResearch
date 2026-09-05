# `spStream`: абстрактный контракт потоков SparkBase

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

- class ID `0x6CC80D8A` одинаков на PC и PS2;
- base ID `0x20A72504`, то есть `spCrossPlatform`;
- RTTI factory и property callback отсутствуют;
- класс имеет девять последовательных pure-virtual stream-операций;
- следующий слот `GetBuffer()` виртуален, но не pure, и в базе возвращает null;
- native layout имеет размер `0x1C` и добавляет два слова к базе `0x14`;
- clone класса возвращает null;
- невиртуальные обёртки сводят typed `Read/Write` к `ReadData/WriteData`;
- C-строка хранится с 16-битной длиной, включающей завершающий ноль;
- есть helper переноса всего содержимого одного потока в другой.

## Контрольные бинарники

| Платформа | Файл | SHA-256 |
|---|---|---|
| PC | `local-data/pc-pristine/WinxClub.exe` | `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` |
| PS2 | `local-data/Winx Club the game PS2/SLES_532.19` | `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE` |

Все адреса ниже относятся только к этим контрольным образам.

## Исходный модуль и путь

Класс однозначно относится к engine-модулю `SparkBase` по иерархии и по прямым
потомкам. В PC executable сохранились точные соседние пути:

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
|---|---:|---:|
| class ID | `0x6CC80D8A` | `0x6CC80D8A` |
| base | `spCrossPlatform` | `spCrossPlatform` |
| registration object | `0x0075AB18` | `0x004A2730` |
| static initializer | RVA `0x002D166D` | VA `0x0047F9BC` |
| registration getter | `0x00417060` | `0x001148B0` |
| factory | null | null |
| property callback | null | null |

Прямые потомки дают независимую проверку ребра наследования:

| Класс | ID | Платформа | registration | factory |
|---|---:|---|---:|---:|
| `spMemoryStream` | `0x57177DB5` | PC | `0x00760280` | `0x00465560` |
| `spMemoryStream` | `0x57177DB5` | PS2 | `0x004A2670` | `0x00112D40` |
| `spFileStream` | `0x5E0623EC` | PC | `0x0084D590` | null |
| `spFileStream` | `0x5E0623EC` | PS2 | `0x004A2610` | null |
| `spSocketStream` | `0x1ED8677D` | PC | `0x00762C88` | `0x00499410` |
| `spPCFileStream` | `0x5EDF341C` | PC | `0x0084D470` | `0x006BD580` |
| `spPS2FileStream` | `0x12FDDAB3` | PS2 | `0x004AD2D0` | `0x001DC680` |

`spFileStream` остаётся абстрактной промежуточной базой: factory отсутствует, а
её vtable реализует только первый `Open`-wrapper и наследуемый `GetBuffer()`.
Этот wrapper на обеих платформах передаёт второй перегрузке mode `1`.

### Режим второй перегрузки `Open`

Исходное имя enum не найдено, но PC `spPCFileStream::Open` `0x006BD710` даёт
точную семантику битов через аргументы Win32 `CreateFile`:

| Бит | Наблюдаемое поведение PC |
|---:|---|
| `1` | `GENERIC_READ`, существующий файл |
| `2` | `GENERIC_WRITE` |
| `4` | `GENERIC_READ | GENERIC_WRITE` |
| `8` | модификатор append: open/create без усечения и затем `Seek(essEnd, 0)` |

Без append write/read-write использует create-always, с append — open-always.
PS2 `spPS2FileStream::Open` `0x001DC020` проверяет те же четыре бита и переводит
их в platform backend modes `1/2/3`, а для бита `8` также вызывает end-relative
`Seek`. Роли битов, таким образом, подтверждены независимо; original enumerator
names остаются неизвестными и в portable API пока передаётся raw `u32`.

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
|---:|---|
| `0x00020002` | операция над неоткрытым потоком |
| `0x00020003` | чтение за logical end / EOF |
| `0x00020004` | повторный `Open` уже открытого file stream |
| `0x00020005` | недопустимый результат `Seek` |
| `0x00020006` | запись за capacity при выключенном resize `spMemoryStream` |

Это значения глобального канала состояния операций, а не поле причины внутри
`spStreamError`. У самого error-объекта функция форматирования PC `0x004905E0`
и PS2 `0x00107DB0` читает `+0x10` и независимо использует такую таблицу:

| Причина `spStreamError` | Текст |
|---:|---|
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

PC vtable начинается по `0x006DB868`. В девяти слотах `+0x1C..+0x3C` стоит один
адрес `_purecall` `0x0060DB76`. PS2 vtable начинается по `0x0048CA90`, имеет два
служебных ABI-слова в начале и девять нулей по `+0x24..+0x44`. Это прямое
доказательство source-level abstract contract, а не вывод только из null factory.

| Роль | PC slot/target | PS2 slot/target | Статус имени |
|---|---|---|---|
| deleting destructor | `+0x00 -> 0x00417070` | `+0x08 -> 0x00114ED0` | ABI |
| notification | `+0x04 -> 0x005B7A00` | `+0x0C -> 0x00100810` | inherited |
| clone | `+0x08 -> 0x004A1BF0` | `+0x10 -> 0x00114F90` | поведение доказано, имя неизвестно |
| clone-copy | `+0x0C -> 0x00413120` | `+0x14 -> 0x00105DC0` | inherited |
| registration getter | `+0x10 -> 0x00417060` | `+0x18 -> 0x001148B0` | роль доказана |
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

Это platform ABI difference, а не повод переставлять смысл методов ради одной
общей таблицы. Portable declaration выбирает один логический порядок, но обе
byte-exact evidence-таблицы хранят реальные offsets раздельно.

## Проверка через `spMemoryStream`

Полная vtable `spMemoryStream` закрывает смысл каждого stream-слота:

| Операция | PC target | PS2 target | Наблюдаемое действие |
|---|---:|---:|---|
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

`essStart` и `essCurrent` встречаются в исходных assertion strings. Значение 2
идентифицируется по end-relative ветке switch. Оригинальный enum type неизвестен.
У `spMemoryStream` end-relative ветка использует buffer capacity `+0x1C`, а не
logical size `+0x20`, и считает `capacity - offset - 1`; это зафиксированное
поведение оригинала, даже если оно выглядит неожиданно.

## Невиртуальные `Read/Write`

Участок PS2 `0x00114990..0x00114E40` содержит последовательность тонких wrappers:

- `WriteData` с фиксированными размерами `0x10`, `0x08`, `0x0C`, `0x40`, `0x24`,
  `0x04`, `0x01`, `0x01`, затем string helper, `0x02`, `0x04`;
- `ReadData` с фиксированными размерами `0x10`, `0x08`, `0x0C`, `0x40`, `0x04`,
  `0x01`, `0x04`, `0x01`, `0x01`, затем string helper, `0x02`, `0x04`.

То есть оригинал имел overloaded typed `Read/Write` для scalar и math types.
Surviving assertions буквально подтверждают как минимум вызовы
`Write((u8)uSize)`, `Write((u16)uSize)`, `Write((u32)uSize)`,
`Write(szString)` и `Read(&pEntry->m_szFilename)` на обеих платформах.
Привязывать размеры `0x10/0x08/0x0C/0x40/0x24` к ещё не восстановленным точным
именам математических типов преждевременно. В portable-срезе они представлены
ограниченной trivially-copyable template-обёрткой; это не ABI-реконструкция
всех исходных overload symbols.

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

### Перенос целого потока

PS2 `0x00114E60` и PC `0x00416D70` вызываются как метод source stream:

1. получает полный размер source через slot PS2 `+0x44` / PC `+0x3C`;
2. при ошибке возвращает false;
3. вызывает у destination stream slot PS2 `+0x40` / PC `+0x34`, передавая
   source и размер;
4. после самого вызова возвращает true, не проверяя его результат.

Portable-имя `CopyTo` аналитическое. Странное игнорирование результата
destination сохранено, потому что это доказанное native behavior.

## Реализованный срез

Добавлены:

- `Sparkplug/Code/SparkBase/spStream.h` — abstract portable contract;
- `Sparkplug/Code/SparkBase/spStream.cpp` — registration, null clone/buffer,
  owned name, string wrappers и whole-stream helper;
- `Sparkplug/Analysis/PC/SparkBaseAbi.h` — layout, ID, vtable/function addresses;
- `Sparkplug/Analysis/PS2/SparkBaseAbi.h` — byte-exact layout и PS2 addresses;
- `Sparkplug/Tests/spBaseObjectTests.cpp` — test-only concrete stream для
  проверки контракта без преждевременного восстановления `spMemoryStream`.

Portable-класс не выдаётся за host-ABI clone оригинала. В частности:

- native `char*` заменён на `std::optional<std::string>`;
- чрезмерная строка, которую native helper молча урезал бы до 16 бит,
  отклоняется;
- native `char**` reader заменён на ownership-safe `std::string`;
- точные math overload types отложены;
- один виртуальный метод остаётся `vfunc_WriteFromStream`, потому что его
  исходное имя не найдено.

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

Эти пункты перенесены в канонический список неизвестного. Следующий класс не
начинался.

## Воспроизведение

Основные команды статической проверки:

```powershell
python research\inspect_executable_architecture.py `
  local-data\pc-pristine\WinxClub.exe `
  'local-data\Winx Club the game PS2\SLES_532.19' --json

python research\inspect_elf_physics.py `
  'local-data\Winx Club the game PS2\SLES_532.19' `
  --match '^$' --dump-words-va 0x0048CA90 --dump-word-count 20

python research\inspect_pe_physics.py `
  local-data\pc-pristine\WinxClub.exe `
  --match '^$' --dump-words-rva 0x002DB868 --dump-word-count 18
```

Изолированная проверка реконструкции:

```powershell
cmake -S Sparkplug -B .codex-tmp\Sparkplug-build -DBUILD_TESTING=ON
cmake --build .codex-tmp\Sparkplug-build --config Debug
ctest --test-dir .codex-tmp\Sparkplug-build -C Debug --output-on-failure
```

Результат текущего среза: `1/1` test passed.
