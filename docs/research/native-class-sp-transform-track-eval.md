# `spTransformTrackEval`: PC PRS evaluator

Статус: `substantial`, PC-first. Дополнено часовым циклом 2026-09-05.
Подтверждены layout, вычисление PRS, binding inputs и непосредственный caller;
полный actor scheduler и SAN loader ещё не восстановлены. Key sampling и
world-update закрыты последующими checkpoints, перечисленными ниже.
PS2 не исследовался в этом цикле.

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
|---:|---|
| `+0x00` | `spBaseObject` prefix `0x10` |
| `+0x10` | transform slot, исходно `-1` |
| `+0x14` | input count |
| `+0x18` | два input по `0x30` |

Внутри каждого input:

| Offset | Подтверждённая роль |
|---:|---|
| `+0x00` | borrowed pointer на playback entry `spActor`, stride `0x60` |
| `+0x04` | pointer на animation track entry, stride `0x44` |
| `+0x08` | unsigned priority; default `0xFFFFFFFF` |
| `+0x0C` | три in/out position key indices |
| `+0x18` | три in/out rotation key indices |
| `+0x24` | три in/out scale key indices |

Layout закреплён в `Analysis/PC/SparkplugAbi.h` через size/offset assertions.
Original member names и ownership track storage не объявляются найденными.

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

- exclusive отвергает приоритет ниже любого активного; иначе оставляет один input;
- неэксклюзивная ветвь сортирует два input по возрастанию unsigned priority,
  сохраняя caches удержанных записей;
- обновляется playback `+0x48`, но это пока не названо полноценным refcount:
  замена первого exclusive input не показывает симметричного decrement;
- локального ограничения capacity до двух не найдено. Проверки третьего
  входа намеренно не запускались; caller invariant остаётся открытым.

`0x005FEB70(index)` очищает только два указателя input. Count, priority,
caches и playback counter не меняются.

Actor binder tail `0x005A1C16..0x005A1E21` соединяет playback entries и tracks,
вызывает insert по `0x005A1D26`; priority берётся из playback `+0x50`,
exclusive соответствует `playback +0x10 == 0`. Entry `0x005A1C10` защищён.

Непосредственный scheduler и применение к узлу теперь разобраны в
[`spActor`](native-class-sp-actor.md) и
[`spNodeController`](native-class-sp-node-controller.md). Внешний frame caller
actor tick подтверждён как `spAnimationManager::4535A0`; старый игровой `startLevel=2` probe проверял binding,
но не входил в evaluator. Это ограничение старого эксперимента сохраняется.

## Исходники, проверки и ограничения

`Sparkplug/Code/Sparkplug/spTransformTrackEval.*` восстанавливает вычислительную
часть и [original insert/clear lifecycle](native-pc-actor-binding.md). Sampling подключается через явный `TrackSamplerForAnalysis`; теперь
для него есть проверенный [prepared SAN key adapter](native-pc-animation-keys.md).
Входы безопасно ограничены двумя. Это не полный SAN loader
и не полная реконструкция upstream actor capacity contract. Host safely initializes
invalid-channel storage, где оригинал мог копировать неопределённые байты.

`inspect_transform_track_eval.py` теперь проходит 15/15 проверок, включая
полную vtable и её границу. Общие evidence/replay проверки и точные
математические особенности описаны в
[PC animation runtime pipeline](native-pc-animation-runtime.md).

Открыты: оригинальные API/source names, полная ownership/capacity политика
input insertion, original API/source names и outer engine frame.
Representations `1..4` и dirty/world transforms закрыты следующими checkpoints;
полная загрузка, ownership и frame integration остаются открытыми.

Ночной checkpoint 5/6 сентября: native factories/discovery/binder/start исполнены;
input insert/clear перенесены, **4368 comparisons /156 cases** проходят.
Exclusive counter asymmetry подтверждена, новая запись наследует cache
физического места назначения. Третий input не запускался: Start остановлен
перед binder CALL. [Подробности и точные ограничения](native-pc-actor-binding.md).

[Owned actor runtime](native-pc-actor-owned-runtime.md) теперь действительно
соединяет discovery/Start/Rebind/Stop, SAN leases, prepared keys, evaluator и node:
5154 сквозных comparisons проходят. Это закрывает отсутствие portable actor
binding, но не внешний frame, full loader или upstream third-input invariant.
