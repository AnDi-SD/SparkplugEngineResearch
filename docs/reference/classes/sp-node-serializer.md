# spNodeSerializer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spNodeSerializer](../../../Sparkplug/Code/Sparkplug/spNodeSerializer.h).

`Z:\Sparkplug\Code\Sparkplug\spNodeSerializer.cpp`

## Lifetime и virtual contract

Target-ID slot возвращает `0x695C0F65`, то есть `spNode`. Clone создаёт новый
пустой serializer через factory, регистрирует пару в clone manager и использует
унаследованный no-payload copy. PS2 полностью подтверждает этот путь; PC даёт
тот же результат через vtable и clone body.

## Поля `spNode`

Read dispatch и writer на обеих платформах подтверждают таблицу:

| ID | Аналитическое имя | Payload | Правило writer |
| ---: | --- | --- | --- |
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
math helper; PC helper теперь переиспользован из общей SAN math реконструкции.

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

## Открытые вопросы

1. Original header, namespace и имена secondary-interface методов.
2. PC allocation0x14 закрыт native factory; полный failure/rollback остаётся открыт.
3. Типы data stream/context и значения status enum.
4. Произвольные численные края quaternion, unsafe/error payloads и lossless unknowns.
5. `spCollisionInfo`, forward-reference fixup и rollback при ошибке.
6. Отсутствующий field8 оставляет default Animated=true. Zero не очищает его,
   как и Static; эта native асимметрия подтверждена, безопасная политика редакторов ещё нужна.
