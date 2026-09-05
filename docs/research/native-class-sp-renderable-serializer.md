# `spRenderableSerializer`: базовые renderable-связи и alpha-sort

Статус: identity, inheritance, storage-free layout, lifetime, target, resource
indexing и четырёхпольный writer/read contract подтверждены на PC и PS2.
Потоковая реализация пока остаётся evidence-only.

Имя класса и подробные diagnostics присутствуют в обоих executable, но строка
оригинального source path не найдена. Поэтому
`Code/Sparkplug/spRenderableSerializer.*` — явно inferred расположение.

## Идентичность и ABI

`spRenderableSerializer` имеет Class ID `0x4D694D82`, напрямую наследует и
регистрируется от `spSerializer` (`0x42429877`), а target slot возвращает
`spRenderable::ClassID` (`0x4FDA4542`).

| Свойство | PC | PS2 |
|---|---:|---:|
| Registration | `0x00761398` | `0x004AA870` |
| Initializer | `0x006D41E0` | `0x00483750` |
| Factory | `0x0047F650` (protected entry) | `0x00197D90` |
| Allocation | observed `0x14` | exact `0x14` |
| Primary vtable | `0x006EB6DC` | header `0x0048F9B0` |
| Secondary vtable | `0x006EB6D0` | header `0x0048F9D4` |

PS2 factory выделяет `0x14` и constructor `0x00197C70` вызывает напрямую
`spSerializer` constructor. PC destructor `0x0047F620` возвращается к
`spSerializer` destructor; собственного storage нет.

## Методы

| Роль | PC | PS2 |
|---|---:|---:|
| registration getter | `0x0047F610` | `0x001976F0` |
| write | `0x0047F7B0` | `0x00197700` |
| index resource graph | `0x0047F730` | `0x00197980` |
| read | `0x0047FBA0` | `0x00197A10` |
| target class ID | `0x0047F640` | `0x00197BF0` |
| deleting destructor | `0x0047F710` | `0x00197C00` |
| blank clone | `0x0047F6C0` | `0x00197CB0` |

PC secondary write thunk — `0x0047F7A0`. PS2 secondary thunks
`0x00197E20/0x00197E10/0x00197E00` корректируют `this` для write, indexing и
read соответственно. PC bodies частично закрыты protection stubs, поэтому
семантика перепроверена по diagnostics и прозрачному PS2 коду, а адреса — по
PC vtable и registration.

## Поля и порядок записи

| ID | Поле | Native write condition |
|---:|---|---|
| 0 | material relationship | material не null |
| 1 | fog relationship | fog не null |
| 2 | alpha-sort enable | всегда |
| 3 | alpha-sort priority | всегда |

Writer сохраняет строгий порядок `0, 1, 2, 3`, пропуская только отсутствующие
relationships. Значит, default-объект всё равно записывает поля 2 и 3 со
значениями constructor state (`true`, `0`); это не default suppression.

Reader принимает поля в произвольном block-order и для relationship полей
проверяет target IDs `spMaterial = 0x5C0314C5` и
`spFog = 0x7AC95AEC`. Неизвестные ID передаются общему skip/advance пути.
Отдельный indexing pass последовательно индексирует material, затем fog.

## Portable срез

Восстановлены RTTI/factory, blank clone, target ID и
`BuildKnownWritePlanForAnalysis`. Пока concrete `spMaterial` и `spFog` не
восстановлены, метод использует уже существующие безопасные relationship
facades `spRenderable`, но не вводит ложные типы или ownership.

Не реализованы stream framing, relationship ID table/fixups, read rollback и
status/error enum. Эти механизмы остаются общими открытыми обязанностями
serialization subsystem.

## Проверка

Последовательная сборка `ninja -j1` и оба CTest-набора проходят. Проверяются
direct `spSerializer` base, exact PS2/observed PC `0x14`, target
`spRenderable`, default plan `{2, 3}`, полный plan `{0, 1, 2, 3}` и blank
clone.

## Открытые вопросы

1. Original header/TU path и имена secondary interface.
2. Прямой PC allocation size вместо observed extent.
3. Точные stream/status types и block framing.
4. Relationship ownership/fixup/rollback contract.
5. Original enum spelling для scalar fields 2 и 3.
