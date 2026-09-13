# spDXMeshDataSerializer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spDXMeshDataSerializer](../../../Sparkplug/Code/Sparkplug/spDXMeshDataSerializer.h).

PC сохраняет точный translation-unit path:

`Z:\Sparkplug\Code\Sparkplug\spDXMeshDataSerializer.cpp`

PS2 независимо содержит filename `spDXMeshDataSerializer.cpp`.

## Идентичность и ABI

Class ID `0x77006ABE`, C++ и registered base — direct
`spMeshDataSerializer` (`0x66380037`), target — `spDXMeshData::ClassID`
(`0x3178114C`).

PS2 factory вызывает `spMeshDataSerializer` constructor `0x00162A90`, меняет
только два vptr и выделяет ровно `0x14` байт. PC destructor `0x00429790`
аналогично восстанавливает оба DX serializer vptr и переходит в base
destructor; отдельного storage класс не добавляет.

## Методы

PS2 secondary thunks `0x001622C0/0x001622B0/0x001622A0` адаптируют write,
indexing и read. Index pass возвращает success без relationships.

## Data-block contract

| ID | Диагностическое имя | Условие writer |
| ---: | --- | --- |
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

## Открытые вопросы

1. Original header и имя secondary serializer interface.
2. Прямое PC `sizeof` и распаковка protected factory.
3. Имена native mode/config flags и полный FVF mapping.
4. Whole field writer и native-containing FFPS chain; PC read buffer order,
   combiner usage/pool и directed shared ownership теперь исполнены отдельно.
5. Error/status enum, partial-allocation cleanup и rollback.
