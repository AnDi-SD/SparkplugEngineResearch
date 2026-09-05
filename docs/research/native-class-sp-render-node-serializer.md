# Нативный `spRenderNodeSerializer`

Дата проверки: 2026-09-05. Статус: identity, factory/lifetime, target,
relationship read/index/write и storage-free layout подтверждены PC/PS2.

## Происхождение и identity

PC содержит точный translation-unit path:

`Z:\Sparkplug\Code\Sparkplug\spRenderNodeSerializer.cpp`

Обе платформы регистрируют class ID `0x66EF6060` с прямым base
`spNodeSerializer` (`0x4545848A`) и target `spRenderNode` (`0x603625D0`).

| Факт | PC | PS2 |
|---|---:|---:|
| Registration / initializer | `0x007604C0 / 0x006D3AF0` | `0x004AA8D0 / 0x00483790` |
| Factory | protected `0x00469040` | `0x00198420` |
| Constructor | protected | `0x00198300` |
| Destructor | `0x00469010`, deleting `0x00469100` | deleting `0x00198290` |
| Clone | `0x004690B0` | `0x00198340` |
| Registration getter | `0x00469000` | `0x00197E30` |
| Target ID | `0x00469030` | `0x00198280` |
| Read / index / write | `0x00469190 / 0x00469120 / 0x00469340` | `0x00197E40 / 0x00197FF0 / 0x001980A0` |
| Primary vtable | `0x006E8A44` | header `0x0048FA10` |
| Serializer interface | `0x006E8A38` | header `0x0048FA34` |

PS2 factory выделяет ровно `0x14` байт, вызывает `spNodeSerializer`
constructor и меняет только две vtable. Derived storage отсутствует. PC
обращения также заканчиваются secondary vptr по `+0x10`, но защищённая factory
оставляет `0x14` как полный observed extent, а не прямой `sizeof`.

Clone создаёт пустой serializer, регистрирует пару в clone manager и использует
унаследованный no-payload copy.

## Единственное поле и read contract

Собственная секция содержит одно повторяемое поле:

| ID | Имя из executable | Payload |
|---:|---|---|
| `0` | `esfRenderNodeRenderable` | relationship на `spRenderable` |

Reader сначала вызывает `spNodeSerializer::Serialize`, затем проходит
data-block поля своей секции. Для каждого field `0` он вызывает общий resolver
с требуемым class ID `spRenderable` (`0x4FDA4542`) и немедленно добавляет
результат в support/container `spRenderNode`: `+0xB4` на PC, `+0xC8` на PS2.
Неизвестные field IDs пропускаются общим `spDataBlockSerializer`.

Null relationship является ошибкой, а не пустым элементом. Обе сборки имеют
одинаковую диагностику:

`No empty rendernode->renderable (NULL renderable) relation allowed. File corrupt?`

Наблюдаемый reader не показывает транзакционного rollback уже добавленных
связей при более поздней ошибке; portable API поэтому обещает только проверку и
добавление одной уже разрешённой relationship.

## Index и write

Index pass сначала вызывает inherited node relationship indexer, затем индексирует
каждый renderable в storage order. PC независимо читает vector
`+0xBC..+0xC0`; PS2 читает count `+0xD0` и storage `+0xD4`.

Writer также сначала сериализует полную `spNode`-секцию. После BeginObject он
для каждого renderable повторяет:

1. `WriteBegin(esfRenderNodeRenderable, 7)`;
2. `SerializeRelationship(...)`;
3. `WriteEnd(esfRenderNodeRenderable)`;

и завершает собственную секцию через `FinalizeObject`. Значение `7` является
нативным data-block size/relationship code; исходное enum spelling пока не
восстановлено.

Portable `WritePlanForAnalysis` возвращает отдельно inherited node fields и
упорядоченный список renderable pointers. Такой интерфейс не притворяется
полным stream writer, пока общий relationship framing/fixup ещё не замкнут.

## Проверка и остаток

`research/inspect_render_node_serializer.py` выполняет 47 read-only проверок:
хэши двух executables и всех основных bodies, registration/base/target,
vtable, exact PS2 allocation, platform container offsets, field `0`/code `7`
и null diagnostic. Portable CTest проверяет RTTI/factory/clone, inherited plan,
повторные alias-связи и отказ от null/не-`spRenderable` объектов.

Открыты: исходный header и secondary-interface имя, прямой PC allocation,
полный common relationship framing/fixup, stream error type, partial-read
rollback и точное enum-имя кода `7`. Следующий model/export узел выбирается из
DX optimizer grouping и renderer consumption, а не из несвязанного leaf.
