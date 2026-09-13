# PC texture: source/local codec, DX header и границы upload

## Actual factories и header

| Тип/роль | Entry | Размер |
| --- | ---: | ---: |
| TextureData | `41A2D0` | `4A0` |
| TextureBuffer | `475DF0` | `30` |
| DXTexture | `4AB520` | `4C` |
| TextureDataSerializer | `42DC30` | `14` |
| DXTextureDataSerializer | `42B660` | `14` |
| DXTextureSerializer | `4B24C0` | `14` |
| Data header | `42DD10` | DXTexture `4C` |

DXTexture ID `3F3651B6`, direct Texture `2F281E13`, record `763210`,
initializer `6D4D40`; startup `6D4D70` регистрирует runtime serializer4B24C0
с masksFF/3. DXTextureSerializer ID `196D44FE`, direct Serializer
`42429877`, record `763D80`, initializer `6D52B0`.
**DXTexture не равно wire DXTextureData `0B1C67BB`.**

## CPU raw codec: две отдельные секции

```text
source section: field2(byte0), terminator
local section: field6(u32=1),
               field0(field5(width,height,format,pixelSize,raw bytes), terminator),
               terminator
```

Actual `42F180`→`42EA50`→`42E100`, target CPU41A2D0. Field5 создаёт
temporary TextureBuffer30, Init475F30(u16width,u16height,1,NULL,format),
читает wireWidth*wireHeight*wirePixelSize bytes и вызывает target slot20
`423250(buffer,1,0,1)`. CPU backend копирует pixels в embedded buffer+38;
temporary buffer уничтожается. Base texture dimensions нормализуются отдельно
от размеров embedded buffer;1 округляется до2, source pixels не расширяются
самим CPU copy.

Actual writer `42EE10` использует общий DataBlockSerializer. SourceNone
получает minimal header22, byte0, terminator0. Затем поля6,0,5 имеют
**UInt32BeginEnd** framing. Для formats0 результат:

```text
220000e60400000001000000e01e000000e5180000000200000001000000000000000400000001020304050607080000
```

`compare_pc_texture_cross.py` сравнивает input,5-word buffer state, opaque
pixels и весь output с compiled `SparkplugTextureSerializationTests.exe`.
Общий source codec `spTextureDataSerializer` повторно использует
`Analysis/PC/spSectionCursor` и `spDataBlockSerializer`, отдельного формата нет.

## DX upload до реального conversion entry

Для buffer1×1 actual normalized destination2×2:

| CPU format | pixel size | runtime format44 | COM format |
| ---: | ---: | ---: | ---: |
| 0 | 4 | 3 | 15 |
| 1 | 4 | 4 | 16 |
| 2 | 1 | 5 | 29 |
| 3 | 2 | 6 | 17 |
| 4 | 2 | 7 | 1A |

## Подсчёт памяти и hidden reset

`4AAC50` проходит levels, GetSurfaceLevel/GetDesc и суммирует размеры.
Protected slot `13B148C` сначала указывает `13E6B70`, после actual dispatch —
`404BC6`: `mov [esi+48],0; jmp4AAC62`. Поэтому повторный вызов
**пересчитывает**, а не накапливает результат. Это исправляет первоначальную
неполную трактовку только незашифрованного хвоста.

Открыто: native DX mip codec, palette/aux class и data containers,
полное conversion/mip generation и ошибки COM/HRESULT, reuse/clone,
source embedded/reference, material texture/controller aliases,
общий FFPS Save и real resource→PC renderer. Неизвестные исходные имена
сохраняются адресами. CPU tests и synthetic COM boundaries не означают100%.
