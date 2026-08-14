# Формат STX

Статус: предварительная спецификация standalone-текстур Winx Club для PC. Она
основана на независимой реализации [`kotwys/stx-gimp-plugin`](https://github.com/kotwys/stx-gimp-plugin),
побайтовом разборе исходного кода плагина и сверке с локальным корпусом
`pc-pristine`. Универсального STX-декодера в проекте пока нет.

## Важная граница

Расширение `.stx` не означает единственную структуру файла. В 243 файлах из
локального чистого PC-корпуса 2026-08-14 обнаружены три семейства:

| Семейство | Файлов | Начало файла | Статус |
|---|---:|---|---|
| legacy/tagged | 186 | `22 00 00` | формат, реализованный плагином kotwys |
| compact `E0/E5` | 56 | `E0 <UInt32 size> E5 <UInt32 size>` | структура подтверждена корпусом, семантика одного flag открыта |
| raw 20-byte header | 1 | пять `UInt32`, затем pixels | наблюдался у `bloom_jeans.stx` |

Следовательно, проверка только расширения или magic `22 00 00` недостаточна.
Портировать код плагина как общий STX parser нельзя: он корректно описывает
legacy-семейство, но отвергает или неверно разбирает остальные PC-варианты.

## Legacy/tagged PC STX

### Общая структура

Все числа little-endian. С учётом полных 32-битных размеров блоков структура
имеет следующий вид (`pixelBytes = width * height * 4`):

```text
22 00 00                                      # внешний magic
[E6 UInt32(4) UInt32(6)]                      # optional data block
E1 UInt32(pixelBytes + 0x20)
  E0 UInt32(pixelBytes + 0x1A)
    Byte[26] geometry
    Byte[pixelBytes] pixels                   # BGRA8
  00                                          # входит в payload E1
00                                            # внешний terminator
```

При наличии `E6` пиксели начинаются с `0x30`, без `E6` — с `0x27`. В корпусе
найдено 138 файлов с `E6` и 48 без него. Все 186 файлов имеют ровно
`width * height * 4` байт изображения и двухбайтовый нулевой trailer.

Порядок каналов **BGRA8** подтверждается не названием переменной, а явным
описанием формата компонентов B, G, R, A при передаче данных в GIMP в
[`src/gimp_interop.cpp`](https://github.com/kotwys/stx-gimp-plugin/blob/f61624a695b0e5516682cd4fe0a07c7a38b96282/src/gimp_interop.cpp).
Сжатие, mip levels и palette branches в реализации отсутствуют.

### Geometry prefix в payload `E0`

Offsets ниже считаются после четырёхбайтового размера `E0`:

| Offset | Размер | Наблюдаемое значение |
|---:|---:|---|
| `0x00` | 1 | `0x01` |
| `0x01` | 4 | дублированный width |
| `0x05` | 4 | дублированный height |
| `0x09` | 4 | `0` |
| `0x0D` | 1 | `0x01` |
| `0x0E` | 4 | width |
| `0x12` | 4 | row stride, всегда `width * 4` |
| `0x16` | 4 | height |

Плагин называет поля X/Y scale и при записи вычисляет их как
`25600 / userValue`. Однако весь 26-байтовый prefix побайтно совпадает с телом
`E0` у встроенной SMO-текстуры формата `0x29E3`. В ней те же поля находятся по
object offsets `+0x1B/+0x1F`, а основные dimensions, stride и pixels — по
`+0x28/+0x30`, `+0x2C` и `+0x34`. Текущий strict decoder уже проверяет первую
пару как дублированные `crossPlatformWidth/Height`. Поэтому лучшая рабочая
интерпретация — повторные dimensions, а не проценты scale; окончательное имя
поля всё ещё требует native evidence. См.
[`SmoTextureDecoder.cs`](../../tools/SmoViewer/SmoViewer.Core/SmoTextureDecoder.cs).

Названный в UI/writer плагина `magical_number` — не самостоятельное поле
формата. Writer моделирует byte 2 внутри little-endian `UInt32` размеров
`E1/E0` как отдельный параметр, поэтому на известных степенях двойки возникает
кажущаяся корреляция `pixelBytes / 65536`. Настоящие инварианты на всех 186
файлах такие:

```text
E0.payloadSize = pixelBytes + 0x1A
E1.payloadSize = pixelBytes + 0x20
```

Writer плагина вручную раскладывает эти размеры как несколько констант и
`magical_number`; для новых размеров это ненадёжно. Наш будущий writer должен
вычислять и проверять полные `UInt32`, а не переносить этот UI-параметр.

Исходные константы и алгоритм чтения находятся в
[`structure.h`](https://github.com/kotwys/stx-gimp-plugin/blob/f61624a695b0e5516682cd4fe0a07c7a38b96282/subprojects/stx/include/stx/structure.h)
и [`read.cpp`](https://github.com/kotwys/stx-gimp-plugin/blob/f61624a695b0e5516682cd4fe0a07c7a38b96282/subprojects/stx/src/stx/read.cpp).

### Ограничения reference parser

- из-за границы `STX_MAGIC_SIZE - 1` фактически сравниваются лишь первые два
  байта `22 00 00`;
- размеры `E1/E0` пропускаются/читаются как фиксированные куски вместо
  проверки полных `UInt32` и границ вложенных блоков;
- не валидируются row stride, terminators и точное число прочитанных байт;
- в репозитории нет STX fixtures или parser/writer round-trip tests.

Эти ограничения не отменяют ценность независимого исследования, но требуют
собственного strict parser вместо прямого копирования реализации.

## Compact `E0/E5` PC STX

56 файлов чистого PC-корпуса содержат вложенные size-prefixed блоки `E0/E5`,
raw payload и два terminator-байта:

```text
E0 UInt32(pixelBytes + 0x16)
  E5 UInt32(pixelBytes + 0x10)
    UInt32 width
    UInt32 height
    UInt32 flag
    UInt32 bytesPerPixel             # 4
    Byte[pixelBytes] pixels
  00
00
```

Пиксели начинаются с `0x1A`. Встречены размеры `32×32` (1 файл), `128×128`
(16), `256×256` (11) и `512×512` (28). Поле `flag` равно `1` в 55 файлах и
`0` у `ptc_01.stx`; `bytesPerPixel` всегда равно `4`. Обе length equations,
размер payload и нулевые terminators точны во всех 56 файлах.

Видимые байты `E0 16 10 ... E5 10 10 ...` у `ptc_01.stx` — не особая
signature: это обычные little-endian размеры `0x1016` и `0x1010` для `32×32`.
Смысл отличающегося `flag` пока не установлен. Во всех compact-файлах лежит
raw 4-byte-per-texel payload без compression/mips; порядок каналов следует
закрепить отдельным decode/native-тестом до появления writer.

## Raw 20-byte PC STX

У `Characters/Bloom/bloom_jeans.stx` наблюдается минимальный заголовок:

```text
00  UInt32  0
04  UInt32  width       # 256
08  UInt32  height      # 256
0C  UInt32  1
10  UInt32  4
14  Byte[width * height * 4] pixels
```

Trailer отсутствует. По одному файлу нельзя считать эту схему окончательной
или переносить на другие платформы.

## PS2 — отдельная задача

В открытом issue автора прямо указано, что PS2 STX имеют другую структуру и
плагин интерпретирует/рисует их неверно. Оба приложенных образца сохраняют
внешний magic `22 00 00` и ту же вложенность size-prefixed блоков, но значение
payload `E6` меняется с `6` на `8`, а `E0` содержит indexed-текстуру:

```text
22 00 00
E6 UInt32(4) UInt32(8)
E1 UInt32(...)
  E0 UInt32(...)
    Byte 1
    UInt32 1
    UInt32 width                        # 512
    UInt32 height                       # 512
    DF 1C UInt16(4) UInt32(1)           # extended field, semantic unknown
    Byte[256 * 4] palette               # RGB + alpha (126/127 в образцах)
    UInt32 3
    UInt32 256
    UInt32 256
    UInt32 indexBytes                   # 262144
    Byte[512 * 512] paletteIndices
  00
00
```

Все размеры сходятся без остатка. После перестановки RGB palette в порядок PC
BGRA восстановленные RGB-гистограммы обеих PS2-текстур полностью совпадают с
одноимёнными PC-файлами, но совпадение цветов по позициям низкое. Это сильное
свидетельство палитры с tiled/swizzled индексами, однако точный алгоритм
deswizzle пока не восстановлен. Значения двух `256` и поля `3` также не следует
именовать до дополнительных образцов. Поэтому даже совпадение внешнего magic
не разрешает выбирать PC parser без проверки внутренней структуры.

Источник и образцы: [PS2 format support, issue #4](https://github.com/kotwys/stx-gimp-plugin/issues/4).

## Что можно безопасно перенести

- использовать код и факты legacy-диалекта как независимую сверку;
- выбирать parser по структуре секций, а не только по расширению/magic;
- перед выделением pixels проверять размеры, stride, overflow, границы файла и
  trailer;
- сохранять неизвестные поля при round-trip;
- не объявлять поддержку PS2, compact и raw20 до отдельных fixtures и native
  smoke tests.

Код плагина распространяется по MIT License, copyright (c) 2020 kotwys.
Если в проект будет перенесён существенный фрагмент реализации, вместе с ним
нужно сохранить copyright и текст разрешения из
[`LICENSE`](https://github.com/kotwys/stx-gimp-plugin/blob/f61624a695b0e5516682cd4fe0a07c7a38b96282/LICENSE).
В текущем изменении чужой код не копировался.
