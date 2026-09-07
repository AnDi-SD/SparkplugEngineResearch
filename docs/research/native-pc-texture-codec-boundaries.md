# PC texture: source/local codec, DX header и границы upload

Checkpoint14 продолжающегося PC SMO/SAN исследования,6 сентября2026.
Последующий [checkpoint15](native-pc-texture-runtime-mips.md) добавляет
runtime source class/codec, native mip copy и уточнение registry key;
ниже сохранена граница именно checkpoint14.
Оригинал `local-data/pc-pristine/WinxClub.exe`, SHA-256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Только PC evidence; PS2 не переоценивался. Это не полная загрузка SMO,
не законченный texture backend и не проверка в игре.

## Actual factories и header

`research/probe_pc_texture_factories.py` выполняет настоящие factories,
deleting destructors и освобождение созданных Resource/debug managers.

| Тип/роль | Entry | Размер | Primary vtable |
|---|---:|---:|---:|
| TextureData | `41A2D0` | `4A0` | `6DE950` |
| TextureBuffer | `475DF0` | `30` | `6E94DC` |
| DXTexture | `4AB520` | `4C` | `6EF6E8` |
| TextureDataSerializer | `42DC30` | `14` | `6DDD90` |
| DXTextureDataSerializer | `42B660` | `14` | `6DD648` |
| DXTextureSerializer | `4B24C0` | `14` | `6F06A4` |
| Data header | `42DD10` | DXTexture `4C` | `6EF6E8` |

`42DD10` читает8 произвольных bytes, **не проверяет classID/marker** и
безусловно вызывает DXTexture factory. Data и DXData serializers используют
этот header; runtime DXTextureSerializer имеет generic `467550`.
Прежняя подпись «generic SBOO load» для42DD10 была неточной.

DXTexture ID `3F3651B6`, direct Texture `2F281E13`, record `763210`,
initializer `6D4D40`; startup `6D4D70` регистрирует runtime serializer4B24C0
с masksFF/3. DXTextureSerializer ID `196D44FE`, direct Serializer
`42429877`, record `763D80`, initializer `6D52B0`.
**DXTexture не равно wire DXTextureData `0B1C67BB`.**

DX constructor4AAEB0 читает renderer `75DB68` + `C9E8`, сохраняет device
в+38 и вызывает COM AddRef. Поля+3C texture,+40 auxiliary,+48 byte count
обнулены; +44 runtime format оставлен неинициализированным. Dtor4AB5D0
отпускает device, затем texture и вызывает base423220. Secondary6EF6D4
расположен+14. Fixture содержит только явно объявленное renderer/device
storage и COM counters, **не исполненный renderer constructor и не GPU**.

## CPU raw codec: две отдельные секции

Проверенные маленькие inputs:2×1 pixels, formats0/2/3, pixelSize4/1/2.
Имена `rgba/gray/rgb16` — удобные labels, не доказательство channel order.

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

Host guards:16MiB, exact extent, positive16-bit dimensions, known format и
matching pixel size, обязательные terminators, хотя оригинал не проверяет
все эти условия. Перед mutation весь raw field прочитан. Отсутствие pixels,
source fields3/4, native field1 и derived/DX targets явно отвергаются.
Unknown fields пропускаются, **ещё не lossless**. Actual42DD10 в source
пока явно возвращает unavailable после8 bytes: не заменяет DXTexture на
ложный CPU object. Полная runtime factory/backend reconstruction следующая.

Начальный fixture без source-wrapper terminator позволил native пропустить
local поля и выдать Read diagnostic при EOF; это была ошибка framing теста,
а не доказательство загруженных pixels. Исправление следует actual42EC58 loop.

## DX upload до реального conversion entry

`TextureUploadBoundaryFixture` содержит только tiny declared COM objects.
`probe_pc_texture_upload_boundary.py upload-0..7` исполняет actual
`423250`→secondary `4ABB70`→`4AB650` и **останавливается перед60FDB4**.
Никакого success seam для60FDB4/61039A нет; их тела находятся внутри EXE,
предположение о D3DX-происхождении пока не признано доказанным именем.

Для buffer1×1 actual normalized destination2×2:

| CPU format | pixel size | runtime format44 | COM format |
|---:|---:|---:|---:|
|0|4|3|15|
|1|4|4|16|
|2|1|5|29|
|3|2|6|17|
|4|2|7|1A|

Flags1/2/3 выбирают runtime0/1/2 и COM DXT1/3/5 соответственно.
Mapping helpers `4AABD0` и `4AADC0`. CreateTexture slot5C получает
width2,height2,levels0,usage0,format,pool1,out,sharedNULL. Затем
GetSurfaceLevel slot48 получает level0. Conversion entry аргументы:
surface,NULL,NULL,sourcePixels,**source** COM format,pitch=sourceWidth*pixelSize,
NULL,rect[0,0,1,1],filterFFFFFFFF,key0. Source/destination formats могут
различаться при compression flags; fixture ничего не конвертирует.

Stop уничтожает suspended guest stack; fixture отдельно отпускает
полученную surface reference и восстанавливает синтетический SEH sentinel,
затем выполняет оригинальные destructors. Это **не нормальный возврат upload**.
8 cases по7 assertions, максимум2360 instructions/52496 arena bytes.

## Подсчёт памяти и hidden reset

`4AAC50` проходит levels, GetSurfaceLevel/GetDesc и суммирует размеры.
Protected slot `13B148C` сначала указывает `13E6B70`, после actual dispatch —
`404BC6`: `mov [esi+48],0; jmp4AAC62`. Поэтому повторный вызов
**пересчитывает**, а не накапливает результат. Это исправляет первоначальную
неполную трактовку только незашифрованного хвоста.

Raw surface: LockRect, **Pitch*Height**, UnlockRect, Release.
DXT1: max(1,width>>2)*max(1,height>>2)*8; DXT3/5 — multiplier16.
Именно floor, не ceil: declared6×5 DXT5 даёт16. Нельзя выдавать такой
synthetic invalid-device case за допустимую реальную D3D texture.
Четыре size cases выполняются дважды с исходным sentinel+48 и подтверждают
reset, balanced locks/refs;28 assertions, first≤1668 instructions.

## Проверки и оставшийся объём

Profile `pc-texture-codec-boundaries`:7 factories,3 exact CPU comparisons,
8 upload-entry stops и4 complete size queries. **116 native assertions**
=20 factory+12 CPU+56 upload-boundary+28 size;3 exact rows.
Source texture suite113 checks; полный CTest32/32,20.89sec после fresh build.
Все caps прежние:100k instructions/2s per call,30s child,64KiB arena,
32KiB native allocation request. К capped cases не возвращались.

Первоначальные missing renderer C9E8 и cleanup ResourceManager были
uncapped fixture faults; диагностированы и исправлены явным storage/teardown.
Тест ожидал ошибочно1×1 destination и накопление size; actual observations
и existing normalization/decoded prefix исправили ожидания, не native code.

Открыто: native DX mip codec, palette/aux class и data containers,
полное conversion/mip generation и ошибки COM/HRESULT, reuse/clone,
source embedded/reference, material texture/controller aliases,
общий FFPS Save и real resource→PC renderer. Неизвестные исходные имена
сохраняются адресами. CPU tests и synthetic COM boundaries не означают100%.
