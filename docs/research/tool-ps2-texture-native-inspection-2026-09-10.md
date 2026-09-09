# Общий PS2 TextureData native-section metadata inspector

`spPS2TextureDataSerializer::InspectNativeSectionForAnalysis` теперь читает
native section в общем C++ коде. C ABI `spv_ps2_texture_*` передаёт ordered
fields/images/mips; `SmoTextureDataDecoder.TryParsePs2` использует этот результат
вместо своего header/palette/mip parser. Это metadata-only срез, без PS2 runtime
target, attachment, swizzle, pixel preview или GPU.

## Оригинальные правила и перенос

Original PS2 reader `00176A10..00176CEC`, outer `00176CF0..00177074`;
PC counterparts `0042D6C0/0042D910`. Exact PC source anchor:
`Z:\Sparkplug\Code\Sparkplug\spPS2TextureDataSerializer.cpp`.

- Common data-block header `0017E890` задаёт поля. Неизвестные поля пропускаются
  к payload end, повторные field0 сохраняются отдельными ordered observations.
- `00176AB4→00114D40` читает raw byte, `00176AC0→00178D80` сохраняет его в
  `spTextureData+0x78`. Нулевое значение не останавливает последующие чтения.
- `00176ACC/D8/E4/F0/FC→00114E30`: format, width, height, auxiliary, mipCount.
  Format0 даёт palette64 bytes, format1 —1024; остальные идут без palette.
- `00176B4C/58/64/70` читает четыре UInt32 каждого mip; четвёртое слово задаёт
  wire dataSize в `00176B78/176B94`. Именно оно продвигает metadata cursor.
  Вычисление размера через width×height×bpp и ограничение formats0/1/3 не перенесены.

Observer сохраняет исходные logical stream offsets и `complete` отдельно для
полей, image records и секции. Completion требует terminator в точном конце
заданного extent. Полностью прочитанные observations остаются при последующей
ошибке. Borrowed bounded stream ограничивает даже чтение size bytes общего header;
pixel bytes не копируются inspector-ом. Явные host caps: section≤16MiB,
fields≤65536, images≤4096, всего mips≤65536, с UInt32 overflow/extent guards.
Эти guards не выдаются за проверку оригинального unchecked partial-read runtime.

Имена descriptor1/2 как mip width/height не доказаны. Writer `00176494/498`
и `001764A8/4AC` только пишет record+4/+8. Managed DTO сохраняет raw descriptors,
ставит `DimensionsKnown=false`, Width/Height0 как явные неизвестные значения;
raw byte доступен через nullable `NativeDataFlag`. Unknown formats остаются raw
metadata. Старый DTO поддерживает одну image и явно отклоняет multi-image shape;
общий inspector и C ABI сохраняют их все.

## Проверка и границы

Один existing DB-selected PS2 pristine positive: `data/levels/redf/redf03.smo`,
file10013, object465/id466 `noisesm`; исходный PCK `2/ST25.PCK`. Весь выбранный
SMO SHA проверен по БД. Object267 bytes, native section235 bytes:

| Наблюдение | Значение / offset внутри native section |
|---|---|
|Header|byte1, format0, width16, height16, auxiliary99590, mipCount1|
|Palette|offset26,64 bytes|
|Mip descriptor|offset90; `[0,16,16,128]`|
|Mip bytes|offset106,128 bytes|
|Final cursor|235, включая terminator|

Static PS2 verification:14 original JAL anchors и5 exact instruction words.
Listing отдельно распознаёт R5900 LQ/SQ вместо конфликтующих generic MIPS DSP
названий. Новых MIPS guest/runtime сравнений не было: основание переноса —
оригинальные инструкции и реальные wire bytes, не прежний C# parser.

Native build завершён: PS2TextureInspection — 204 checks PASS, FullLoader —
213 PASS, TextureSerialization — 1219 PASS. Отдельный
`--fixture noisesm.native-section.bin source-positive.json` дал 15 checks PASS
и точное совпадение header/offsets. Synthetic guards проверяют raw flags0/2,
unknown format, zero mips, произвольные wire mip sizes, unknown/repeated fields,
positioned offsets, truncated data, terminator, UInt32 overflow и host caps.

Managed проверка сохранена в `managed-run1/report.json`: 15 checks PASS на
реальном `noisesm.object.bin` и его zero-flag/malformed-field вариантах.
Подтверждены image 16×16, format0, auxiliary99590, raw flags1/0 и descriptors
`[0,16,16]`; SHA256 palette и raw mip совпадают с точными срезами fixture.
Per-mip `DimensionsKnown=false`, Width/Height0 остаются неизвестными размерами.
Malformed field явно отклонён за несовпадение exact extent. Preview возвращает
`PS2_TEXTURE_PREVIEW_UNSUPPORTED`; editing path остаётся непроверенным.
Это проверка metadata-адаптера на одном реальном объекте, без MIPS runtime,
полного PS2 loader, swizzle или GPU. Execution SHA256 DLL/assemblies сохранены
в managed capture отдельно от актуальных fingerprints файлов в JSON отчёте.

Full PS2 source-wrapper dispatch пока остаётся legacy C# migration work. Отдельно
исправлена старая ошибка в [class dossier](native-class-sp-ps2-texture-data-serializer.md):
field6 перезаписывает effective platform word stack88 (`00176E1C`), который
используют branches с mask8 (`00176E60/176EF0`). Эти исправленные знания не
выдаются за уже подключённый общий PS2 wrapper. Runtime lifecycle target24767C83,
частичные ошибки оригинального allocation/attachment, swizzle и GPU остаются открытыми.

Evidence: [JSON отчёт](../../research/tools-core-ps2-texture-inspection-block-2026-09-10.json).
Source bytes и captures находятся только локально в
`local-data/results/tools-core-cycle-20260910-0730/ps2-texture-native-inspection/`;
original assets в Git не добавлены. Общий cycle report/status этим документом не менялся.
