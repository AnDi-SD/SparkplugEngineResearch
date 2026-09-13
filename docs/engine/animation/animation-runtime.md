# PC animation runtime: SAN → node → skin palette

## Подтверждённая последовательность и оставшийся разрыв

```text
SAN reader → spAnimation tracks [0x44]
                     ↓ actor binder 0x005A1C10 (protected entry, visible tail)
spActor playback [0x60] → spTransformTrackEval inputs [2 × 0x30]
          ↓ tick 0x005A2380
spNodeController apply/blend → evaluator 0x005FEBB0
          ↑                         ↓ sampler 0x00479290
          └──────── local PRS ───────┘
          ↓ node position/scale and quaternion→matrix setter
          ↓ world-update slot +0x30 (visible tail; protected entry still open)
spNode cached world PRS → spSkin 0x0046A240
          ↓ inverseBind × boneWorld
renderer palette → existing mesh submission
```

Track `+0x18/+0x24/+0x30` содержит по три descriptor pointer для position,
rotation, scale. Descriptor prefix: count `+0x00`, representation `+0x04`,
times pointer `+0x08`, values pointer `+0x0C`. Это memory layout, не SAN wire.

На синтетических finite-input треках реальное PC тело дало:

| Keys / time | Native результат |
| --- | --- |
| 2 keys, times `[0,10]`, time `5` | середина между Vector3 keys |
| 2 keys, time `10` | первый key: interval index `0`, fraction `0` |
| 3 keys, times `[0,10,20]`, time `20` | последний key |

Отдельный interval test также подтвердил fraction `0` после последнего из
двух ключей. Это **не утверждение о видимом баге игры**: неизвестны upstream
padding/duplicate-key invariants и то, какие времена реально достигают sampler.
В экспортёр/импортёр никакая «коррекция» этого поведения не добавлялась.

## Quaternion convention

`0x004647F0` строит row-indexed matrix из XYZW:

```text
[ 1-yy-zz, xy+wz,   xz-wy   ]
[ xy-wz,   1-xx-zz, yz+wx   ]    xx=2*x*x; xy=2*x*y; wx=2*w*x; ...
[ xz+wy,   yz-wx,   1-xx-yy ]
```

`0x00464CB0` выполняет обратное преобразование: positive trace либо ветвь
наибольшей диагонали; axis cycle по `0x00740350` равен `{1,2,0}`.
`0x004648C0` выбирает кратчайшую quaternion hemisphere, ограничивает dot
сверху единицей, применяет slerp. При `sin(theta) < 0.001` копирует первый
quaternion, **не nlerp**. Factor не clamp-ится и результат не нормализуется.
Portable math воспроизводит finite-input поведение, не побитовую x87 арифметику.

## World cache и palette

Vector helper `0x00420350` выполняет row-vector×matrix; `0x0040F1E0` копирует
девять floats. Matrix3 product `0x00420C00` не содержит внешних calls.
Отрицательная разведка: `0x00420530/0x004212F0` — debug tree dump,
`0x00420E60` — bounds aggregation, не world updater. `0x00420B20`,
`0x00420BD0`, `0x00420D20` начинаются protected thunks; случайный линейный
disassembly их недостижимых байтов не считается исходным кодом.

Skin получает palette pointer из renderer `+0xC9B8`, передаёт cached PRS в
builder `0x00461D70` и вызывает matrix product `0x00426B00` с
`this=inverseBind`, аргументом `boneWorld`. Подтверждён точный порядок:

`palette[4*r+c] = sum(inverseBind[4*r+k] * boneWorld[4*k+c])`.

Исправление прежнего описания `spSkin`: renderer count `+0xC9BC` очищается
по успешной ветви `0x0046A38B`, но не по всем error exits (`0x0046A382`).
