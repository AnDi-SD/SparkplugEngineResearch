# PC SAN: payload → descriptors → preparation → sampling

Checkpoint 2026-09-05, после [node world](native-pc-node-world.md).
Pristine PC SHA-256:
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
PS2 deferred. Игра и приложения не запускались/не изменялись.

## Типы записей

Track — embedded `0x44` **`spAnimTrack`** внутри `spAnimation`; оригинальное имя
подтверждено следующим [lifecycle checkpoint](native-pc-animation-lifecycle.md).
PRS descriptor pointers находятся в тройках
`+0x18/+0x24/+0x30`. Descriptor: count `+0`, representation `+4`, times `+8`,
values `+0xC`, размер `0x10`. Ниже названия режимов — analytical aliases.

| Representation | Position/scale | Rotation | Values stride на ключ |
|---:|---|---|---|
| 1 | packed Vector3 linear | packed quaternion Slerp | 12 / 16 bytes |
| 2 | packed cubic Vector3 | quaternion + squad control | 60 / 32 bytes |
| 3 | три scalar linear descriptors | три scalar angles | 4 bytes |
| 4 | три scalar cubic descriptors | три scalar cubic angles | 20 bytes |

Scalar descriptors имеют независимые times/counts/caches; первый mode `>=3`
включает три axes. Numeric modes отдельных scalar axes могут быть `3/4`.
Mode `0` payload — один нулевой uint, канал отсутствует. Packed mode `1`
с count `0` — иной, реально встреченный случай: descriptor существует,
но sampler не выставляет validity.

## Reader и общие массивы

Protected reader `0x0043DB90` разрешается через `0x013B11B4` в `0x00450D51`:
initial axis count `1`, затем jump `0x0043DBA3`. Signature:
`this=serializer working state`, arguments stream, track, role, animation;
`ret 0x10`. Для первого scalar descriptor axis count становится `3`.

Для каждого descriptor читаются uint representation, uint count, `count`
float times и `count*stride` raw values. Прямое stream read — vslot `+0x30`;
metadata uint helper `0x00416DA0` использует тот же byte-source.

Times берутся с offset `serializer+0x44` из единого `animation+0x50` buffer.
Values используют один из шести массивов `animation+0x38..+0x4C`, а текущие
cursors находятся в `serializer+0x2C..+0x40`.

| SAN field | Animation storage | Назначение capacity/count hint | Stride |
|---:|---:|---|---:|
| 6 | `+0x38` | scalar linear values | 4 |
| 7 | `+0x3C` | scalar cubic values | 20 |
| 8 | `+0x40` | Vector3 linear values | 12 |
| 9 | `+0x44` | Vector3 cubic values | 60 |
| 10 | `+0x48` | quaternion linear values | 16 |
| 11 | `+0x4C` | quaternion cubic values | 32 |
| 12 | `+0x50` | все float times | 4 |
| 64 | track array reservation | необязательный reserve hint, не terminator | track `0x44` |

Main reader `0x0043ECC0` выделяет эти массивы по hints. Writer
`0x0043DDC0` подтверждает ту же таблицу и суммирует фактические counts.
Нельзя объявлять field `64` обязательным: pristine `barrel.san` его не имеет.
Shared-array capacities должны трактоваться отдельно от числа завершённых tracks.

`0x00479830` получает role, count, times, values, representation, axis,
ownership bool. Track `+0x40` указывает на owner animation; descriptors
выделяются из его pool по `+0x58` (`0x00417510`), возвращаются в него через
`0x004175D0`. Reader передаёт ownership `false`, записываемый в track `+0x3C`:
сами time/value buffers принадлежат animation, не каждому descriptor.
Следующий [lifecycle checkpoint](native-pc-animation-lifecycle.md) установил,
что replacement использует новый, а не старый ownership flag; original release
оставляет stale descriptor pointers. Это не безопасный универсальный Reset.

## Подготовка после чтения

Raw cubic payload **содержит место для coefficients**, но attach пересчитывает
их в общем массиве. Поэтому точное wire payload не равно готовому runtime
значению всех его floats.

Scalar/vector cubic record хранит пять scalar/Vector3 блоков:
value, incoming tangent, outgoing tangent, quadratic coefficient, cubic coefficient.
Для сегмента `i → i+1`, `delta = value[i+1]-value[i]`:

```
c2[i] = 3*delta - (2*outgoing[i] + incoming[i+1])
c3[i] = outgoing[i] + incoming[i+1] - 2*delta
sample = value[i] + u*(outgoing[i] + u*(c2[i] + u*c3[i]))
```

Scalar helper `0x00493160`, vector `0x00493290`. Последняя запись не
пересчитывается. Times здесь не читаются: `u` — уже нормализованная доля
интервала. Incoming tangent используется при preparation, но не при sample
того же сегмента — это не «неизвестный/unused float».

Quaternion helper `0x004933C0` разрешён в `.rld` body `0x013C1220`.
Для одного ключа control копирует quaternion; protected resolver также
инициализирует referenced constant `0x013B354C` значением `1`. Нулевые исходные
file bytes этой константы нельзя принимать за runtime значение до resolver-а.
Для нескольких ключей helper `0x00464DE0` вычисляет:

```
control[i] = q[i] * exp(-0.25 * (
    log(conjugate(q[i]) * q[previous]) +
    log(conjugate(q[i]) * q[next])))
```

На краях отсутствующий сосед повторяет крайний quaternion. Log/exp
`0x00464C10/0x00464B90` используют threshold `0.001`, не добавляют
normalization/clamp. Squad `0x00464B20` смешивает Slerp endpoints и Slerp
controls с коэффициентом `2*u*(1-u)`.

## Sampling и границы

`0x00479290` использует state time и три persistent index-cache arrays.
Scalar rotations — углы в радианах: последовательно строятся axis-angle X,
Y, Z и слева умножаются на накопленный quaternion (`qZ*qY*qX`).
Packed linear quaternion helper `0x004790D0` разрешается в `0x00425CB0` и
использует уже исследованный native Slerp.

Interval `0x00478F90` двигается от cached index в обе стороны. При
`time >= lastTime` и **ровно двух** keys возвращает index `0`, fraction `0`;
при `count >=3` возвращает предпоследний index, fraction `1`. Эта особенность
сохранена, а не заменена обычным clamp. Нельзя считать её доказанным видимым
багом игры без проверки actor time/wrap и конкретного track.

Native packed quaternion/scale и scalar rotation/scale читают next record
без отдельного one-key guard. При finite соседних данных fraction `0`
сохраняет первый ключ. Portable adapter использует bounded repeated endpoint,
а не воспроизводит out-of-bounds read. Negative cache, incomplete scalar
triples, nonfinite/unsorted inputs и unsafe empty cubic preparation отклоняются
или безопасно сбрасываются **явными host guards**, не объявляются native rules.

## Реальные ресурсы: только read-only cross-check

`inspect_pc_san_keys.py` ограничен 12 файлами / 2 MiB на файл; это узкий
single-object PC FFPS `0x26` inspector, не общий loader. Четыре pristine файла:

- `barrel.san`: 20 tracks, 700 vector + 700 quaternion linear keys; reserve
  field `64` отсутствует; scale descriptors mode 1/count 0.
- `bbush.san`: 5 tracks; **один cubic scale channel с тремя ключами**.
- `bflower.san`: 17 tracks, включая 17 single-position и 5 single-quaternion
  channels; два field `5` оставлены как ещё не разобранные tags.
- `bw.san`: 20 tracks, reserve 20, linear PRS и empty scale.

Для всех четырёх семь pool hints точно совпали с суммой разобранных counts.
Existing `SmoAnimationDecoder.ReadVectorCurve` принимает только длину
`8+count*16`; payload cubic scale из `bbush.san` ему не соответствует и
возвращается пустой curve. Это подтверждённое ограничение текущего viewer,
не исправление приложения в данном research-цикле.

## Реконструкция и воспроизводимость

`Analysis/PC/spAnimationKeySampling.h` — явно analytical immutable value
adapter, **не выдуманный original class или полный `spAnimation` source**.
Он готовит четыре вида записей, применяет exact finite-input sampling и
подключается к `spTransformTrackEval` через существующий sampler interface.
Есть проверка prepared keys → evaluator → controller → node world.

```powershell
python research/inspect_animation_keys.py
python research/probe_pc_animation_keys.py
python research/compare_pc_animation_keys.py --portable .codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugAnimationKeyTests.exe
python research/inspect_pc_san_keys.py
```

- body/SHA checks: 12/12;
- original guest instructions: 396/396 (runtime representations, caches,
  coefficient setup, original payload-reader→attach→sampler chain);
- C++/PC comparison: 120/120, 40 datasets × PRS/cache/validity;
- portable tests: 60/60.

Seams: источник байтов, descriptor-pool allocation и CRT `acos` (оба ABI
входа). Engine sampler, Slerp, quaternion multiply/log/exp/control и cubic
preparation исполняются из оригинальных bytes. Нет OS API forwarding, игры
или Direct3D; лимиты из [emulation harness](native-pc-node-world.md) сохранены.

Не закрыты: общий data-block/object-directory transaction и rollback, tag original
class name, frame time/wrap caller, original method names. Constructor/factory,
track identity, normal ownership и stable tag order закрыты следующим
[object lifecycle checkpoint](native-pc-animation-lifecycle.md).
