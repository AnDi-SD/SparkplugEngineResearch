# spPS2MaterialDataSerializer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spMaterialDataSerializer](../../../Sparkplug/Code/Sparkplug/spMaterialDataSerializer.h), [spMaterialSerializer](../../../Sparkplug/Code/Sparkplug/spMaterialSerializer.h), [spPS2MaterialDataSerializer](../../../Sparkplug/Code/Sparkplug/spPS2MaterialDataSerializer.h).

Class ID — `0x69327633`; прямой base — `spMaterialSerializer` (`0x2A14745F`), не
`spMaterialDataSerializer`. Независимого target-hook нет; write/index/read и layer
helpers остаются общими.

PS2 factory выделяет `0x3C`, вызывает constructor общего material serializer и меняет
только vptr. Его secondary table наследует thunks
`0x00195E50/0x00195E40/0x00195E30`; новых relationships или fields класс не вводит.
Совпадение не сводится автоматически с DX ABI: адреса lifetime/RTTI/vtable хранятся
раздельно.

Portable-класс восстанавливает RTTI и concrete blank clone. Стандартная material grammar
приходит из base. PS2 runtime-лист `spPS2Material` теперь отдельно подтверждён до
exact `0xD0`, color/power layout, deep clone и pass-update consumer; сам механизм
выбора/conversion serializer-а всё ещё не доказан.

Открыты original header/source path, интерфейсные имена/signatures, прямой PC allocation,
platform selection/conversion в `spPS2Material` и in-game mutation test.
