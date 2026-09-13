# spSerializer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spSerializer](../../../Sparkplug/Code/Sparkplug/spSerializer.h), [spSerializerManager](../../../Sparkplug/Code/Sparkplug/spSerializerManager.h).

| Платформа | PC | PS2 |
| --- | ---: | ---: |
| Class ID / base | `0x42429877 / spBaseObject` | то же |
| Object extent | observed `0x14` | exact `0x14` через concrete factories |

Точный исходный путь `Z:\Sparkplug\Code\Sparkplug\spSerializer.cpp` сохранён в PC diagnostics.

## Layout и абстрактность

Обе версии сначала строят `spBaseObject`, затем устанавливают отдельный vptr по
`+0x10`. Собственных data fields за ним не обнаружено. PS2 factories
`spNodeSerializer` и `spLightDataSerializer` обе выделяют ровно `0x14`, поэтому
там extent доказан напрямую. PC protected factories и `.rld` не позволяют так
же строго назвать `sizeof`, но все constructors/destructors используют тот же
полный префикс.

RTTI registration намеренно не содержит factory. Кроме того, secondary table
базового класса оставляет payload callbacks абстрактными: сам `spSerializer`
не является обработчиком конкретного object type. Clone возвращает null, copy
повторно использует no-payload реализацию `spBaseObject`.

## операции

Object-header reader `0x00467550 / 0x001815B0`:

1. читает восьмибайтовый object header `[class ID, "SBOO"]`;
2. пропускает class ID через identity/remap hook текущего serializer-а;
3. создаёт объект через `spRTTIManager`, если ID зарегистрирован и имеет factory.

Он не ищет serializer в `spSerializerManager`: прежняя формулировка смешивала
RTTI validation с последующим manager dispatch.

PS2 дополнительно позволяет разделить save pipeline:

- `0x00181BE0` получает runtime Class ID, добавляет объект в FAT maps через
  helper `0x0017FC60` и только для новой entry запускает index callback;
- `0x00181720` выбирает serializer через manager по runtime object и вызывает
  relationship-index callback;
- `0x001817E0` находит уже индексированный object в object→entry map, пишет ID,
  а payload записывает только один раз по флагу `entry+0x1C`;
- перед payload он сохраняет начало в `entry+0x14`, пишет `[Class ID, SBOO]`,
  вызывает secondary writer, затем вычисляет `entry+0x18 = end - start` и
  возвращается к концу stream после заполнения size.

Таким образом PC `0x004672C0` / PS2 `0x00181BE0` точнее называются
index-object entry, а не всей сериализацией. Полная запись использует несколько
методов, FAT и manager dispatch.

`spLightDataSerializer` не вызывает `spSerializer` напрямую: его constructor
идёт через `spNodeSerializer` (`PS2 0x00197540`), после чего ставит собственные
vtable `0x00490110 / 0x00490134`. При этом RTTI records и `spNodeSerializer`, и
`spLightDataSerializer` регистрируют прямой base ID `0x42429877`. Это различие
между C++ implementation inheritance и engine RTTI нельзя схлопывать. Поэтому
до полного `spLightDataSerializer` следующим dependency-классом разбирается
`spNodeSerializer`.

Открыты: исходное имя secondary interface, оставшиеся signatures callbacks,
error/result enum, rollback/cyclic ownership, полная FFPS integration и PC exact allocation.
Структура `[Class ID, SBOO]`, registry ownership и dispatch теперь закрыты;
подробности — в
[`spSerializerManager`](sp-serializer-manager.md).
