# PC: общие raw pixels → DXTexture (CP115–116)

8 сентября 2026. Общая ветка TextureData теперь создаёт runtime DXTexture,
нормализует размеры, переносит RGBA pixels и строит полный mip chain.
33 native/source сравнения прошли точно: 16004 mip bytes и восемь state fields.
29 случаев используют DX serializer, четыре — общий TextureData serializer.
Все1399 actual native allocations освобождены; максимум276989 инструкций,
65392 байта arena. Пакет с четырьмя workers занял29,14 с.

## Original path и общая реализация

DX reader `42C640` с platform bit1=0 выбирает cross field0. Common reader
`42F180` также передаёт field0 в `42E100`. Helper читает field5: четыре слова
width, height, pixel format, pixel size; создаёт TextureBuffer и вызывает
texture virtual `+20` с `(buffer, 1, 0, 1)`. На DXTexture это
`423250 → 4ABB70 → 4AB650 → 60FDB4`, затем `4AB030` генерирует mips и
`4AAC50` считает размер с physical pitch. Внутренние helpers не заменены
callback; fixtures задают stream, COM storage и внешние CRT/debug inputs.

Source использует один `ReadCrossSectionForAnalysis` для CPU TextureData и
runtime DXTexture. Проверки raw extent/format, bounded SectionCursor и
source-wrapper общие. DX и common serializers вызывают один
`InitializeCrossDXForAnalysis`; отдельного codec в frontend нет.
`InitializeCrossMipShadowForAnalysis` устанавливает field18=0, field1C=1,
flags20=0, initialized=1, normalized width/height, format44=3,
size48=sum(physicalPitch×rows). Native-data reader из
[CP108](native-pc-texture-missing-mips.md) по-прежнему оставляет44/48
незаписанными; это различие overloads сохранено.

Размеры нормализуются существующим `NormalizeDimensionForAnalysis`: минимум2,
затем ближайшая степень двойки вверх. При совпадении обеих сторон original
сохраняет исходные pixels. При изменении хотя бы одной стороны фильтрует обе.
Первый перенос повторял pixels по увеличиваемой стороне и прошёл13/17;
случаи1×8/8×1 выявили фильтрацию неизменённой оси.

## Resize и округление

Восстановлен `619219` для wrapped triangle coefficients. Для каждого source
coordinate проходят две половины линейной функции, интегрируют её на отрезках
destination pixels, объединяют соседние одинаковые wrapped indices и отбрасывают
веса≤float32 `1e-5`. Ratio, границы и накопления сохраняют float32 stores.
Consumer `61C44F` суммирует в порядке source y, source x, destination y
contribution, destination x contribution.

`60FDB4` заменяет DEFAULT `FFFFFFFF` на `00080004`: triangle плюс ordered
dither. Missing-mip path передаёт4, без dither. Это объяснило следующий
результат16/29 при совпадающих коэффициентах. Readonly traces на входе actual
`61F153` подтвердили отсутствие gamma, premultiply и error diffusion в этих
случаях и таблицу bias/32:

```text
31 15 27 11
 7 23  3 19
25  9 29 13
 1 17  5 21
```

Нечётные строки проходят справа налево; индекс bias следует порядку прохода.
Channel умножается на255 с float32 store toward zero, затем прибавляется bias
с таким же store, затем integer truncation. Dither одинаков для четырёх
каналов. Последующие mips используют прежнее округление+.5. C++ regression
сохраняет найденный1×8 random case целиком; guards проверяют отсутствие output
mutation при неправильном extent/размере.

## Старый payload и границы доказательства

`SFX/star_currency.smo`,3994 байта, SHA-256
`2A5FC66D3F297B6015AB98D59CD911DB70E488D6E7E3DE91C6E6455E98491316`,
содержит texture slice `[385,1437)` с cross field0, но без source wrapper.
Запущенный отдельно original DX reader пропускает field, возвращает1 с
диагностикой `pStream->Read( uTmp8 ) ERROR: 0x%08X` и незаписанными texture
полями; pixels не создаются. Это не успешная загрузка текстуры и не whole-file
проверка ресурса. Source сохраняет отказ на отсутствии инициализированного payload.

Для двух положительных случаев взяты исходные1024 pixel bytes этого ресурса;
вокруг них построен явно синтетический корректный source-none wrapper. Их
успех не приписывается неизменённому целому SMO. Остальные входы — seeded
random/ramp, стороны1×1…16×16, включая1×8/8×1,3×5/5×3,7×9/9×7,15×16/16×15.
Captures и assets остаются локальными.

CP116 переносит CPU formats1–4: XRGB, P8 без входной палитры, RGB565 и ARGB4444.
Общий resize сохраняет порядок накопления и dither CP115. XRGB decode задаёт
alpha=1, encode записывает X=0; при прямом копировании исходный X сохраняется.
RGB565 использует float reciprocals1/31 и1/63, ARGB4444 —1/15; упаковка
повторяет toward-zero stores и quantization по соответствующим bit widths.

Readonly capture actual codecs до `61C44F` подтвердил обе P8 palette tables:
256 одинаковых белых RGBA записей, четыре float32=1 в каждой. SHA-256 каждой
4096-byte таблицы:
`E9BAC255F4ADC7CB4ADA9298E193A5FF66B434D15AFABD458505325F29C398C7`.
Decoder `626332` читает эту таблицу; encoder `62274A` меняет ближайший индекс
только при строго меньшей ошибке, поэтому все filtered pixels получают index0.
Source использует этот доказанный частный случай без перебора256 одинаковых
цветов. Исходные индексы при прямом base copy сохраняются. Это не реализация
квантизатора произвольной пользовательской палитры.

Новая matrix прошла70/70 за57,30 с при4 workers: форматы1/3/4, девять размеров,
random/ramp; P8, пять размеров, random/ramp; четыре common-reader случая и
два RGBA regression. Fixed C++ cases проверяют упаковку четырёх новых форматов.
Native decoder/encoder pairs: XRGB `6257F5/61F502`, P8 `626332/62274A`,
RGB565 `62589D/61F818`, ARGB4444 `625AAC/6201AA`. Format44 теперь равен CPU
format+3. Произвольные палитры, compressed cross output, gamma/error diffusion,
другие filters и все failure branches остаются открытыми.
Caps прежние:1 млн инструкций/8 с, fresh process30 с, arena128 КиБ,
allocation32 КиБ,≤5 mip levels,≤2048 bytes/surface.

```powershell
python research/native_workbench.py run pc-texture-cross-upload --workers 4
python research/probe_pc_texture_cross_upload.py 0 16x16 legacy diagnostic
```

[Манифест CP115](../../research/native-cycle-checkpoint-2026-09-08-cp115.json)
хранит fingerprints, результаты и ограничения. Class scores не повышены.
[Манифест CP116](../../research/native-cycle-checkpoint-2026-09-08-cp116.json)
добавляет70 сравнений raw formats; прежние caps и границы whole-file evidence сохранены.
