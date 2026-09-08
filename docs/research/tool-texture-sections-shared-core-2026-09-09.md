# Общие readers текстур для приложений

Девятый блок цикла до 07:30 МСК 9 сентября. Закрыт перенос raw pixel section
и PC native mip section в общий C++ reader. Полное ядро текстур, material binding
и все приложения этим блоком не объявляются готовыми.

## Реализация и исходная семантика

`spTextureDataSerializer::ReadCrossSectionForAnalysis` остаётся одним reader
вложенного field5. Дополнительный observer сообщает положение исходных пикселей
после фактического чтения четырёх слов; он не меняет объект или порядок чтения.
`spDXTextureDataSerializer::ReadNativeSectionForAnalysis` выделен из прежней
ветки whole reader. Он возвращает те же prefix, packed rows и сохранённые mip
уровни, добавляя только наблюдения offsets. Whole reader вызывает этот же метод,
затем прежние generation/attachment. Второй реализации грамматики нет.

Leaf ABI2 возвращает metadata и offsets относительно переданного поля.
`SmoTextureDataDecoder` формирует DTO и slices исходного SMO, без C# чтения
raw/mip headers. TextureTool использует этот же декодер. Отдельные FFPS слова
в TextureTool больше не читаются: каталог и диагностика приходят из общего
`SmoDocument`, использующего исходные header/FAT readers.

Прежний C# PC parser требовал presence byte=1 и format=0. Теперь действуют
существующие ограничения исходного reader: ненулевой presence, BGRA или
DXT1/3/5, подтверждённые packed row extents. Это перенос уже восстановленной
семантики, не изменение реконструкции ради теста. Raw formats0..4 доступны
как metadata; DXT preview ещё не подключён. Сохраняются явные host limits
16 МиБ на section, bounded dimensions/extents и защита неполного ввода.

Inspector получает только записанные mip levels. Недостающие уровни достраивает
runtime reader при загрузке; он не выдаёт их за исходные байты редактора.
Ни GPU, ни legacy ОС/backend не вызываются.

## Найденная ошибка XRGB

В `igmenu_opt_ps2.smo` есть cross format1: четыре байта XRGB. Старый C# parser
считал любой четырёхбайтовый pixel BGRA и использовал X как alpha. Это ошибка
инструмента. Использованы уже восстановленные `DecodeRawPixel` и
`EncodeRawChannel` из общего codec: XRGB задаёт alpha=1, BGRA проекция получает
255. BGR остаётся точным; исходный X сохраняется в raw mip slice. Viewer и
TextureTool PNG/анализ каналов используют одну эту проекцию. Доказательство
codec: [CP116](native-pc-texture-cross-upload.md), включая native XRGB
`6257F5/61F502` и различие прямого копирования от resample.

## Проверки

- Семь свежих original-PC сравнений: CPU raw RGBA/P8/RGB565 и полные
  BGRA/DXT1/DXT3/DXT5 chains4×4. Оригинал читает целые texture sections,
  вызывает реальные CPU/COM-copy методы и освобождает native allocations.
  ABI читает точно их representation bytes; сохранённые pixels/rows совпали.
  COM — ограниченная память fixture, не настоящий device.
- Исходный `TextureSerialization` suite: 1007 проверок, без полного corpus scan.
- C# FormatTests: PC-меню9300, PS2-меню2975. Добавлен случай presence byte2;
  XRGB проверяет BGR отдельно и opaque alpha вместо ошибочного сравнения X/A.
- TextureTool: шесть прежних образцов, восемь PNG, 32 замены, 4248 проверок.
  Дополнительно PS2-меню: пять PNG и40 проверок, включая XRGB alpha/каналы;
  запись PS2-текстур не заявлена. Исходные SMO сохранены побайтно.

Пробы используют стандартный micro profile64 КиБ/100k instructions/2s,
outer30s, два независимых процесса одновременно. Проверки TextureTool занимали
0,862s для шести образцов и0,326s для отдельного preview. Это тесты конкретного
workflow, не сравнительный benchmark скорости приложения.

Первый compile после выделения helper обнаружил оставшиеся ссылки на старый
scope `native`; они исправлены на внешний `local` failure path. Первый build
TextureTool с `--no-restore` использовал старый assets graph до выделения
`SmoViewer.Editing`; локальный restore исправил зависимость без загрузки пакетов.
Первая PS2-проверка обнаружила описанную выше ошибку XRGB. Итоговые проверки
прошли после исправлений.

## Оставшаяся работа

Source-wrapper aggregation и PS2 texture byte parser ещё находятся в C#;
legacy cross source0 — оставшаяся интерпретация инспектора, а не новое
доказательство whole-resource загрузки оригиналом. PC/PS2 trial dispatch
требует замены настоящим выбором serializer. Typed texture writer и material
bindings также пока не перенесены. Следующий блок — общий writer; PS2 runtime
palette/swizzle и неиспользуемые глубины не становятся отдельной целью.

Hashes и evidence: `research/tools-core-texture-sections-block-2026-09-09.json`.
