# PS2 mesh header и bounds для tools

Восьмой блок цикла до 07:30 МСК. Инспектор получает PS2 native mesh metadata
и field2 bounds через восстановленные методы `spPS2MeshDataSerializer`.
C# byte parser этих данных удалён. Это чтение нужного инструменту префикса,
не реализация PS2 DMA/VIF/GIF renderer, packet builder или runtime attachment.

## Подтверждённые операции

Original PC42A420 и PS2163160 сначала читают16 bytes **в sphere выходного mesh
по offset18**, затем шесть UInt32: primitive count, vertex count, component flags,
packet qwords, counter38, counter3C. Старое описание «descriptor header+18» нельзя
читать как первые16 bytes `spPS2MeshData`: у этого data-класса другой layout.
После префикса начинается packet allocation, затем factory/attachment — эти
операции в новом API не заявлены восстановленными.

`ReadNativeHeaderForAnalysis` сохраняет именно этот порядок. Struct — явный host
snapshot полей, не новый RTTI-класс игры. `ReadBoundingBoxForAnalysis` сохраняет
два последовательных12-byte чтения PC42AC2A/PS21635C8. Операции доступны через
существующий serializer-класс, а не параллельный reader инструмента.

PC header probe останавливается **0042A4B1**, до packet allocation, после88
инструкций вызова. Snapshot совпал с исходными40 bytes и в обычном случае,
и при sphere radius=-4, vertex count999, component flags0, counters7/2.
Это доказывает сохранение metadata на данном этапе, не успешную последующую
обработку такого packet. Остановленный frame явно отброшен, не возобновлялся.
В обоих случаях освобождены3/3 tracked allocations.

Отдельный bounds-only input прошёл **полный original PC42AB40**:
**3 525 инструкции**,5/5 allocations освобождены. Reverse minimum/maximum
(3,4,5)/(-1,-2,-3) скопированы без перестановки. Source comparisons также
сохранили все24 bytes.

PS2 prefix проверен статически: Read16 в mesh+18, шесть calls00114E30, те же
stack outputs; field2 делает два Read12 и переносит values в mesh+2C..40.
Hash SLES_532.19 и prefix записаны в probe reports. Это отдельное PS2 evidence,
но не запуск PS2 кода в эмуляторе. Ошибочные DSP-названия R5900 quadword-save
инструкций в обычном Capstone не использованы как доказательство алгоритма.

## Приложение и guards

ABI2 дополнен `spv_ps2_mesh_header` и `spv_mesh_bounds`. Native header inspector
проверяет только bounded envelope:40 bytes плюс ровно qwords×16, максимум32 MiB,
без переполнения. Payload остаётся opaque и не аллоцируется/исполняется.
Bounds требует24 bytes. Отказ при неполном Read — явная host защита от original
unchecked-read/uninitialized-local поведения.

C# больше не выводит допустимость counters из vertex-format bits, не ограничивает
UV counter значениями0/1 и weights значениями0/4, не меняет raw sphere/bounds.
Bounds можно инспектировать самостоятельно, без обязательного platform payload.
Header platform mask8 безPC bit2 выбирает PS2 metadata; остальные inputs используют
PC field view. Пробное распознавание PS2 после неудачи PC reader удалено.
Поддержка полного непривычного field order/повторов в C# metadata aggregation
ещё требует отдельной замены; эта обвязка пока сохраняет прежний порядок0?/1?/2?.

## Проверка и остаток

Три original observations сверены побайтно через ABI: два40-byte prefixes и
24-byte bounds. Original fixture неизменён, hashes EXE/probe проверены перед
повторным сравнением. Для обычного prefix source дополнительно получил16
синтетических нулевых opaque bytes по qword count1; original их не исполнял.

C++ DLL и C# сборки прошли. C# FormatTests: **2 960 assertions** на
igmenu_opt_ps2, **9 833** на mmenu_new_ps2, **9 285** на igmenu_opt_pc.
Добавлены unusual counters/radius, bounds-only reverse values и overflow envelope.
Полный corpus и незатронутые C++ suites не запускались повторно.

Локальные результаты:
`local-data/results/tools-core-cycle-20260909-0730/ps2-mesh/`.
Снимок: `research/tools-core-ps2-mesh-metadata-block-2026-09-09.json`.
Следующий приоритет — texture resource reading, общий для viewer и TextureTool.
