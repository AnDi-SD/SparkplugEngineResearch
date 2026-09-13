# spTextureData

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spTextureBuffer](../../../Sparkplug/Code/Sparkplug/spTextureBuffer.h), [spTextureData](../../../Sparkplug/Code/Sparkplug/spTextureData.h).

## Объект и поведение

| Platform | PC | PS2 |
| --- | ---: | ---: |
| Class ID / base | `0x78EA082B / spTexture` | same |
| Buffer-copy implementation | interface entry `0x00435060` | thunk `0x00135D30` → `0x00178CD0` |
| Payload cleanup | interface entry `0x00435140` | thunk `0x00135D20` → `0x00178E60` |

### Exact layout и platform split

Общий prefix заканчивается одинаково:

| Offset | Size | Подтверждённая роль |
| ---: | ---: | --- |
| `+0x00` | `0x38` | `spTexture` |
| `+0x38` | `0x30` | embedded `spTextureBuffer` |
| `+0x68` | 1 | выбирает один из двух payload-cleanup paths |
| `+0x6C` | platform | первый контейнер записей размером `0x10` |

Дальше компиляторные ABI расходятся:

| Role | PC | PS2 |
| --- | ---: | ---: |
| first container header | `+0x6C`, `0x10` | `+0x6C`, `0x0C` |
| следующий byte flag | `+0x7C` | `+0x78` |
| opaque inline platform state | `+0x80`, `0x410` | `+0x7C`, `0x410` |
| second container header | `+0x490`, `0x10` | `+0x48C`, `0x0C` |
| final `sizeof` | `0x4A0` | `0x498` |

PC container имеет allocator/begin/end/capacity-end, PS2 — три слова
capacity-or-high-water/count/storage. Это две независимые разницы по четыре
байта и полностью объясняет расхождение итогового размера на восемь байт.
Размер и границы блока `0x410` доказаны, но его поля пока не названы.

PS2 constructor явно вызывает `spTexture`, ставит две vtable, конструирует
`spTextureBuffer +0x38`, обнуляет `+0x68`, оба контейнера и byte после первого
контейнера. PC factory непосредственно выделяет `0x4A0`; cleanup-код независимо
подтверждает переведённые offsets обоих контейнеров.

### Копирование CPU texture buffer

`spTexture::Init` передаёт адрес локального `spTextureBuffer*` в virtual slot.
Concrete реализация `spTextureData`:

1. читает source buffer pointer;
2. вызывает `Init` встроенного `spTextureBuffer +0x38` с теми же width, height,
   третьим `u16`, auxiliary pointer и pixel format;
3. вычисляет `width * height * depth * pixelSize`;
4. копирует весь raw payload в `+0x54`, то есть buffer pointer embedded-объекта;
5. возвращает true.

Нормализация logical dimensions принадлежит `spTexture` и происходит перед
копированием. Поэтому при source `3×5` и включённой нормализации внешний
`spTexture` хранит `4×8`, но embedded `spTextureBuffer` всё ещё содержит
исходные `3×5` и соответствующие 60 байт BGRA32. Portable-тест закрепляет это
разделение вместо ошибочного resize payload.

### Clone и cleanup

RTTI clone обеих платформ выделяет полный объект, запускает constructor,
регистрирует пару в clone manager и вызывает inherited copy slot. Этот slot
копирует только shared name из `spNamedObject`. `spTexture` state, embedded
buffer, оба flags, platform block и списки остаются constructor-blank.

Payload cleanup использует `+0x68`:

- при ненулевом flag проходит первый список с шагом `0x10` и освобождает pointer
  записи `+0x0C`;
- при нулевом flag проходит второй список с шагом `0x14` и освобождает pointer
  записи `+0x10`.

Затем destructor уничтожает оба container storage, embedded `spTextureBuffer`
и базовый `spTexture`. Роли остальных record-слов и источник значения `+0x68`
пока не доказаны, поэтому portable class не предлагает API создания мнимых
platform records.

### Границы описания

`Code/Sparkplug/spTextureData.*` содержит concrete RTTI/factory, name-only
clone, embedded buffer, два доказанных constructor flags и безопасный вариант
buffer-copy пути. Exact PC/PS2 layouts остаются в `Analysis`, чтобы разница
container ABI не маскировалась host STL.

Открыты: исходные имена flags и record types, layout блока `0x410`, назначение
всех четырёх interface slots, связь обоих списков с cross/platform-specific
serializer sections, rollback при частичном чтении и platform upload lifetime.

## Данные в SMO

### Прямые поля объекта

Executable обеих платформ подтверждают имена:

| Field | Имя serializer | Наблюдение в прямой секции |
| ---: | --- | --- |
| 0 | `esfTextureDataCrossPlatform` | Прямое cross-platform представление |
| 1 | `esfTextureDataPlatformSpecific` | используется внутри embedded payload |
| 2 | `esfTextureDataSourceNone` | Имя поля известно; прямая форма не описана |
| 3 | `esfTextureDataSourceEmbeded` | Встроенный источник PC или PS2 |
| 4 | `esfTextureDataSourceReference` | Имя поля известно; прямая форма не описана |
| 6 | `esfTextureDataPlatformType` | Встречается перед прямым field 0 в legacy PC-форме |

В написании строки executable действительно используется `Embeded` с одной
буквой `d`. Поля 0/1/6 повторяются во вложенной производной секции; одинаковый
номер на разных уровнях не означает один и тот же физический offset.

### Пять наблюдаемых storage-вариантов

| Представление | Смысл |
| --- | --- |
| `texture_legacy_cross` | прямой field 0 с BGRA32 |
| `texture_direct3d_embedded` | embedded Direct3D BGRA32, 1–9 mip |
| `texture_ps2_native` | embedded PS2 без cross-копии |
| `texture_ps2_native_with_cross` | одновременно BGRA и PS2 native |
| `texture_embedded_cross_only` | embedded field 0 без native-копии |

### Cross-platform BGRA32

Прямой либо вложенный field 0 содержит один field 5 и terminator:

```text
field 5 payload:
    UInt32 width
    UInt32 height
    UInt32 pixelFormat          // наблюдаемые 0/1 дают четырёхбайтовые pixels
    UInt32 bytesPerPixel        // всегда 4
    byte bgra[width*height*4]
terminator
```

### Embedded-обёртка

```text
section 0, source base:
    field 2, byte false
    terminator

section 1, texture implementation:
    optional field 6, UInt32 platformType
    optional field 0, cross-platform representation
    optional field 1, platform-specific representation
    terminator
```

Наблюдаемые platform type:

### Direct3D BGRA32

Внутри platform-specific field 1 находится mip-stream:

### PS2 native

Внутри platform-specific field 1 находится один field 0:

```text
byte   nativeDataFlag          // raw byte; zero does not skip the image
UInt32 pixelFormat
UInt32 width
UInt32 height
UInt32 auxiliaryValue         // сериализован, семантическое имя пока неизвестно
UInt32 mipCount

palette:
    format 0:  16 * 4 bytes
    format 1: 256 * 4 bytes
    other formats: no palette

repeat mipCount times:
    UInt32 descriptor0
    UInt32 descriptor1
    UInt32 descriptor2
    UInt32 dataSize
    byte   data[dataSize]
```

```text
format 0: ceil(mipWidth * mipHeight / 2)   // indexed 4-bit
format 1: mipWidth * mipHeight             // indexed 8-bit
format 3: mipWidth * mipHeight * 4         // 32-bit
```

| Format | Представление | Объекты | Палитра | Mip count |
| ---: | --- | ---: | ---: | --- |
| 0 | indexed 4-bit | 644 | 64 байта | 1–5 |
| 1 | indexed 8-bit | 1 630 | 1 024 байта | 1–6 |
| 3 | 32-bit | 83 | нет | 1, 2 или 4 |

Размеры лежат в диапазоне от 8×8 до 512×512, включая прямоугольные 64×32,
128×64, 128×256, 256×128 и 512×256. Четыре служебных значения mip полностью
сохраняются в БД как `descriptor0..2` и `dataSize`; аппаратные имена первых трёх
пока не присвоены.

### Исправленная старая гипотеза

Старый decoder называл значения `0x0EE3`, `0x32E3`, `0x43E3`, `0x54E3` и
`0x29E3` форматами текстуры. Это неверно:

- младший байт `E3` — compact data-block header field 3 с 32-битным размером;
- следующий байт — младшая часть размера payload;
- поэтому число меняется от общего размера вложенного контейнера и mip-цепочки,
  а не от pixel format;
- байт по старому offset `+0x3C` в обычной Direct3D-форме является последним
  байтом `mipHeight`, а не отдельным нулевым serializer marker;
- настоящий pixel format хранится внутри representation: `0` для наблюдаемой
  Direct3D-формы и `0/1/3` для PS2.

`SmoTexture.FormatCode` временно сохраняет это двухбайтовое значение только как
legacy diagnostic signature для совместимости. Новый код не принимает решений
по нему.
