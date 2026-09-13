# PC SAN: payload → descriptors → preparation → sampling

## Типы записей

| Representation | Position/scale | Rotation | Values stride на ключ |
| ---: | --- | --- | --- |
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
| ---: | ---: | --- | ---: |
| 6 | `+0x38` | scalar linear values | 4 |
| 7 | `+0x3C` | scalar cubic values | 20 |
| 8 | `+0x40` | Vector3 linear values | 12 |
| 9 | `+0x44` | Vector3 cubic values | 60 |
| 10 | `+0x48` | quaternion linear values | 16 |
| 11 | `+0x4C` | quaternion cubic values | 32 |
| 12 | `+0x50` | все float times | 4 |
| 64 | track array reservation | необязательный reserve hint, не terminator | track `0x44` |

Main reader `0x0043ECC0` выделяет эти массивы по hints. Writer `0x0043DDC0` подтверждает ту же таблицу и суммирует фактические counts. Shared-array capacities должны трактоваться отдельно от числа завершённых tracks.

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

Native packed quaternion/scale и scalar rotation/scale читают next record
без отдельного one-key guard. При finite соседних данных fraction `0`
сохраняет первый ключ. Portable adapter использует bounded repeated endpoint,
а не воспроизводит out-of-bounds read. Negative cache, incomplete scalar
triples, nonfinite/unsorted inputs и unsafe empty cubic preparation отклоняются
или безопасно сбрасываются **явными host guards**, не объявляются native rules.
