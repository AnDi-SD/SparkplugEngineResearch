# `spDXMeshDataSerializer`: общий меш и DX payload

Статус: exact source identity, RTTI/lifetime, storage-free ABI, target, два
field ID, write/read branch и DX payload header подтверждены на PC/PS2.
Backend buffer codec пока остаётся evidence-only.

PC сохраняет точный translation-unit path:

`Z:\Sparkplug\Code\Sparkplug\spDXMeshDataSerializer.cpp`

PS2 независимо содержит filename `spDXMeshDataSerializer.cpp`.

## Идентичность и ABI

Class ID `0x77006ABE`, C++ и registered base — direct
`spMeshDataSerializer` (`0x66380037`), target — `spDXMeshData::ClassID`
(`0x3178114C`).

| Свойство | PC | PS2 |
|---|---:|---:|
| Registration | `0x0075E338` | `0x004A9190` |
| Initializer | `0x006D2B70` | `0x004827A0` |
| Factory | `0x004297C0` (protected entry) | `0x00162180` |
| Allocation | observed `0x14` | exact `0x14` |
| Primary vtable | `0x006DCCB8` | header `0x0048E3D0` |
| Secondary vtable | `0x006DCCAC` | header `0x0048E3F4` |

PS2 factory вызывает `spMeshDataSerializer` constructor `0x00162A90`, меняет
только два vptr и выделяет ровно `0x14` байт. PC destructor `0x00429790`
аналогично восстанавливает оба DX serializer vptr и переходит в base
destructor; отдельного storage класс не добавляет.

## Методы

| Роль | PC | PS2 |
|---|---:|---:|
| registration getter | `0x00429780` | `0x00161610` |
| generic `SBOO` load slot | `0x0042AFD0` | `0x00162990` |
| load cross-platform payload | `0x0042B0A0` | `0x001622E0` |
| load platform-specific payload | `0x00429A40` | `0x00161630` |
| serialize platform-specific payload | `0x004298A0` | `0x001618A0` |
| write | `0x00429EA0` | `0x00161DA0` |
| index resource graph | `0x005A7DB0` | `0x00161620` |
| read | `0x00429BC0` | `0x00161AE0` |
| target class ID | `0x004297B0` | `0x00162020` |
| deleting destructor | `0x00429880` | `0x00162030` |
| blank clone | `0x00429830` | `0x001620A0` |

PS2 secondary thunks `0x001622C0/0x001622B0/0x001622A0` адаптируют write,
indexing и read. Index pass возвращает success без relationships.

## Data-block contract

| ID | Диагностическое имя | Условие writer |
|---:|---|---|
| 0 | `esfMeshDataCrossPatform` | native mode равен `0` или `2` |
| 1 | `esfMeshDataPlatformSpecific` | всегда |

Порядок строго `CrossPlatform? -> PlatformSpecific`; оба поля имеют type `7`.
Field 0 использует base helper и пишет index buffer перед vertex buffer.
Field 1 сначала строит временный `spDXMeshData` из входного `spMeshData`, а
затем пишет DX header и оба преобразованных buffer payload. Как и в PS2
serializer, target class ID описывает результат reader-а, а не тип уже готового
backend packet на входе writer-а.

Reader обрабатывает только `0/1`, неизвестные fields пропускает и финализирует
объект. В cross-platform режиме field 0 создаёт/читает общие buffers и вызывает
initializer выходного DX mesh. В native режиме field 1 читает DX header и
backend buffers. Неактивная ветка пропускается.

Маска native-reader конфигурации снова различается: PC `0x02`, PS2 `0x08`.
Совпадение с PS2 serializer подтверждает общую платформенную настройку, но
layout-числа всё равно сохранены раздельно.

## DX payload header

Перед buffer data writer последовательно сохраняет:

1. `uFVFCode` — component flags исходного vertex buffer;
2. vertex count;
3. `uVBDataSize`;
4. index-buffer byte size;
5. `Is32Bit`;
6. для подтверждённого **PC reader** — полный обычный serialized index buffer;
7. затем полный обычный serialized vertex buffer.

Уточнение6 сентября: прежний порядок «raw vertex, затем raw index» был неверным
для PC429A40. Actual helper читает17byte planning header, **игнорирует все пять
значений при materialization**, затем вызывает45FB80 и460300. Это независимо
проверено на directed bytes и readonly field1 изlogo_screen.smo. Protected PC
writer4298A0 ещё не исполнен полностью; его прежнее описание, основанное на
частичных/cross-platform свидетельствах, не считать byte-exact PC writer proof.
Подробности: [PC DX materialization](native-pc-dx-materialization.md).

`uVBDataSize` начинается с `vertexStride * vertexCount`. При component bit
`0x20` native writer добавляет ещё `12 * vertexCount`. Index size равен index
count, умноженному на `2` или `4` согласно bit 0 format flags. Portable
`NativePayloadHeader` воспроизводит только эту доказанную арифметику и безопасно
отклоняет отсутствующие buffers/32-bit overflow.

## Portable-срез

Восстановлены RTTI/factory, blank clone, target ID, field plan, раздельные
reader masks и header planner. Реальное преобразование в Direct3D buffer и
stream read/write не симулируется поверх host containers: ещё неизвестны
original Direct3D ownership, FVF validation, device-loss path и serializer
status/rollback API.

## Проверка

Последовательная сборка `ninja -j1` и оба CTest-набора проходят. Тесты
фиксируют direct base, exact PS2/observed PC `0x14`, target, write plans,
platform masks, blank clone и контрольный header: FVF `0x20`, 3 vertices,
84-byte expanded vertex payload, 24-byte 32-bit index payload.

## Открытые вопросы

1. Original header и имя secondary serializer interface.
2. Прямое PC `sizeof` и распаковка protected factory.
3. Имена native mode/config flags и полный FVF mapping.
4. Whole field writer и native-containing FFPS chain; PC read buffer order,
   combiner usage/pool и directed shared ownership теперь исполнены отдельно.
5. Error/status enum, partial-allocation cleanup и rollback.
