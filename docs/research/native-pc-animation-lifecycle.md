# PC `spAnimation`, `spTrack`, `spAnimTrack`: object lifetime

Checkpoint 2026-09-05, продолжение [SAN keys](native-pc-animation-keys.md).
Pristine PC SHA-256:
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
PS2 deferred; оригинальные ресурсы и приложения не изменялись.

## Исправление предыдущей интерпретации

`0x00413090` — destructor **`spNamedObject`**, а не root `spBaseObject`:
он разрушает shared name по `+0x10` и лишь затем вызывает root `0x004102B0`.
Constructor `0x00413060` разрешён в `0x00407110`: root constructor, named
vtable `0x006DB5E8`, name pointer = 0. Protected copy `0x00413120` удерживает
shared name, повышая byte count entry `+8`. Это не no-op copy.

Следовательно, физический base `spAnimation` — `spNamedObject`; его `+0x10`
не неизвестный animation field. Engine RTTI по-прежнему объявляет
`spAnimation → spController → spSubController → spBaseObject`. Две иерархии
не сливаются. Clone анимации создаёт пустую анимацию **с именем источника**,
не переносит duration/tracks/tags. Имя копируется и у `spAnimTrack`.
Старые immutable manifests не редактируются; уточнение записывается новым evidence.

## Идентичность и размеры

| Тип | Class ID | Physical / engine base | Exact PC size | Factory / vtable |
|---|---:|---|---:|---|
| `spAnimation` | `0x56EE563A` | `spNamedObject` / `spController` | `0x84` | `0x0041A090` / `0x006DE6CC` |
| `spTrack` | `0x60C839C5` | `spNamedObject` / тот же | `0x14` | `0x00493070` / `0x006ECBA4` |
| `spAnimTrack` | `0x33B61869` | `spTrack` / тот же | `0x44` | `0x004791B0` / `0x006EAA24` |

Original class names подтверждены регистрациями `0x006D4980` и `0x006D3EB0`;
registration objects `0x00762B68` и `0x00760C98`. Embedded SAN track теперь
можно называть **`spAnimTrack`**, а не безымянным record. Точного original TU/header
для этих трёх классов не найдено: новые `Code/Sparkplug/*.h/.cpp` — inferred paths.
Exact `spAnimationSerializer.cpp` остаётся отдельным translation unit.

Оба track vtable содержат девять slots и заканчиваются перед строками
`spTrack`/`spAnimTrack`. Extra slots `+0x1C/+0x20`:

- base: `0x00493060` возвращает float 0, `0x0048EAA0` — no-op;
- animation track: `0x00478E30` возвращает max последнего time всех присутствующих
  PRS descriptors, начальное значение 0; `0x00479760` освобождает key resources.

`spTrack` конкретен, не abstract. Binding `+0x14` принадлежит **derived**
`spAnimTrack`, не base track. Остальные fields: nine descriptor pointers `+0x18`,
track-wide ownership byte `+0x3C`, owner animation `+0x40`.

## Factory и constructor

`spAnimation` factory `0x0041A090 → 0x013D1E00` выделяет `0x84`, затем вызывает
`0x00430290 → 0x013C64B0`. Эти оригинальные instructions исполняются без patch
подставленного тела. Constructor:

- вызывает настоящий `spNamedObject` constructor;
- обнуляет time `+0x14`, пока безымянное `+0x18`, track storage/count/capacity;
- обнуляет tag vector begin/end/capacity и семь shared buffers;
- **не пишет** container state `+0x28`; это не доказанный numeric default;
- создаёт descriptor pool с entry size 16, 64 entries/block, initial blocks 0,
  block limit `0xFFFFFFFF`, coefficient `0x3EAA7EFA`;
- берёт значение для `+0x54` из `spDebugManager::0x0041D4E0`.

Последнее слово — не постоянный `-1`: helper последовательно берёт значения
20-entry table `0x0073FEE8`, пропускает sentinel `0x0073FEBC` и оборачивает cursor.
Constructor дополнительно исключает указанное protected constant значение,
при совпадении запрашивая ещё раз. При pristine table первые две анимации
получают `0xFFFFFFFF` и `0xFFFF0000`. Lazy reference `0x0075526C` указывает на
реальный созданный `spDebugManager`; его собственный singleton — `0x00755278`.
Не следует выдавать этот эксперимент за полный debug renderer lifecycle.

Standalone `spAnimTrack` factory и constructor `0x00478DE0` обнуляют descriptors,
ставят binding `-1` и ownership false, но **не инициализируют owner `+0x40`**.
Owner ставится только при append к animation. Поэтому standalone RTTI factory
сама по себе ещё не делает безопасным attach descriptors.

## Track array и уничтожение

Append `0x0042FED0 → 0x013C91A0` при `count == capacity` увеличивает capacity
ровно на 1, выделяет новый массив `capacity*0x44`, переносит старые records
побайтово и освобождает прежний массив. Затем конструирует один `spAnimTrack`,
ставит owner и возвращает его адрес. **Адреса старых tracks при росте меняются**;
это важно для evaluator inputs и будущего loader/binder order.

Reserve/resize `0x00430010` переносит retained bytes и обрезает count до новой
capacity. При уменьшении native loop разрушает `oldCapacity-newCapacity` slots,
не `oldCount-newCapacity`. Поэтому shrink после reserve с неинициализированным
хвостом нельзя считать безопасным универсальным operation. Guest test shrink
использует только полностью сконструированный old capacity; опасный случай
не запускался и не «исправлялся» в executable.

Destructor `0x00430130` снимает name-binding непустых slots через
`0x00453B10`, вызывает deleting destructor track с аргументом 0, освобождает
track array, owned tags, общие times/values, descriptor pool и inherited name.
Реальный name registry пока не включён в linked tests: проверены unbound tracks.

## Владение descriptors: важные особенности оригинала

Attach `0x00479830` в самом начале записывает **новый** ownership bool в
track `+0x3C`. При замене axis 0 этим же входным bool решается, освобождать ли
старые time/value arrays. Не прежним ownership! Матрица `old/new = 0/1 × 0/1`
проверена с явным allocation recorder. Нельзя трактовать флаг как независимый
ownership каждого descriptor: он общий для всего track.

`0x00479760 → 0x013D2850` освобождает time/value arrays, если текущий флаг true,
и возвращает descriptors в pool. При этом **указатели track не обнуляются**:
повторный вызов не доказан безопасным Reset. В probe перед последующим destructor
эти stale pointers явно обнуляются как fixture cleanup, не как native evidence.

Descriptor pool имеет `0x2C` bytes внутри animation `+0x58`. Первый descriptor
создаёт блок на 64 slots; после attach free count 63. Возврат последнего descriptor
снимает полностью свободный блок из active list и сохраняет один spare block:
active block/count/free count становятся 0, spare `pool+0x24` остаётся ненулевым.
Destructor animation освобождает и этот cache. Original template/class name pool
не найден; `AnimationDescriptorPoolLayout` — только analytical ABI label.

## Tags

Field `5` reader-а создаёт `0x1C` object с vtable `0x006E0AE4`, но registration
getter у него root `0x0040E930`. Отдельное original class name пока не доказано.
`+0x10` — owned обычный char buffer, `+0x14` — time, `+0x18` — wire ordinal.
Reader повышает ordinal при чтении, до сортировки. Insertion `0x004305F0`
держит стабильный порядок по возрастанию time; равные times сохраняют order.
Destructor удаляет каждый tag и его owned name. Guest tests проверяют пять tags
без name payload; name setter и полный malformed-tag transaction ещё открыты.

## Что реализовано и где граница

`spTrack`, `spAnimTrack`, `spAnimation` добавлены в portable source tree, а точные
PC layouts — в `Analysis/PC/SparkplugAbi.h`. Prepared keys из прошлого checkpoint
используются новым `spAnimTrack`; отдельные имена sampler-классов не выдумываются.

Host-only отличия указаны в коде: initialized null owner, immutable owned key
snapshots, stable track pointees, bounded counts/finite values, transactional
key replacement и idempotent release. Shrink не разрушает unconstructed slots.
Debug cycle value подаётся явно и не объявляется постоянным default. Name-binding
registry, оригинальный pool allocator, полный SAN loader, actor/frame и D3D
не входят в portable object slice.

В `spNamedObject` добавлен защищённый name-copy helper для физического наследника,
чья engine RTTI не содержит `spNamedObject`: это позволяет не ломать две иерархии.
`spController` представлен identity-only RTTI record, не новым выдуманным concrete class.

## Воспроизведение и безопасность

```powershell
python research\inspect_animation_lifecycle.py
python research\probe_pc_animation_lifecycle.py
python research\test_pc_instruction_emulator.py
```

20 hash/identity anchors; 206 guest checks на момент object checkpoint. Native
allocation/free заменены bounded arena + recorder, включая CRT malloc wrapper
`0x00417190`. FS:0 — явный synthetic segment с отдельным GDT/TEB page, не Windows
loader/exception dispatcher. Линейный null остаётся unmapped (отдельный guard test).
Ошибки памяти не приводят к автоматическому mapping; OS/IAT calls не проксируются.
Пределы: 100 000 instructions/2 sec на call, 30 sec на отдельный Python process.
Это не in-game validation. Непосещённые редкие protector branches не объявлены
исследованной игровой логикой.

Следующий front: полный PC SAN reader со строгими stream/name-binding seams,
tags/error transaction, затем actor state machine и frame caller.
