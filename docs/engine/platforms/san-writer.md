# PC SAN: запись анимации и вложенные блоки

## Границы описания

Оригинальные TU:

- `Z:\Sparkplug\Code\Sparkplug\spAnimationSerializer.cpp`;
- `Z:\Sparkplug\Code\Sparkplug\spDataBlockSerializer.cpp`.

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
| --- | ---: | ---: | --- |
| empty animation | 47 | 5577 | точный эталон; reader затем reserve1 |
| `bbush.san` | 2492 | 18171 | побайтно как исходник |
| `bflower.san` | 9102 | 40213 | побайтно как исходник |
| `barrel.san` | 26262 | 44394 | только добавлены 6 bytes field64 с count |
| `bw.san` | 26377 | 45323 | побайтно как исходник |

## Общий PC nested block writer

`473000`: объект28, list/head/count в первых0C, current header0C..1B,
object1C, stream20, **один общий reserved size code24**.
`472710` только сохраняет object/stream и возвращает true, ничего не пишет.
`472D30 -> 44EB66 -> 472D5D` append-ит header/node, устанавливает ID/sizeFFFF,
запоминает width, tell/start, пишет all-one placeholder и tell/dataStart.
`472E20` берёт top header, **игнорирует переданный field ID**, вычисляет payload
size, seek/start, patch в reserved width, seek/end, pop. Failures не pop-ают
header и не восстанавливают позицию после неуспешной записи patch.

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

## Восстановленный код и ещё неизвестное

`spDataBlockSerializer.*` теперь содержит nested writer; `spAnimationSerializer.*`
— canonical field writer на том же core, с preflight и explicit partial-I/O
semantics. `spAnimTrack::GetKeysForAnalysis` открывает только immutable snapshot.
Ни original API spelling этого accessor, ни lost header names не придуманы.

Portable сторона также повторно читает каждый свой output; это по-прежнему fields, не весь FFPS/FAT transaction. Native reread witness отдельно указан выше.
