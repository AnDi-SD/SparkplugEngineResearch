# PC SAN: запись анимации и вложенные блоки

Checkpoint 6 сентября 2026, продолжение [PC loader](native-pc-smo-san-loader.md).
Это field writer, **не готовый FFPS/FAT save, exporter или lossless editor**.
PS2 в этом checkpoint не исполнялся и новых процентов не получает.

## Источники и безопасная граница

PC `local-data/pc-pristine/WinxClub.exe`, SHA-256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Папка исторически называется pristine, но исследуемый hash — ранее отмеченный
resolution-research variant, а не доказательство исходности дистрибутива.

Оригинальные TU:

- `Z:\Sparkplug\Code\Sparkplug\spAnimationSerializer.cpp`;
- `Z:\Sparkplug\Code\Sparkplug\spDataBlockSerializer.cpp`.

`research/inspect_pc_serializer_writers.py`: 8 fixed hash checks.
Native инструкции исполняются только в гостевой памяти: 100k инструкций / 2 s
на вызов, 30 s на child, 64 KiB arena, 32 KiB allocation limit. Stream I/O,
allocator и diagnostic output — явно обозначенные внешние fixtures. Нет
игры, Windows API forwarding, GPU, записи оригинальных ресурсов или роста caps.
Writer использует independently completed field reader; это **не возобновление**
capped whole-load `bflower/barrel/bw` из предыдущего checkpoint.

## Полный original SAN field writer

`43DFE0(this=serializer+10, stream, animation)` вызывает общий block writer;
`43DDC0` пишет runtime key descriptors. Native object layout `84`, serializer
layout `4C` и secondary vtable `6E0B00` сохранены без PS2-проекций.

Порядок полей:

1. `0`: duration float; `64`: **logical track count**, не capacity.
2. `12`, затем `6..11`: нулевые counters, с последующим patch-back.
3. Для каждого track безусловно `2` position, `3` rotation, `4` scale,
   затем `1` length-prefixed name.
4. Tags `5` в runtime order, каждый name + time.
5. Patch-back фактических pool counters, возврат в конец и zero terminator.

PRS fields резервируют `sizeCode7/u32`, даже если payload имеет 8 bytes;
tag field5 использует `sizeCode6/u16` (`43E440 push6`).
Representations 1/2 используют одну axis descriptor, 3/4 — три. Каждый
descriptor: representation, count, times и runtime prepared values. Pool IDs:
scalar linear0/cubic1, vector linear2/cubic3, quaternion linear4/cubic5;
times — отдельный counter6. Нулевой count packed-linear descriptor допустим.
Missing descriptor pointer writer разыменовывает: это не поддержка отсутствия
канала; portable writer пока отвергает такой вход до записи.

| Вход | Bytes fields | Native writer instructions | Результат |
|---|---:|---:|---|
| empty animation | 47 | 5577 | точный эталон; reader затем reserve1 |
| `bbush.san` | 2492 | 18171 | побайтно как исходник |
| `bflower.san` | 9102 | 40213 | побайтно как исходник |
| `barrel.san` | 26262 | 44394 | только добавлены 6 bytes field64 с count |
| `bw.san` | 26377 | 45323 | побайтно как исходник |

`research/probe_pc_san_writer.py`: 64 directed checks для пяти success cases.
Output hashes печатаются тестом; `barrel` отличие проверено полным сравнением
после удаления ровно bytes5..10, а не только сравнением counts/размеров.
Для `bbush` original writer -> original reader дополнительно сверены 227
captured runtime values (identity, PRS, tags). Parser-derived usedPools исключён.
Для остальных трёх native reread в той же arena не заявляется.
Во всех success cases источник84 bytes не изменён, tracked allocations
reader/writer/descriptor освобождены, fixture name references сбалансированы.

## Ошибки записи

Три проверенных stream-write failure: empty call1, empty call9, bbush call100 —
17 directed checks. Native возвращает false, оставляет partial destination
(0 / 17 / 1592 bytes соответственно), source object не меняется, writer-owned
headers очищаются. Это не transaction rollback.

bbush failure на write-call30 вошёл в `spErrorManager`, затем в `4169A0`
и внешний formatting IAT `6D9328`, получив unmapped fetch `0033D03C`.
Это **граница harness**, не доказанный баг анимации. Профиль запрещает слепой
повтор этой комбинации; внешняя ошибка не подменяется success. Статически
`43E506..43E5B5` показывает error-manager submission и последующий cleanup/
false, но complete execution этого branch пока не подтверждено.

## Общий PC nested block writer

`473000`: объект28, list/head/count в первых0C, current header0C..1B,
object1C, stream20, **один общий reserved size code24**.
`472710` только сохраняет object/stream и возвращает true, ничего не пишет.
`472D30 -> 44EB66 -> 472D5D` append-ит header/node, устанавливает ID/sizeFFFF,
запоминает width, tell/start, пишет all-one placeholder и tell/dataStart.
`472E20` берёт top header, **игнорирует переданный field ID**, вычисляет payload
size, seek/start, patch в reserved width, seek/end, pop. Failures не pop-ают
header и не восстанавливают позицию после неуспешной записи patch.

163 directed checks: 119 success, 12 mixed-width, 18 empty/ID31, 6 failed begin,
8 failed end. 18 success outputs сравнены с compiled source: 1658 exact bytes.
Standalone fixture сама освобождает оставшиеся nodes через allocator boundary;
это не выдаётся за исполненный standalone destructor. Whole SAN writer
сам исполняет свой inlined stack cleanup.

Подтверждённые особенности оригинала:

- Mixed nested widths повреждают внешний header: outer42/u32, inner3/u8,
  payload A/bc/D дают `bf2a06ffffff41a302626344`; три stale FF остаются.
- Zero payload: `WriteHeader4727B0 -> 4F5AD0` возвращает success без записи,
  так что End оставляет all-one placeholder. SelectSizeCode(0)=5 сам по себе
  не доказывал, что native реально записывает корректное пустое поле.
- ID31 считается inline writer-ом, а reader принимает31 за escape: пример
  `ff03000000616263` не содержит обязательного extended-ID byte.
- Слишком узкая reservation логирует ошибку, но затем продолжает patch.
  Полная error-manager ветвь overflow не исполнялась.

Portable nested API явно отклоняет mixed nesting, ID31, fixed reservation,
empty/overflow End и unsafe unbound/unfinished usage. Прямой portable helper
может корректно кодировать empty/ID31 как **host wire policy**, не утверждая
воспроизведение этих native bugs. Проверки этих различий отделены от
native/portable byte-equality tests.

## Восстановленный код и ещё неизвестное

`spDataBlockSerializer.*` теперь содержит nested writer; `spAnimationSerializer.*`
— canonical field writer на том же core, с preflight и explicit partial-I/O
semantics. `spAnimTrack::GetKeysForAnalysis` открывает только immutable snapshot.
Ни original API spelling этого accessor, ни lost header names не придуманы.

Compiled writer на четырёх файлах сравнивается через
`research/compare_pc_san_writer.py` / `SparkplugSanReaderTests --rewrite-fields`:
64 233 output bytes, включая native canonical addition field64 для barrel.
Portable сторона также повторно читает каждый свой output; это по-прежнему
fields, не весь FFPS/FAT transaction. Native reread witness отдельно указан выше.

Побайтная проверка обнаружила две ошибки первого portable черновика: UInt32
вместо native UInt16 для tag header и исчезнувший `-2^-24` cubic scale
коэффициент bbush. Обе исправлены по инструкциям, не допуском сравнения.
`493160` scalar и `493290` vector сохраняют промежуточные значения x87 в float
в разных местах; даже X/Y/Z vector имеют разные spill schedules.
`CubicCoefficientsForAnalysis` отражает эти stores. Новая проверка
`research/compare_pc_cubic_preparation.py`: 96 fixtures / 4160 bit-exact float
values для dimensions1/3 и count1/2/3/5/6/9. Это не универсальная гарантия
для всех exceptional float/FPU modes; finite host safety остаётся явной.

Открыты: whole FFPS/FAT save и indexing references, missing descriptors/rep0
producer conventions, malformed/filled-target/allocation paths, оставшиеся
seek/tell/error-manager branches, opaque unknown-field preservation, полный
корпус вариантов, software/native scene/backend integration. Canonical writer
не сохраняет unknown fields, которые reader пропустил; это отмечено в API.
Наличие source и passing tests не означает 100% всей области SMO/SAN.
