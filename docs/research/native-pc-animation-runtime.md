# PC animation runtime: SAN → node → skin palette

Evidence checkpoint: 2026-09-05, часовой цикл с 14:04 МСК.
PC pristine `WinxClub.exe`, SHA-256:
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Все адреса — VA этого binary, не универсальные offsets. PS2 deferred.

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

Непосредственный actor/controller caller подтверждён статически. Внешний
frame caller actor tick и игровой проход через всю схему пока не проверены.
Карточки: [evaluator](native-class-sp-transform-track-eval.md),
[node controller](native-class-sp-node-controller.md), [actor](native-class-sp-actor.md).

## Key sampling: что действительно проверено

Track `+0x18/+0x24/+0x30` содержит по три descriptor pointer для position,
rotation, scale. Descriptor prefix: count `+0x00`, representation `+0x04`,
times pointer `+0x08`, values pointer `+0x0C`. Это memory layout, не SAN wire.

`0x00479290` вызывает interval helper `0x00478F90` с times, count, time,
fraction output и предыдущим index; обновлённый index сохраняется в cache.
Предыдущий index за верхней границей сбрасывается в ноль; поиск поддерживает
движение времени вперёд и назад. Empty/negative/NaN случаи пока не проверялись.

Representation `1` position использует packed Vector3 keys и linear helper
`0x00479060`; representation `2` вызывает cubic helper `0x00479110` с шагом
`0x3C`. В scalar representations `3/4` имеются соответственно линейная и
полиномиальная ветви. Полный контракт всех rotation/scalar/cubic вариантов,
подготовки times и ownership ещё не восстановлен и не перенесён в исходники.

На синтетических finite-input треках реальное PC тело дало:

| Keys / time | Native результат |
|---|---|
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

Независимые node consumers `0x00420660/0x00420710` подтверждают cached world
position `+0x74`, scale `+0x80`, matrix `+0x8C`. Forward point сначала
масштабирует компоненты, затем умножает row vector на matrix, затем добавляет
position. Inverse вычитает position, умножает на transpose и делит на scale.
Проверен обратимый orthonormal случай; нулевой scale не тестировался.

В конце цикла найден сам world-update: node vslot byte offset `+0x30` ведёт
в `0x00421420`. Его preamble по `0x00421428` уходит через protected pointer
`0x013B1510`; видимый хвост `0x0042142E..0x00421633` разбирается непрерывно.
В следующем [node world-этапе](native-pc-node-world.md) protected pointer
разрешён bounded x86 emulation в `0x00442FA6` (`mov ebx,[esi+0xB0]`).
Прежнее ограничение «контракт только хвоста» для этого входа снято:

- `(node.flags | inheritedFlags) & 1` разрешает пересчёт cached PRS;
- без parent копируются local position/scale, а при небиллбордном пути — orientation;
- с parent маска `0x10000` включает пересчёт position через parent orientation
  и translation; `0x40000` дополнительно применяет parent scale к local position;
  при выключенной `0x10000` этот хвост position не перезаписывает;
- scale при `0x40000` — покомпонентный local×parent scale, иначе local scale;
- orientation при `0x20000` и небиллбордном пути — local×parent matrix
  (`0x00420C00`), иначе доступная ветвь копирует local matrix. Billboard
  orientation готовит `0x004207E0`; camera/flag-семантика проверена в продолжении;
- после пересчёта вызывается `0x004651E0` для collision entries;
- children получают тот же virtual `+0x30` с `(node.flags | inheritedFlags) & ~2`;
- в конце node flags очищаются маской `~7`. Точные роли bits `2/4` пока не названы.

Vector helper `0x00420350` выполняет row-vector×matrix; `0x0040F1E0` копирует
девять floats. Matrix3 product `0x00420C00` не содержит внешних calls.
Отрицательная разведка: `0x00420530/0x004212F0` — debug tree dump,
`0x00420E60` — bounds aggregation, не world updater. `0x00420B20`,
`0x00420BD0`, `0x00420D20` начинаются protected thunks; случайный линейный
disassembly их недостижимых байтов не считается исходным кодом.

В часовом этапе полный world updater ещё не проверялся. Продолжение добавило
179 guest-instruction checks и portable transform/tree/billboard slice.
Открыты collision implementation и внешний frame caller; подробности
и границы host-адаптации вынесены в [node world](native-pc-node-world.md).

Skin получает palette pointer из renderer `+0xC9B8`, передаёт cached PRS в
builder `0x00461D70` и вызывает matrix product `0x00426B00` с
`this=inverseBind`, аргументом `boneWorld`. Подтверждён точный порядок:

`palette[4*r+c] = sum(inverseBind[4*r+k] * boneWorld[4*k+c])`.

Builder entry разрешён в продолжении: `0x013B162C -> 0x004061E8` подготавливает
стек, затем tail `0x00461D76..0x00461E5F` собирает scale/orientation product,
translation в indices `12..14`, affine last column. Полный builder теперь
проверен на 32 fixtures. Matrix multiply и native copy `0x0041D330` также
проверены отдельно на некоммутирующем примере.

Исправление прежнего описания `spSkin`: renderer count `+0xC9BC` очищается
по успешной ветви `0x0046A38B`, но не по всем error exits (`0x0046A382`).

## Безопасная воспроизводимость

```powershell
python research\inspect_transform_track_eval.py
python research\inspect_animation_runtime.py
python research\probe_pc_animation.py
```

- Read-only inspectors: 15/15 и 46/46; SHA, body hashes, vtable boundaries,
  offsets, прямые и виртуальные call sites.
- Native replay: 48/48; 128
  quaternion→matrix, 128 обратных, 640 slerp и по 64 Matrix4/Matrix3 product
  сравнений внутри агрегированных checks.
- CMake/CTest: 3/3 suites, новый `SparkplugAnimationTests` — 32 assertions.
- Safety-gate unit tests: `python research\test_pc_animation_probe.py`, 5/5;
  mocks проверяют SHA rejection, compile failure и timeout control flow без
  открытия game EXE, запуска compiler или native instructions.

Replay runner проверяет SHA **до исполнения**, компилирует отдельный MSVC x86
процесс и ограничивает его запуск 10 секундами. Только перечисленные открытые
тела копируются в отдельные страницы, которые после relocation становятся RX.
Game EXE не загружается как module и не запускается; файлы игры не меняются.
Это ограниченная инструкция-уровневая проверка, **не OS sandbox и не тест в игре**.

Seams названы явно: synthetic track sampling для evaluator; synthetic evaluator
для controller; protected rotation setter; matrix→quaternion и slerp при проверке
controller, отдельно проверенные настоящими телами; CRT `acos` для slerp.
Дополнительно controller и evaluator проходят связанные проверки с настоящими
matrix→quaternion/slerp телами вместо этих двух math seams; protected setter
и источники samples по-прежнему заменены контролируемыми fixtures.
При проверке самого track sampler разрешена только linear-position ветвь;
неисследованные rotation/cubic calls ведут в немедленный аварийный выход probe.
Matrix product использует настоящий native copy. Actor tick, renderer и D3D
не запускаются. Третий input/плохие указатели/нулевой scale не тестируются.

Portable `spTransformTrackEval` и `spNodeController` находятся в исходном
module tree с inferred file paths. Base interfaces — минимальные dependency
slices, а не доказательство закрытия ещё трёх классов. Полный SAN decoder,
actor state machine и node-world updater на момент этого checkpoint оставались
в очереди. Последующие [node world](native-pc-node-world.md) и
[SAN keys](native-pc-animation-keys.md) закрыли protected setter/world builder,
representations `1..4` и reader-to-sampler вычислительную цепочку. Полный SAN
loader, actor state machine и frame integration по-прежнему открыты.
