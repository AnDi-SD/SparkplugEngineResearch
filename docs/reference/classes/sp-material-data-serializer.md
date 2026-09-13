# spMaterialDataSerializer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spDXMaterial](../../../Sparkplug/Code/SparkplugDX/spDXMaterial.h), [spMaterialData](../../../Sparkplug/Code/Sparkplug/spMaterialData.h), [spMaterialDataSerializer](../../../Sparkplug/Code/Sparkplug/spMaterialDataSerializer.h).

Статус: RTTI, lifetime, размер, target и тонкие read/write-обёртки подтверждены
независимо в PC и PS2. Собственной material grammar у класса нет: она наследуется от
`spMaterialSerializer`.

Размещение в `Code/Sparkplug` следует подтверждённому base-классу; это отдельно помечено в исходнике.

## Идентичность и ABI

Class ID — `0x0B251467`, прямой зарегистрированный и C++ base —
`spMaterialSerializer` (`0x2A14745F`), target — `0x6160348B` (`spMaterialData`).

PS2 factory выделяет ровно `0x3C` байта. Конструктор/деструктор меняют vptr поверх
`spMaterialSerializer`, а новых members после его embedded data-block helper не
инициализируют. PC factory находится в защищённой секции, поэтому его размер отмечен как
наблюдаемый, а не как independently exact.

## Методы

PS2 write wrapper передаёт управление общему writer `spMaterialSerializer` по
`0x00193060`. Reader сначала отклоняет нулевой target, затем вызывает общий reader по
`0x00193EB0`. Secondary thunks находятся по `0x001A5570` и `0x001A5560`; index thunk
остаётся у общего material serializer (`0x00195E40`). Это подтверждает, что отдельной
грамматики и собственных resource relationships у leaf-класса нет.

## Восстановленная граница

Portable-класс восстанавливает только доказанные свойства:

Он намеренно не копирует внутренний stream codec. Планировщик базового класса по-прежнему отклоняет нестандартные layer classes, так что thin leaf не расширяет доказательную границу молча.
