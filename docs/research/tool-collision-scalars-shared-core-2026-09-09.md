# Общие CollisionInfo scalars — 9 сентября 2026

Блок17 цикла до07:30; PC EXE SHA256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

`spCollisionInfoSerializer::ReadScalarFieldForAnalysis` содержит прежние тела
field1 Group и field2 PRS. Full reader и новый `spv_collision_info_values`
используют один helper. Size/finite guards прежние; native defaults Group1,
P0/Ridentity/S1 не изменены. Optional raw quaternion observation не меняет
настоящую Matrix3 внутри CollisionInfo. Descriptor ABI ограничивает field1/2,
размеры4/40 bytes, полные границы входа и65536 descriptors.

C# CollisionInfo decoder больше не читает числа сам, не нормализует quaternion
и не отвергает нулевой quaternion. Group/Transform допускают исходный порядок,
повторения (последнее значение), bounded unknown fields и отсутствующие поля.
EffectiveCollisionGroup отделён от nullable authored group; field mask сохраняет
присутствие. Primitive остаётся metadata-only relationship через общий prefix
reader; null/отсутствующий primitive не подменяется выдуманным объектом.
Nonnull target должен разрешиться в поддерживаемый inspector bounding-volume
класс. Эта host-граница не объявляется запретом движка на другие наследники.

`SmoCollisionInfoTransform.WorldMatrix` теперь использует общий quaternion/affine
расчёт вместо C# математики. Это матрица **сериализованных PRS**, не результат
runtime attachment/registration в произвольной сцене. Финальный owning Node и
правило выбора stored/node transform в collision placement остаются отдельной
работой. Обычный Node обновляет CollisionInfo из своего world, PartitionSystem
сохраняет stored PRS; физический parent каталога не доказывает runtime owner.
Полная загрузка уровня пока не подключена: помимо лимита4096 objects отсутствуют
некоторые runtime классы/serializers, в том числе StaticRenderObject/Zone/
PartitionSystem. Нельзя подменять их пустыми factories ради успешного loader.

## Проверки

`validate_tools_collision_scalars.py`: пять свежих original cases — defaults,
nonunit Q, zero Q, group-only, unknown/repeated fields. Реально исполняются
factories4653A0/438960, reader438A80, affine builder461D70 и полное owning teardown.
P/R/S совпадают побитно, group/mask точно равны, raw quaternion сохранён, все
64 bytes affine matrix равны. Primitive-reference seams в этом probe нет.
Три descriptor guards проверены. Zero/nonunit Q — подтверждённое original
поведение; прежняя C# normalization была ошибкой приложения.

C++ CollisionCore54. C#9319 PC menu,2994 PS2 menu,1552 tile_bad,1566 Icy,
36315 Alfea02; synthetic cases проверяют null/defaults, repeats, raw Q и
truncated payload. Старая transform-only заготовка без terminator исправлена
добавлением реального завершителя. Viewer/Importer builds0 warnings/errors.
Три collision append на Alfea01/02/03 прошли; полные outputs побайтно равны блоку16.
Reports: `local-data/results/tools-core-cycle-20260909-0730/collision-scalars/`.
Snapshot фиксирует sources/reports/DLL. Новый PS2 runtime, весь corpus и релиз
этим блоком не заявляются.
