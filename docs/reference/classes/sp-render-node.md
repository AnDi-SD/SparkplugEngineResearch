# spRenderNode

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spNode](../../../Sparkplug/Code/Sparkplug/spNode.h), [spRenderNode](../../../Sparkplug/Code/Sparkplug/spRenderNode.h).

## Объект и поведение

| Факт | PC | PS2 |
| --- | ---: | ---: |
| Class ID / direct base | `0x603625D0 / spNode` | same |

Точного исходного пути класса в строках executable нет. Известен только
`Z:\Sparkplug\Code\Sparkplug\spRenderNodeSerializer.cpp`, поэтому нынешние
`Code/Sparkplug/spRenderNode.*` явно помечены как inferred paths.

### Layout и владение renderables

PC factory `425520→13C5390` теперь исполнена и выделяет **0x1D4**. После
полного `spNode` (`0xB4`) находится secondary support:

| Offset | Роль |
| ---: | --- |
| `+0xB4` | secondary vptr `6DCADC`, шесть методов, original type name неизвестен |
| `+0xB8` | allocator/служебное слово compiler-specific vector |
| `+0xBC` | begin массива renderable-ссылок |
| `+0xC0` | end |
| `+0xC4` | capacity end |

Старый prefix type `spRenderNodeObservedPrefixLayout` оставлен для совместимости,
но больше не является пределом знания. Новый `spRenderNodeLayout` покрывает
local/world spheres `C8/D8`, matrix pointers `E8/EC`, light-cache `F0`, self124,
scene links128/12C, cull bypass130, dirty134, matrices138/178, inverse scale1B8
и callback vector1C4. Неназванные cache words/bytes и untouched padding не
получают вымышленных ролей. Exact fields/static asserts — `Analysis/PC/SparkplugAbi.h`.

PS2 factory чисто выделяет `0x1E0` байт с выравниванием 16. Там `spNode`
занимает `0xC0`, а renderable-контейнер принадлежит support-subobject по
`+0xC8`: count расположен по `+0xD0`, storage по `+0xD4`. Constructor также
создаёт два matrix/cache блока по `+0x140/+0x180`, пишет self-link `+0x134`,
inverse scale `(1,1,1)` по `+0x1C0` и инициализирует хвостовой callback state.
Exact структура сохранена в `Analysis/PS2/SparkplugAbi.h`; PC и PS2 layouts
намеренно не объединены.

Native список владеет renderables через intrusive references. PC copy/clone
для **каждого вхождения** вызывает always-clone `412BE0`: повторные указатели
дают разные Model, но shared Mesh. Это не map-aware entry `412C40→4D3810`.
Copy **добавляет** элементы в непустой destination, не очищает его; spheres
копируются до и снова после append-loop. Destructor сначала
освобождает хвостовые callbacks, затем support-subobject и лишь потом `spNode`.

Portable реконструкция использует `shared_ptr`, сохраняет порядок и разные
Model для повторов при clone. Attach немедленно пересчитывает bounds, как
native `469ED0→469820`, **без установки dirty bits**. Host detach/clear не
выдаются за ещё не исполненный native individual detach469F50.

### Bounds, update и render boundary

Чистые PS2 bodies показывают больше, чем один контейнер:

`spModel` в этом dispatch затем передаёт `baseMesh` в renderer slot `9`.
Таким образом, доказана цепочка `camera matrices -> RenderNode culling ->
spRenderable render -> spModel -> platform mesh backend`. Названия secondary
interface, точные сигнатуры и часть cache fields ещё не найдены, поэтому
portable код не симулирует renderer.

PS2 primary vtable header содержит 16 подтверждённых slots:

`0, 0, 1AAEF0, 1A5AC0, 1AB090, 1AA230, 1A9DE0, 100010, 100050,
1AAEA0, 1AACA0, 1AABF0, 1A7160, 1A7130, 1AA2C0, 1A9F30`.

Secondary header содержит 13 slots:

`0, 0, 1AB2D0, 135E10, 135E00, 1AB2E0, 135E80, 135E70, 1AAA20,
1AA810, 1AA580, 1A9F00, 1A9DF0`.

### Связь с optimizer и открытая граница

Открыты: original header/TU/API, имя support-subobject и light-cache helper,
remaining cache roles, automatic Scene/Partition/Occlusion side effects,
scene45EC70/renderer456310/material submission и native individual detach.
Простые callback-vector records теперь
подтверждены как borrowed pointers с swap-last/remove/drain протоколом, но их
оригинальные interface names/внешние lifetime invariants ещё не закрыты.

### Две serializer-секции

```text
UInt32 0x603625D0
char[4] "SBOO"

section 0: spNode fields 0..8
terminator

section 1: repeated field 0, esfRenderNodeRenderable
terminator
```

В исследовательской базе секции адресуются от конца объекта: наследованная
`spNode` имеет `section_from_end = 1`, собственная `spRenderNode` — `0`. Это
позволяет не путать одинаковый номер field 0 у position и renderable.

### Наследованные поля `spNode`

| Field | Семантика | Payload |
| ---: | --- | --- |
| 0 | position | `Vector3` |
| 1 | rotation | quaternion XYZW |
| 2 | scale | `Vector3` |
| 3 | is bone | byte boolean |
| 4 | is static | byte boolean |
| 5 | child | object relationship |
| 6 | billboard axis | `UInt32` enum |
| 7 | collision | object relationship |
| 8 | is animated | byte boolean |

### Собственное поле renderable

| Encoding | PS2 | Всего |
| --- | ---: | ---: |
| ID-only, 4 байта | 0 | 6 |
| Sized reference, 8 байт | 3 839 | 12 827 |
| Inline object | 10 510 | 40 224 |
| **Всего** | **14 349** | **53 057** |

Формы relationship совпадают с `spNode`:

```text
UInt32 objectId                                      // legacy ID-only
UInt32 objectId; UInt32 inlineSize                   // existing object reference
UInt32 objectId; UInt32 inlineSize; byte SBOO[...]   // inline object
```

При inline-форме `inlineSize` во всех случаях точно совпадает с
`SerializedSize` target-объекта. Все ID разрешаются в object directory, а inline
payload начинается с ожидаемых class ID и `SBOO`.

Три различные компактные PC-связи найдены в `Characters/Knut/staff_projectile.smo` (`projectile_staff`) и `Levels/Gardenia/test_world_winx.smo` (`ramp`, `world_box`).

### Viewer и база

`SmoRenderNodeDecoder` возвращает декодированный наследованный `SmoNodeData` и
список renderable relationships. Реестр полей различает собственную и
наследованную секции; inspector показывает семантические имена и target metadata.
Hierarchy теперь включает child-поля `spRenderNode`, а transform resolver
применяет подтверждённые defaults даже при полностью пустой node-секции.

Команда

```text
SmoViewer.Inspect research-db analyze-class <db> spRenderNode
```

Безопасная мутация остаётся открытой. Для изменения списка отношений потребуется
пересчитать inline sizes, object directory и все охватывающие serialized ranges,
после чего проверить результат нативным loader игры.
