# PS2: нормализация vertices и сборка mesh packet

`spPS2MeshData` получает общие `spIndexBuffer` и `spVertexBuffer`, выбирает
комбинацию streams, разворачивает индексы и записывает EE DMA/VIF packet.
Это работа CPU до исполнения VU. Полученные bytes сами по себе не доказывают
численную семантику VU, состояние GS или правильное изображение.

Исходники: [план и таблицы](../../../Sparkplug/Code/Sparkplug/spPS2MeshData.cpp),
[нормализация](../../../Sparkplug/Code/Sparkplug/spPS2MeshDataNormalization.cpp),
[packet builder](../../../Sparkplug/Code/Sparkplug/spPS2MeshDataPacket.cpp).
Общий [component emitter](../../../Sparkplug/Code/Sparkplug/spPS2MeshPacketEmitter.cpp)
используется и Type2 builder, и отдельным
[direct strip payload](../../../Sparkplug/Code/Sparkplug/spPS2MeshDataStripPacket.cpp).
Объявления и границы host API находятся в
[spPS2MeshData.h](../../../Sparkplug/Code/Sparkplug/spPS2MeshData.h).

## Нормализация vertex buffer

PS2 body `0x00160900` возвращает исходный buffer, если есть UV0 (`0x800`) и
хотя бы один из Normal (`0x40`) или Color (`0x100`). Иначе создаётся новый
buffer с добавленным UV0 и, только при отсутствии обеих альтернатив, Color.
Число vertices и `m_uFlags` сохраняются; stride и word offsets рассчитывает
общий [spVertexBuffer](../../../Sparkplug/Code/Sparkplug/spVertexBuffer.cpp).

Для каждой вершины порядок записей следующий:

1. position: три исходных 32-битных слова;
2. Color: одно исходное слово либо default `0`;
3. Normal: три исходных слова, если был исходный bit `0x40`;
4. при исходном UV0 — по два слова для каждого установленного UV bit
   `0x800..0x40000`; иначе два нулевых слова UV0.

Никакой float conversion здесь нет. Веса, RHW и остальные компоненты
автоматически не копируются, даже когда их bits сохранены в новом layout.
`writtenBytes` отмечает каждую записанную destination byte. Неотмеченные bytes
сохраняют переданный caller снимок allocation; без такого снимка они имеют
нулевое начальное значение собственного host buffer. Это различие относится
к allocation, а не к восстановленному default для компонента.

Сохраняется необычная ветвь оригинала: при Normal без Color и UV0 новый
layout не получает Color, однако default Color всё равно записывается по
нулевому offset отсутствующего компонента. Поэтому position.x становится
нулём. Подмена этой записи «исправленным» layout изменила бы результат.

Совместимый buffer заимствуется и не изменяется; расширенный принадлежит
результату нормализации. Проверки размеров, byte budget и точного размера
captured allocation — ограничения аналитического API. Native allocation
failures, faults и aliasing этим API не исполняются.

## Поддержанные целые packets

`BuildPacketForAnalysis` создаёт весь окончательный packet для Type2
(список треугольников), с constructor defaults descriptor/order tables.
Поддержаны position, Color, Normal и непрерывные
UV0..UV7. Нормализация входит в эту операцию. Её неописанные bytes никогда
не становятся неявными входами поддержанного packer.

| Ветка | Payload |
| --- | --- |
| `grouping == 1`, без Normal | По отдельному stream: UV, Color, position |
| `grouping != 1` | Для каждой вершины: UV, Color при наличии, Normal при наличии, position |

Вторая строка означает любую величину, отличную от `1`, а не только `0`.
Поле `+0xF8` выбирает эту ветвь. Для Type2 исходный порядок uint16 indices
сохраняется, вершины разворачиваются в этом порядке, ADC lanes равны нулю.
Число emitted vertices равно числу indices, primitive count берётся из
index buffer. Chunk capacity для поддержанной невзвешенной ветви:

```text
reservation = hasWeights ? 234 : 362
capacity = floor(floor(reservation / (streamCount + 6)) / 3) * 3
```

Builder `0x0015F900` пишет внешний 16-byte DMA RET tag с двумя словами
`0x11000000`; перед записью owned packet целиком очищается вызовом
`0x004076A8(packet, 0, allocationQwords * 16)`. Поэтому alignment padding
поддержанной non-external ветви имеет определённое нулевое значение.
Окончательный QWC записывается как low16 от `finalQwords - 1`.
При слишком большом packet этот счётчик может завернуться; аналитический
результат не объявляется аппаратно допустимой DMA chain.

### Отдельные streams

`0x0015EA70` обходит constructor order: UV0..UV7, Color, Normal, position,
пропуская отсутствующие компоненты. `0x0015EC20` создаёт для каждого stream
четыре header words:

```text
0, 0,
0x01000000 | (vertexCount << 16) | 0x100 | streamCount,
(format << 24) | (vertexCount << 16) | 0x8000 | unsignedFlag | ordinal
```

Для default UV format равен `0x64`, payload — raw пары `(u,v)`;
для Color format `0x6E`, `unsignedFlag=0x4000`, payload — bytes `(R,G,B,A)`;
для position format `0x6C`, payload — `(x,y,z,0)` raw words.
Каждый payload выравнивается на 16 bytes. Завершающие слова chunk:
`0, 0x04000000 | vertexCount, 0x01010101, 0x17000000`.

Default Normal descriptor `0x1006F` использует другой producer с float
quantization. Эта ветвь возвращает `UnsupportedCompressedNormal`; raw Normal
из interleaved пути не подставляется вместо неё.

### Interleaved

`0x0015EDB0` начинает chunk словами
`0,0,0,0x6C008000 | ((streamCount * vertexCount) << 16)`.
Далее для каждой вершины идут 16-byte records:

- каждый UV: `(u,v,0x3F800000,0)`;
- Color: четыре **целочисленных** 32-bit channel values `(R,G,B,A)`;
- Normal при наличии: `(nx,ny,nz,0)` raw words;
- position: `(x,y,z,0)` raw words.

Завершение: `0,0,0x04000000 | vertexCount,0x17000000`.
Не следует интерпретировать integer Color lanes как IEEE float либо
приписывать этим records отдельный завершённый GIF/GS backend.

### Веса и bone indices

Поддержан Type2 с последовательными weight bits `0x2..0x10` (от одного до
четырёх) и обязательным bone-index bit `0x20`. Исходный vertex buffer уже
должен иметь UV0 и Normal либо Color, либо caller передаёт точные bytes
новой allocation через пятый аргумент `capturedExpandedAllocation`.
Если требуется expanded normalization без этих bytes, результат остаётся
`UnsupportedWeightedNormalization`: native normalizer оставляет retained
weights/bones незаписанными. Их значения определяются внешней allocation,
а не исходными weight fields и не нулевым host default.
Отсутствующий bone flag и holes в weight bits также остаются
вне поддержанного domain.

`streamCount` включает два дополнительных streams, но отдельные обычные
streams обходятся с `ordinaryStreams = streamCount - 2`. После них producer
`0x0015D6C0` пишет header:

```text
0,0,0x01010101,
0x6C008000 | (vertexCount << 17) | unpackAddress
```

Для каждого index следуют четыре исходных weight words и четыре integer
bone-index lanes. У отсутствующих хвостовых weights/indices записан `0`;
присутствующий bone byte превращается в `uint32(byte) << 2`. Это raw copy и
целочисленная упаковка, без нормализации суммы weights или float conversion.
Операция общего record вынесена в
[spPS2MeshWeightPacking](../../../Sparkplug/Analysis/PS2/spPS2MeshWeightPacking.h)
и вызывается builder, а не заменяется successful callback.

Для grouped `unpackAddress = ordinaryStreams * vertexCount`. Для interleaved
native accumulator увеличивается на **полный** `streamCount`, поэтому здесь
`unpackAddress = streamCount * vertexCount`. Эти адреса намеренно различаются.
Interleaved начальный header также сохраняет NUM `streamCount * vertexCount`
даже при последующем отдельном weight header. Дополнительный header увеличивает
chunk на один qword; весовые records занимают два qwords на vertex.

Переданный allocation snapshot не изменяется и должен иметь ровно размер
нового vertex buffer. Общая normalization copy/default sequence и write mask
применяются к его копии; затем packet builder читает полученный buffer через
обычный packer. Это контракт содержимого неинициализированной native memory,
а не попытка определить результат `malloc` без входных bytes. Та же ветвь
сохраняет absent-Color overwrite position.x при Normal без UV0/Color.

## Отдельная compressed-Normal arithmetic операция

[spPS2MeshNormalPacking](../../../Sparkplug/Analysis/PS2/spPS2MeshNormalPacking.h)
сохраняет operation order `0x0015E584`: компоненты Z,Y,X; `1+n`, умножение
на15.5, ordered compare с2^31, условное unsigned adjustment, CVT.W и пять
младших bits каждой компоненты в `0x8000 | Z<<10 | Y<<5 | X`.
Arithmetic provider явно определяет допустимые входы и может вернуть отказ.

Собственный `ExactMeshNormalArithmeticForAnalysis` использует integer dyadic
arithmetic без host float. Он принимает finite normal/zero inputs и только
точно представимые ADD/MUL/SUB results; rounding, subnormal, overflow и
underflow не подменяются IEEE предположением. Десять конечных records,
включая signed zero, дали20 bytes, совпавших с неизменённым original
instruction stream на generic R4000 при явном RZ. Отдельный rational observer
проверил30 ADD,30 MUL,30 comparisons и30 CVT; он не записывал arithmetic
results. Annulled BC1TL/SUB slots учитывались отдельно с проверкой отсутствия
FPR эффекта.

Это квалификация конкретных packing bits, а не аппаратного EE FPU.
Upper unsigned branch не исполнялась на этих records. Общие rounding,
exception/status flags, timing и другие inputs не закрыты. Default grouped
compressed-Normal packet path по-прежнему возвращает incomplete: отдельный
provider не превращает неизвестные arithmetic domains в принятый packet.

## Владение, результат и проверки

Аналитический builder сначала собирает bytes во временном owning vector.
Только при полном результате обновляются packet, metadata и признак
`HasPreparedPacketForAnalysis`. Host guards и incomplete results оставляют
прежний packet. Оригинальный `0x00160D10` освобождает прежний owned packet
раньше: точное воспроизведение его allocator/fault side effects не является
контрактом этого безопасного API. `PacketInfoForAnalysis` содержит план,
grouping, окончательный QWC и оба счётчика; host layout не является PS2 ABI.

`completed` отдельно от `nativeReturn`: известные Type1/другие типы и пустой
Type2 возвращают completed/false, Type4 — completed/true без создания packet.
Неподдержанные weights/layouts/components и compressed Normal
возвращают incomplete. Успешный callback вместо неизвестного producer не
используется. RTTI clone по-прежнему создаёт объект с пустым packet state.

### External buffer и различие declared/written extent

Native constructor задаёт `FC=0`, `F8=1`. Два известных factory-to-initialize
маршрута сохраняют эти значения. Отдельный caller `0x001F6A90` получает
cached mesh по vertex flags, задаёт `FC=1` и внешний packet pointer:
`manager.buffer0C + 16 * manager.cursor14`. Перед `0x00160D10` он также
записывает primitive count `+34` и source count `+2C`. Initialize при `FC!=0`
принудительно задаёт `F8=0`. Поэтому external interleaved storage имеет
отдельный контракт владения; ему не нужны array cookie и owned shrink.

У interleaved size helper `0x001605E0` reserved extent равен
`streams * vertices + 2 * chunks`, без общего RET qword. Weighted emitter
пишет дополнительный weight header на chunk. В external path `FC=1`
пропускает последующую коррекцию размера по emitted end pointer. Для одного
chunk с тремя vertices и пятью streams это даёт published `mesh+44=18` и
`RET.QWC=17`, хотя вместе с RET записаны19 qwords. Сам raw body соответствует
поддержанному weight packing; различие metadata остаётся особенностью
оригинального маршрута. Факт использования weighted inputs реальным верхним
игровым consumer этим не установлен.

Caller после успешного возврата увеличивает `manager.cursor14` на published
`mesh+44`; сравнение unsigned cursor с `manager.capacity20` выполняется
**после записи**. Его нельзя считать проверкой свободного места до emission.
Обычный unweighted interleaved случай имеет совпадающие declared и written
extents. Сочетание `FC=0,F8=0` не установлено как вход известного свежего
factory/initialize маршрута; точный allocator request для weighted случая
недостаточен для полного written extent. Дополнительный usable slack
оригинального allocator не подразумевается.

`BuildPacketForAnalysis` возвращает безопасно размещённый **owning packet**
с QWC по окончательным bytes. Это не API внешнего native buffer/cursor:
undersized observed QWC в него не переносится. При интеграции external route
нужно отдельно представлять observed metadata, written extent и доступную
capacity либо возвращать явный несовместимый/incomplete результат.
Нельзя объявить такой packet полностью совместимым, сравнив только raw body.
Nonzero initial bytes внешнего allocation, остальные caller paths и lifetime
serialized packets остаются отдельными границами.

### Type3: известный native отказ

В каноническом PS2 image вызываемый stripifier `0x001C4FE0..0x001C5008`
занимает40 bytes и возвращает `0` при любом index type. Он загружает поле
type по `a1+0x14`, сравнивает с3 и2, но обе ветви и общий выход записывают
нулевой result. Caller `0x00160C00` устанавливает поля strip object
`+0x38=2`, `+0x3D=0`, `+0x40=capacity` и сразу распространяет этот отказ.
Обход списка и дальнейший strip producer из этого вызова не достигаются.

Поэтому Type3 получает `completed=true`, `nativeReturn=false`, status
`NativePreparationRejected`. Это не успешный packet и не подстановка
несуществующего stripifier. Внешняя factory/allocation/fault модель остаётся
за границей аналитического API. Другие вызовы низкоуровневых функций с заранее
заполненными strip objects этим фактом не закрываются.

### Прямой prepopulated strip route

У `0x0015F1F0` есть отдельный вход с готовым strip object. Входной record
содержит `uint16 count` по `+0` и указатель на indices по `+4`. Producer
копирует его в owning record: indices pointer `+0`, ADC pointer `+4`,
`uint32 count` по `+8`. Начальный ADC заполнен нулями. Записи разделяются
на списки чётной и нечётной длины с сохранением исходного порядка.

`0x0015FD30` greedily добавляет целые записи из того же списка, если их
длина помещается в оставшуюся capacity. Уже обработанные и поглощённые
записи пропускаются. При нечётной накопленной длине indices добавляемой
записи разворачиваются; первые два её ADC bytes получают1. Исходная запись
поглощается и позднее удаляется. Деления слишком длинного strip здесь нет.

Оставшиеся чётные записи, затем нечётные, попадают в общий список.
`0x001611E0` выполняет stable merge sort с comparator `0x0015D560`:
сортировка по убыванию unsigned count, порядок равных длин сохраняется.
Каждый результат передаётся обычному grouped/interleaved emitter вместе
со своими indices/ADC/count. Возвращаемое значение `0x0015F1F0` — конечный
output pointer, а не число qwords.

[Captured-value helper](../../../Sparkplug/Analysis/PS2/spPS2MeshStripPreparation.h)
реализует parity partition, join/reversal/ADC и stable sort для независимых
captured arrays. Он отдельно от стандартного Type3 entry. Его guards
отклоняют counts меньше3, counts больше capacity и недопустимые budgets;
native fault/allocator behavior этих случаев не подменяется успешным
packet. Helper не исполняет списочные указатели и не создаёт DMA payload.

`BuildPrepopulatedStripPayloadForAnalysis` принимает исходные strip arrays,
capacity, совместимый нормализованный vertex buffer и явный captured vertex
plan. Он применяет этот helper и записывает один chunk на каждый survivor
через тот же component emitter, который использует Type2. Для position
четвёртое слово равно `0x8000`, если соответствующий ADC byte ненулевой,
иначе `0`. UV, Color, raw Normal и weight payload от ADC не зависят.

Результат содержит owning payload bytes и emitted vertex count. Внешнего RET
в нём нет: длина соответствует смещению возвращаемого direct end pointer.
Packet и metadata самого mesh instance не меняются. API не выполняет
normalization, Type3 stripifier или native allocator. Destination представлена
явно нулевой allocation, поэтому это также контракт padding bytes,
оставляемых component emitter без записи. Grouped compressed Normal остаётся
incomplete; успешный arithmetic provider вместо него не подставляется.

Для ограниченных captured arrays сравнение с неизменёнными original words
подтвердило parity partition, join/reversal/ADC и порядок результатов,
включая равные длины между parity groups. Отдельный случай без допустимых
joins дошёл до рекурсивных `0x001613C0` и `0x001615B0`: stable merge сохранил
исходный порядок равных counts. Это существенно уже, чем одно исполнение
базового compare/swap `0x001611E0`.

Original prefix исполнялся до `0x0015F700`, до передачи результатов emitter.
Внутренние list/join/sort bodies не заменялись callbacks. Только конечные
heap/libc операции имели явную модель; integer control/delay-slot seams
интерпретировались отдельно в ограниченном generic R4000 harness. На границе
оставались принадлежащие активному frame records, indices, ADC и list nodes.
Поэтому эти сравнения не подтверждают whole direct emitter, освобождение
всех allocations, hardware EE или VU semantics.

Отдельно проверен полный direct `0x0015F1F0` для ограниченного compatible
unweighted UV0/Color layout, с join и recursive-sort случаями в обоих grouping
режимах. Original исполнял настоящие component emitters, возвращал конечный
output pointer и освобождал все временные frame allocations. Фактические
payload bytes текущего C++ совпали с этим original output, включая ADC lanes.
Проверка относится к явно нулевой destination allocation. Terminal heap/libc
оставались моделями: одинаковая native allocator strategy и аппаратная EE
семантика не утверждаются. Это также не успешный Type3 stripifier и не
исполнение внешнего `0x0015F900` по недостижимой положительной ветви Type3.

### Reservation готовых strips

[ComputeMeshStripReservationForAnalysis](../../../Sparkplug/Analysis/PS2/spPS2MeshStripSizing.h)
сохраняет integer body `0x00160750` и component accumulator `0x001604B0`.
Его вход — captured flags,22 descriptors, stream count, grouping, declared
list count, аргумент A2 и counts фактически пройденных records. Сам helper
не запускает native constructor, stripifier, caller или allocator.

При `grouping != 1` результат равен low32 от
`streamCount * A2 + 2 * declaredStripCount`. List body не читается, поэтому
record budget этой ветви не применяется. В known outer caller `0x0015FC38`
A2 берётся из capacity `floor(reservation / (streamCount + 6))`, где
reservation равен362 либо234. Это не сумма длин strips; Type2 округление
capacity до кратного3 к этой ветви не относится.

При `grouping == 1` создаются22 нулевых component byte counters. Для каждого
реально пройденного uint16 count выбранные компоненты получают
`align16(formatWidth * count)`, с low32 addition. Position component0 выбран
всегда; остальные выбираются flags, а components3/4/5 пропускаются.
Low16 descriptor задаёт width: `0x64→8`, `0x68→12`, `0x6C→16`, `0x6F→2`, `0x6E→4`;
неизвестный format даёт0. Этот
[lookup](../../../Sparkplug/Analysis/PS2/spPS2MeshPacketFormat.h) используется
также общим packet emitter для его поддержанных constructor formats.

Итог grouped равен low32 от
`streamCount * declaredStripCount + ceil(low32(sum(counters)) / 16) + declaredStripCount`.
Суммируются все22 counters. Header/trailer counts зависят от declared count,
а payload counters — от действительно пройденных records. Эти значения
могут различаться; A2 в grouped ветви не участвует. Результат не включает RET
и не является безопасной границей allocation для произвольного captured
state. Owning packet builder продолжает рассчитывать свой размер по
окончательным emitted bytes.

Host record budget отклоняет oversized grouped input до частичного результата.
Он не подменяет native pointer faults или циклический список. Low32 result
не описывает побочные HI/accumulator effects и не доказывает достижимость
положительной Type3 ветви через canonical stripifier.

Для ограниченных captured inputs actual original body и скомпилированный
текущий helper дали одинаковые low32 QW и grouped component counters,
включая несовпадение declared/traversed counts и bit-wrap случаи.
Это integer sizing qualification: width для compressed Normal не является
численной проверкой его quantization или VU исполнения.

[PS2MeshPreparation tests](../../../Sparkplug/Tests/spPS2MeshPreparationTests.cpp)
задают целиком literal grouped/interleaved packets, проверяют chunk boundary,
все поддержанные UV/Color/Normal комбинации, normalization write mask,
untouched allocation, offset-zero quirk, malformed inputs и отказ без
потери прежних bytes. Эти host tests не являются исполнением оригинала на EE.

## Оставшиеся пути

| Путь | Недостающая операция |
| --- | --- |
| Прямой low-level strip path | Bounded unweighted UV0/Color direct payload/end/temporary-free сравнен целиком; остальные layouts и native allocator/fault domains отдельно. Reservation арифметика вынесена в captured-value helper |
| Weighted allocation / неподдержанные flags | Expanded path использует явный allocation snapshot; его отсутствие, отсутствующий bone flag, holes и другие нестандартные layouts остаются вне определённого packet domain |
| Grouped compressed Normal | `0x0015DFD0`, default format `0x6F`: EE add/multiply/unsigned conversion, rounding/edge values и 16-bit packing |
| Изменённые descriptor/order tables | Другие formats `0x68/0x6C` и изменения порядка; публичного setter и заявления о поддержке нет |
| External/serialized packets | Borrowed packet mode, allocation/lifetime и полная serializer grammar |
| Исполнение | EE cache/DMA memory, выбор VU unit, VU arithmetic/dataflow, GS state и visual/runtime qualification |

Это восстановление ограниченных операций, а не закрытие всего класса или
всего PS2 renderer. Смежные контракты: [PS2 scalar preparation](ps2-cpu-preparation.md),
[VU images и ISA](../../reference/shaders/ps2-vu.md),
[карточка класса](../../reference/classes/sp-ps2-mesh-data.md).
