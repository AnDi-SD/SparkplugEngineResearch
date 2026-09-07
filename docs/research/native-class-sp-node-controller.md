# `spNodeController`: PC применение анимации к узлу

Статус: `substantial`, PC, 2026-09-05. Это восстановленный локальный PRS-срез,
не законченный scheduler или world-transform pipeline.

## Идентичность и layout

Class ID `0x14A9784E`; engine RTTI base `spSubController` (`0x062C22ED`).
Registration `0x00768EF0`, initializer `0x006D79F0`; factory `0x005FF550`
выделяет ровно `0x18`, constructor `0x005FF400`, destructor `0x005FF4D0`.

| Offset | Роль |
|---:|---|
| `+0x00` | base prefix `0x10` |
| `+0x10` | intrusive-owned `spNode*` |
| `+0x14` | непосредственно удаляемый `spTransformEval*` |

Constructor kind `0` создаёт `spTransformTrackEval` (`0x78`, `0x005FF000`),
kind `1` — `spTransformConstEval` (`0x68`, `0x00601C20`). Остальные kind не
исследуются как безопасный API; defaults const evaluator ещё открыты.
Node attach `0x005A15C0` выполняет intrusive ownership. Destructor освобождает
node и напрямую удаляет evaluator. Native clone `0x005FF5F0` использует
factory + no-payload copy: результат — пустой node и свежий track evaluator.

Vtable `0x00711444`, 12 slots; следующая строка `spNodeController` по
`0x00711474`. После семи base slots:

| Byte slot | PC body | Роль |
|---:|---:|---|
| `+0x1C` | `0x005FF1B0` | direct apply(time) |
| `+0x20` | `0x005FF250` | transition apply(time, factor) |
| `+0x24`, `+0x28` | `0x005FF4C0` | evaluator getter |
| `+0x2C` | `0x005FF3E0` | node-name getter |

Это analytical signatures, не восстановленные spelling оригинальных методов.

## Поведение

Обе apply-ветви вызывают evaluator vslot `+0x20` с time, тремя PRS outputs и
тремя validity flags. Direct branch:

1. valid position копируется в node `+0x20`, flags `+0xB0 |= 1`;
2. valid rotation проходит через `0x00420640`;
3. valid scale копируется в node `+0x30`, flags `+0xB0 |= 1`.

Rotation setter переводит quaternion в local matrix `+0x40` через
`0x004647F0`, затем через `0x013B13CC`. Продолжение разрешило этот переход
в `0x004023A9`: `flags |= 1`, затем обычный return. Portable direct/blend
исправлены для rotation-only каналов; неизвестных scene effects в этом setter
больше не предполагается. [Проверка](native-pc-node-world.md).

Transition branch сравнивает position и quaternion покомпонентно с epsilon
`0.001`. При отличии position получает `(1-factor)*old + factor*sample`;
rotation переводит старую matrix в quaternion (`0x00464CB0`) и смешивает через
`0x004648C0`. Scale **копируется напрямую**, а не интерполируется. На factor
не найден clamp `0..1`. Invalid channels сохраняют старое состояние.

## Реконструкция

`Sparkplug/Code/Sparkplug/spNodeController.*` повторяет эти локальные операции,
RTTI и blank clone. `shared_ptr` заменяет intrusive node ownership;
`unique_ptr` сохраняет прямое владение evaluator. Null guards — явно host-only.
Пути файлов inferred. `spSubController`, `spEvaluator`, `spTransformEval`
добавлены как минимальные dependency interfaces, не как полностью закрытые классы.

Native instruction replay проверяет реальную direct/blend арифметику и flags;
в старом replay rotation setter остаётся test seam; новый guest replay проверяет
его полностью с protected entry. Quaternion math сопоставляется с оригинальными
телами. Подробности: [общая проверка](native-pc-animation-runtime.md).

Открыты: const evaluator, original names/TU и полная frame integration.
Local-to-world propagation уже восстановлен отдельным node slice. PS2 deferred.
