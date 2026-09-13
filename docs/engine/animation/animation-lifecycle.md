# PC `spAnimation`, `spTrack`, `spAnimTrack`: object lifetime

## Исправление предыдущей интерпретации

`0x00413090` — destructor **`spNamedObject`**, а не root `spBaseObject`:
он разрушает shared name по `+0x10` и лишь затем вызывает root `0x004102B0`.
Constructor `0x00413060` разрешён в `0x00407110`: root constructor, named
vtable `0x006DB5E8`, name pointer = 0. Protected copy `0x00413120` удерживает
shared name, повышая byte count entry `+8`. Это не no-op copy.

## Идентичность и размеры

| Тип | Class ID | Physical / engine base | Exact PC size |
| --- | ---: | --- | ---: |
| `spAnimation` | `0x56EE563A` | `spNamedObject` / `spController` | `0x84` |
| `spTrack` | `0x60C839C5` | `spNamedObject` / тот же | `0x14` |
| `spAnimTrack` | `0x33B61869` | `spTrack` / тот же | `0x44` |

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

Standalone `spAnimTrack` factory и constructor `0x00478DE0` обнуляют descriptors,
ставят binding `-1` и ownership false, но **не инициализируют owner `+0x40`**.
Owner ставится только при append к animation. Поэтому standalone RTTI factory
сама по себе ещё не делает безопасным attach descriptors.

## Владение descriptors: важные особенности оригинала

Descriptor pool имеет `0x2C` bytes внутри animation `+0x58`. Первый descriptor
создаёт блок на 64 slots; после attach free count 63. Возврат последнего descriptor
снимает полностью свободный блок из active list и сохраняет один spare block:
active block/count/free count становятся 0, spare `pool+0x24` остаётся ненулевым.
Destructor animation освобождает и этот cache. Original template/class name pool
не найден; `AnimationDescriptorPoolLayout` — только analytical ABI label.

## Границы описания

Host-only отличия указаны в коде: initialized null owner, immutable owned key
snapshots, stable track pointees, bounded counts/finite values, transactional
key replacement и idempotent release. Shrink не разрушает unconstructed slots.
Debug cycle value подаётся явно и не объявляется постоянным default. Name-binding
registry, оригинальный pool allocator, полный SAN loader, actor/frame и D3D
не входят в portable object slice.

В `spNamedObject` добавлен защищённый name-copy helper для физического наследника,
чья engine RTTI не содержит `spNamedObject`: это позволяет не ломать две иерархии.
`spController` представлен identity-only RTTI record, не новым выдуманным concrete class.
