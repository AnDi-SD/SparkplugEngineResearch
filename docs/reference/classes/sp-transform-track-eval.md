# spTransformTrackEval

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spTransformTrackEval](../../../Sparkplug/Code/Sparkplug/spTransformTrackEval.h).

## Идентичность

- class ID `0x5DAF152D`, engine RTTI base `spTransformEval` (`0x87B0E260`),
  далее `spEvaluator` (`0xE91D088D`);
- registration `0x00768E90`, initializer `0x006D79C0`;
- factory `0x005FF090`, constructor `0x005FF000`, allocation `0x78`;
- vtable `0x00711404`: **девять**, не семь слотов. После неё по
  `0x00711428` начинается строка `spTransformTrackEval`;
- `+0x1C -> 0x004D6550` — no-op `ret 8`, роль пока неизвестна;
- `+0x20 -> 0x005FEBB0` — PRS evaluator, `ret 0x1C`;
- clone `0x005FF0F0` использует no-payload copy `0x0040ECE0`: новая копия
  не наследует входы и binding slot.

Фабричный/constructor control flow частично защищён; allocation и записи
полей установлены по доступным инструкциям и согласованным consumers.
Названия класса точные; пути новых `.h/.cpp` и имена методов — inferred/analytical.

## PC layout

| Offset | Поле / analytical role |
| ---: | --- |
| `+0x00` | `spBaseObject` prefix `0x10` |
| `+0x10` | transform slot, исходно `-1` |
| `+0x14` | input count |
| `+0x18` | два input по `0x30` |

Внутри каждого input:

| Offset | Подтверждённая роль |
| ---: | --- |
| `+0x00` | borrowed pointer на playback entry `spActor`, stride `0x60` |
| `+0x04` | pointer на animation track entry, stride `0x44` |
| `+0x08` | unsigned priority; default `0xFFFFFFFF` |
| `+0x0C` | три in/out position key indices |
| `+0x18` | три in/out rotation key indices |
| `+0x24` | три in/out scale key indices |

## Вычисление и binding

`0x005FEBB0` принимает `float time`, три output-адреса PRS и три byte-validity
output-адреса. Тип первого аргумента теперь подтверждён controller call sites,
но именно этот evaluator его **не использует**: время берётся из
`input.playback +0x34`, вес — из `+0x0C`. Sampling выполняет `0x00479290`;
он изменяет сохранённые в input key indices.

Нет samples: все validity flags очищаются, caller output storage не меняется.
Один ненулевой track: копируются PRS и флаги. Два: position/scale смешиваются
с cumulative weights; для каждого канала первое valid значение копируется.
Rotation смешивается через `0x004648C0`, только когда доля прежнего веса
положительна. Null track пропускается, сохраняя соответствие sample его
исходному input. Clamp весов/перехода не найден.

`0x005FE9C0` вставляет `(playback, track, priority, exclusive)`:

`0x005FEB70(index)` очищает только два указателя input. Count, priority,
caches и playback counter не меняются.

Actor binder tail `0x005A1C16..0x005A1E21` соединяет playback entries и tracks,
вызывает insert по `0x005A1D26`; priority берётся из playback `+0x50`,
exclusive соответствует `playback +0x10 == 0`. Entry `0x005A1C10` защищён.

[Owned actor runtime](../../engine/animation/actor-owned-runtime.md) теперь действительно
соединяет discovery/Start/Rebind/Stop, SAN leases, prepared keys, evaluator и node:
5154 сквозных comparisons проходят. Это закрывает отсутствие portable actor
binding, но не внешний frame, full loader или upstream third-input invariant.
