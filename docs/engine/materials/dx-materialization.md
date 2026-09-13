# PC: DX metadata, buffer materialization и declaration

## Исправленный PC wire contract

`429A40` читает четыреu32 иbyte в переиспользуемые temporaries: **planning
header не участвует в самой материализации**. После него реально вызываются:

1. `45F9D0` factory и `45FB80` обычного spIndexBuffer: type, primitiveCount,
   flags, index bytes.
2. `460130` factory и `460300` обычного spVertexBuffer: componentFlags,
   vertexCount, flags, vertex bytes.
3. Выходной mesh virtual initializer -> `4AA000`: преобразование/копирование
   CPU buffers в выбранное DX storage.

## Buffer и mesh lifetime

Actual factories подтвердили DX vertex wrapper20h, index wrapper1Ch,
**spDXMesh88h**. Vertex ctor:COM pointer/14/18 нули,byteSize1C остаётсяCC;
index ctor:COM pointer/14/18 нули. Роль reserve fields18/14 всё ещё неизвестна.
Combiner2Ch ctor не пишет target4, обнуляет остальные собственные слова.

Create sequence:index→vertex; Lock:vertex→index; partial commit меняет cursors/counts, exact-full unlock-ает vertex→index; destructor отпускает index→vertex. Mesh и combiner каждый держат по одной intrusive ссылке на общие wrappers; COM resource принадлежит wrapper-у. Подтверждены ignored Create/Lock/Unlock HRESULT. Failed Lock leaves null cursors; unsafe copy после него **не исполняется**. Это не полный device-loss или allocation-failure контракт.

Два дополнительных original hazards:

Доказанные полные helper calls всё ещё не являются whole mesh-containing
FFPS load, whole save, source differential всей геометрии или GPU rendering.
Неизвестные ветви перечисляются дальше;100% не объявляется.

## Checkpoint8: concrete readers и whole mesh-containing FFPS

Макс30439 instructions/63760 heap. Field80 прежде ошибочно назывался UV count: он использует priority bits 02/04/08/10→1/2/3/4, а на840 даёт0 при наличии одного UV. Алгоритм source уже был правильным, исправлены только ABI/API/field labels; inferred blend weight role и оригинальное неизвестное имя не выдаются за доказанный symbol.

Ещё9 проверок deferred/strict RTTI в SparkBase. Все28 CTests прошли18,69sec; workbench10/10, platform12/12, два новых bounded profiles6/6 и1/1. Новых native caps нет. Далее: Node/RenderNode/Model graph readers и save dispatch, texture/material/ skin зависимости, renderer lifetime и настоящая backend validation.
