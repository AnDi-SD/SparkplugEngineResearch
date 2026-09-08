# CP113: compressed mip generation и exact block encoding

8 сентября 2026. C++ `spDXTextureDataSerializer` теперь достраивает неполные
power-of-two цепочки DXT1, DXT3 и DXT5. В original/source matrix из15 случаев
совпали1560 packed bytes, включая640 сгенерированных. Сравниваются все уровни,
base texture state и полный cursor; supplied level сохраняется.

Это default filter4 без dithering, подтверждённый actual codec callback
arguments. Произвольные conversion modes, dithering и live GPU остаются открыты.
Число классов и scores не менялось.

## Original block encoders

| Формат | Original entry | Восстановленная ветвь |
|---|---|---|
| DXT1 | `64C798` → `64BB40` | RGB565 endpoints, 3/4 colors, threshold0,5, transparent block |
| DXT3 | `64C8BC` → `64BB40` | 4-bit explicit alpha и four-color block |
| DXT5 | `64C9EB` → `64BB40`/`64B299` | optimized6/8 alpha palette и four-color block |

[spTextureBlockEncoder.h](../../Sparkplug/Analysis/PC/spTextureBlockEncoder.h)
использует [RGB optimizer CP112](native-pc-texture-block-optimizer.md) и новый
[spTextureAlphaOptimizer.h](../../Sparkplug/Analysis/PC/spTextureAlphaOptimizer.h).
Сохранены original RGB weights, квантование, ordering endpoints, palette index
mapping и x87 store boundaries. Alpha optimizer использует table value8/7
в последнем high coefficient восьмишаговой ветви (`71B7A0`), именно как в EXE.
Прямое gradient/curvature division имеет отдельную точную extended операцию;
его нельзя заменить reciprocal/product последовательностью из RGB optimizer.

12 batches по59 блоков дали708 точных packed comparisons /9440 bytes за19,36 с
при4 workers. Это609 уникальных input/codec pairs, включая99 повторений общих
контрольных cases между seeds. Набор включает byte-derived и continuous float
colors, полностью прозрачные/непрозрачные блоки, mixed alpha и соседние с0,5
float значения. Все8 007 959 original инструкции выполнены без seams; arena288
байт на guest. Dither argument всегда0; body encoder не подменялся.

## Filter и заполнение неполных блоков

[spTextureCompressedMipFilter.h](../../Sparkplug/Analysis/PC/spTextureCompressedMipFilter.h)
соединяет [block decoder CP111](native-pc-texture-compressed-blocks.md),
wrapped filter4 и encoder. Между decode и filter сохраняются float values,
промежуточного перевода в bytes нет. Накопление идёт в original source order.
Для уровней2×2/1×1 перед encoding достраивается block4×4 повторением строк и
столбцов, согласно `627452`, а не добавлением нулевых pixels.

Reader сохраняет различие native attach и runtime attach: поля44/48 остаются
неинициализированным format/нулевым byte count, как после `4ABBA0`. Физический
pitch задаётся прежним callback; native fixture подтверждает неизменённый padding.
Malformed input не заменяет предыдущий generated output в helper. Decoded pixel
extent ограничен16 МиБ RGBA, что допускает до64 МиБ source floats и16 МиБ
destination floats плюс packed storage; это host resource guard, не native limit.

Original path `42C640 → 4ABBA0 → 4AB030 → 61039A → 60FDB4 → 61C44F`
прошёл15 fresh cases: random4/8/16 для всех трёх codecs и red/transparent4.
Matrix занял14,50 с при4 workers. Все609 native allocations освобождены;
максимум222002 инструкции и65552 байта arena. До128 КиБ arena,32 КиБ на
allocation,2048 bytes per surface,5 levels, file1m/8 с, child30 с — выбраны
до fresh guest. Original encoder calls сняты read-only hook и имеют dither0.
COM storage и ограниченные external inputs остаются явными fixture boundaries.

```powershell
python research/native_workbench.py run pc-texture-block-encode --workers 4 --deadline-utc 2026-09-08T04:00:00Z
python research/native_workbench.py run pc-texture-compressed-mips --workers 4 --deadline-utc 2026-09-08T04:00:00Z
```

Дополнительные source regressions сохраняют original red block bytes на2×2
и1×1, отказ при повреждённом extent и неизвестном codec. Полная сборка и
CTest62/62 (60,22 с), Python96/96 (13,969 с) прошли. Исходные fingerprints
и generated report metadata приведены в
[manifest CP113](../../research/native-cycle-checkpoint-2026-09-08-cp113.json).

Следующее: реальные SMO consumers, DX cross upload и другие source branches,
общий whole save, startup и live resource-to-renderer. Результат этого этапа
не означает восстановление всего texture backend или произвольного DXT codec.
