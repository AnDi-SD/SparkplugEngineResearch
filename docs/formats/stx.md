# Формат STX

## Важная граница

| Семейство | Начало файла | Статус |
| --- | --- | --- |
| legacy/tagged | `22 00 00` | BGRA8 с вложенными блоками |
| compact `E0/E5` | `E0 <UInt32 size> E5 <UInt32 size>` | четыре байта на texel; смысл flag неизвестен |
| raw 20-byte header | пять `UInt32`, затем pixels | наблюдался у `bloom_jeans.stx` |

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

В описанном legacy-варианте порядок каналов — BGRA8. Эта схема не описывает сжатие, палитры или дополнительные mip-цепочки.

### Geometry prefix в payload `E0`

Offsets ниже считаются после четырёхбайтового размера `E0`:

| Offset | Размер | Наблюдаемое значение |
| ---: | ---: | --- |
| `0x00` | 1 | `0x01` |
| `0x01` | 4 | дублированный width |
| `0x05` | 4 | дублированный height |
| `0x09` | 4 | `0` |
| `0x0D` | 1 | `0x01` |
| `0x0E` | 4 | width |
| `0x12` | 4 | row stride, всегда `width * 4` |
| `0x16` | 4 | height |

Размеры блоков — полные little-endian UInt32. Отдельные байты длины не образуют самостоятельный параметр формата:

```text
E0.payloadSize = pixelBytes + 0x1A
E1.payloadSize = pixelBytes + 0x20
```

## Compact `E0/E5` PC STX

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

Пиксели начинаются с `0x1A`. Поле `bytesPerPixel` равно 4; смысл различающегося `flag` и окончательный порядок каналов здесь не установлены. В описанной форме payload несжатый, без дополнительных mip-уровней.

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

## PS2 — отдельная задача

У известного PS2-варианта сохраняются magic `22 00 00` и вложенность блоков, но `E6` содержит 8 вместо 6, а `E0` — индексированную текстуру. Пример структуры:

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

Точная PS2 swizzle-развёртка не установлена. Совпадение внешней обёртки с PC не означает одинаковой интерпретации пикселей.
