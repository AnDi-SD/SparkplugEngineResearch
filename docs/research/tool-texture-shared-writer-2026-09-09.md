# Общий texture writer: создание и замена

Десятый блок цикла до 07:30 МСК 9 сентября. `SmoTextureDataWriter` больше
не собирает вручную prefix/размеры/rows/pixels. Создание нового встроенного
BGRA-объекта и замена одного записанного mip используют исходные C++ writers.
Это общий путь TextureTool и Importer, без нового UI/release.

## Подключённые операции

`spDXTextureDataSerializer::WriteMipRecordForAnalysis` выделен из прежней
ветки `42B9E0`: first prefix, три слова row header, pixels. Whole writer
`WritePayloadWithContextForAnalysis` вызывает этот же метод. C ABI для записи
готовит один `spTextureData::NativeMipForAnalysis`, вызывает writer и возвращает
владеющий output handle. C# только копирует результат и освобождает SafeHandle.
`field1C` читается исходным reader и сохраняется как byte, включая0/2; native
presence writer всегда пишет1, как оригинал. Значение `field1C` не переименовано
в вымышленную игровую настройку.

Создание нового объекта вызывает исходные object header и полный DX data
writer, затем общий embedded-source writer. Generic lossless editor пока
оставлен для замены leaf field и сохранения неизвестных соседних bytes, FAT,
ID/inline extents. Это операция редактора поверх сериализации; C# generic
data-block writer всё ещё требует переноса. Не заявляется полный Save графа.

Ограничение текущего пути — одна встроенная PC BGRA-репрезентация и один
записанный mip. C ABI проверяет dimensions1..16384, длину, byte range и bounded
16 МиБ input/sections. Недостающие mip levels при чтении создаёт runtime,
редактор их не выдумывает и не записывает вместо оригинальных данных.

## Уточнение source stream ownership

Свежие вызовы оригинального `42E5F0` с настоящим `spMemoryStream` показали:

1. читается `GetSize`, копируется весь `GetBuffer`, независимо от cursor;
2. field3 использует UInt32-reserved header;
3. после успешного окончания field stream закрывается и уничтожается;
4. затем ставится handled=1 и записывается terminator;
5. native generic attachment `object+0C` остаётся dangling. Base destructor
   не удаляет его повторно.

Таким образом, прежнее описание сохранения/восстановления позиции в
`native-class-sp-texture-data-serializer.md` неверно для этой PC-ветки.
Историческое досье/его hashes сохранены; данный результат заменяет именно
тот вывод. Probe также выполнил protected factory465560: фактическое выделение
MemoryStream0x38 и vtable6E7E50 подтверждены динамически.

`WriteEmbeddedSourceForAnalysis` выполняет ту же последовательность. В host
границе передаётся явный `unique_ptr<spMemoryStream>` и после уничтожения
очищается owner. Generic native attachment+0C не моделируется и не выдаётся
приложению за живую ссылку. Это граница управления памятью, не изменение
исходной последовательности записи. Полный attached-source dispatch, external
source writer и повторное использование висячего native pointer не заявлены.

## Размеры без округления

Прежний host input helper `SetNativeMipDataForAnalysis` запрещал все NPOT
dimensions. Оригинальные setters и полный writer записали raw13×7,1×9,3×2
без округления. Этот лишний guard удалён для raw; compressed POT boundary
сохранена, её расширение здесь не исследовалось. Самостоятельная реализация
алгоритма ради быстродействия не вводилась.

## Проверки

- Четыре original-PC writer cases:1×1/flag1,13×7/flag1,1×9/flag2,3×2/flag0.
  11/11 allocations освобождены в каждом, writer14 142–14 164 instructions.
- Два original embedded-source cases с cursor0/5: полное совпадение bytes,
  actual Close/delete,7/7 allocations,9 280 instructions, arena1616 bytes.
- C ABI совпал побайтно с четырьмя original mip records и object/source
  framing. Проверены три ошибочных input и повторный успешный вызов после них.
- TextureSerialization1007 checks; TextureTool4272 checks на шести файлах,
  включая создание двух новых объектов, сохранение field1C0/2 и untouched
  enclosing fields. Importer192 checks на gem/Bloom_body прошли.
- Все32 edited outputs совпали SHA256 и bytes с прежней выборкой CP125.
  Для уже проверенных original whole-file inputs это позволяет переиспользовать
  старую проверку по идентичному input hash, без повторного запуска всего корпуса.
  Непроверенные тогда файлы не получают нового whole-file зачёта.

Стандартные micro limits64 КиБ/100k/2s, outer30s, два независимых процесса.
Пробы отдельно записывают instruction count самого writer до teardown;
счётчик эмулятора сбрасывается между вызовами. GPU/game process не запускались.
Доказательства setters/vector input не объявляются реконструкцией native
создания mip vector; его storage задан fixture явно.

Manifest: `research/tools-core-texture-writer-block-2026-09-09.json`.
Следующая работа — общий data-block writer и оставшиеся typed resource cores.
