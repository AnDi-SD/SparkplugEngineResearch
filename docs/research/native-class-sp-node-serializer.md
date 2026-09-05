# `spNodeSerializer`: общий сериализатор узлов

Статус: проверенный PC/PS2 срез реконструкции. Исполняемые файлы и игровые
ресурсы не изменялись.

## Происхождение и идентичность

PC executable содержит точный путь исходника:

`Z:\Sparkplug\Code\Sparkplug\spNodeSerializer.cpp`

Обе сборки регистрируют один контракт:

| Свойство | PC | PS2 |
|---|---:|---:|
| Class ID | `0x4545848A` | `0x4545848A` |
| Registered base ID | `0x42429877` (`spSerializer`) | тот же |
| Registration | `0x007601C0` | `0x004AA810` |
| Initializer | `0x006D3970` | `0x00483710` |
| Factory | `0x004638F0` (protected entry) | `0x00197660` |

PS2 factory выделяет ровно `0x14` байт. Constructor `0x00197540` сначала
вызывает `spSerializer::spSerializer` (`0x00181D00`), затем ставит primary
vptr `0x0048F950` и secondary vptr `0x0048F974` по `+0x10`. Нового storage нет.
PC constructor/factory защищён, но все наблюдаемые обращения также заканчиваются
на `+0x10`; поэтому его `0x14` пока отмечен как полный observed extent, а не как
прямо доказанный `sizeof`.

## Lifetime и virtual contract

| Роль | PC | PS2 |
|---|---:|---:|
| non-deleting/deleting destructor | `0x004638C0` / `0x004639B0` | deleting `0x001974D0` |
| blank clone | `0x00463960` | `0x00197580` |
| registration getter | `0x004638B0` | `0x00196520` |
| target class ID | `0x004638E0` | `0x001974C0` |
| relationship finalizer | `0x004639D0` | `0x00196A60` |
| read | `0x00463A70` | `0x00196530` |
| write | `0x00463F10` | `0x00196B90` |

Target-ID slot возвращает `0x695C0F65`, то есть `spNode`. Clone создаёт новый
пустой serializer через factory, регистрирует пару в clone manager и использует
унаследованный no-payload copy. PS2 полностью подтверждает этот путь; PC даёт
тот же результат через vtable и clone body.

## Поля `spNode`

Read dispatch и writer на обеих платформах подтверждают таблицу:

| ID | Аналитическое имя | Payload | Правило writer |
|---:|---|---|---|
| 0 | `Position` | `Vector3` | не писать около `(0,0,0)` |
| 1 | `Rotation` | quaternion | не писать около identity |
| 2 | `Scale` | `Vector3` | не писать около `(1,1,1)` |
| 3 | `IsBone` | byte bool | писать только `true` |
| 4 | `IsStatic` | byte bool | писать только `true` |
| 5 | `Child` | object relationship | повторять для каждого child |
| 6 | `BillboardAxis` | `UInt32` | не писать `0`; известны `1/2` |
| 7 | `Collision` | object relationship | повторять для каждого collision |
| 8 | `IsAnimated` | byte bool | писать всегда |

Сравнения transform с default используют константу `0x3A83126F`, то есть
примерно `0.001`. Порядок writer: `0, 1, 2, 3, 4, 8, 5, 6, 7`. Rotation в
runtime хранится матрицей `3x3`, а stream получает quaternion через отдельный
math helper; его численная реализация пока не переносилась.

Read применяет position/scale и помечает transform dirty, преобразует quaternion
в orientation, меняет флаги `0x1000`, `0x400`, `0x800`,
`0x00100000/0x00200000`, присоединяет child и добавляет `spCollisionInfo`.
После чтения отдельный callback проходит все child/collision relationships и
передаёт их общему fixup `spSerializer`.

## Восстановленная граница

В `Sparkplug/Code/Sparkplug/spNodeSerializer.*` восстановлены:

- RTTI/factory/blank clone и target Class ID;
- точный enum девяти field IDs;
- проверяемый `BuildKnownWritePlanForAnalysis`, повторяющий native order,
  default suppression и число доступных child relationships;
- отдельные PC/PS2 ABI records и адреса.

Потоковый read/write намеренно не имитируется. Portable `spNode` ещё не хранит
`spCollisionInfo`, а quaternion codec, data-block protocol, relationship rollback
и исходное имя secondary interface не закрыты. Поэтому метод называется
`BuildKnownWritePlanForAnalysis`: он не заявляет, что collision fields отсутствуют
в настоящем объекте.

## Проверка

Сборка выполняется сериализованно (`ninja -j1`). Тест проверяет RTTI, factory,
target ID, blank clone, default-план только с обязательным field 8 и порядок
`0,1,2,3,4,8,5,6` для изменённого узла с одним child.

## Открытые вопросы

1. Original header, namespace и имена secondary-interface методов.
2. Точный PC allocation size без опоры на observed extent.
3. Типы data stream/context и значения status enum.
4. Quaternion conversion и byte-exact payload encoding.
5. `spCollisionInfo`, forward-reference fixup и rollback при ошибке.
6. Семантика старых файлов, где field 8 отсутствует.
