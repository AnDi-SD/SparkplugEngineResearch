# Нативный класс `spSerializer`

Статус: RTTI, прямой base, constructor/destructor, отсутствие factory, null clone,
двойной vptr-префикс и центральные load/save границы подтверждены на PC и PS2.
Переносимый RTTI/identity-срез реализован. Registry и FAT boundary теперь
закрыты в отдельном срезе `spSerializerManager`; точный secondary stream
interface и relationship/failure callbacks остаются evidence-only.

| Платформа | PC | PS2 |
|---|---:|---:|
| SHA-256 | `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` | `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE` |
| Class ID / base | `0x42429877 / spBaseObject` | то же |
| Registration / initializer | `0x007602E0 / 0x006D3A00` | `0x004A9DF0 / 0x00483050` |
| Factory / properties | null / null | null / null |
| Constructor | protected `0x004063F0` | `0x00181D00` |
| Destructor | body `0x004671B0`, deleting `0x00467240` | deleting `0x00181C90` |
| Clone / copy / getter | `0x004A1BF0 / 0x0040ECE0 / 0x004671D0` | `0x00181D50 / 0x00100320 / 0x001810E0` |
| Primary / secondary vtable | `0x006E81C0 / 0x006E81B4` | headers `0x0048EFB0 / 0x0048EFD4` |
| Object extent | observed `0x14` | exact `0x14` через concrete factories |

Точный исходный путь `Z:\Sparkplug\Code\Sparkplug\spSerializer.cpp` сохранён в
PC diagnostics. Имя header и имя вторичного interface пока не найдены.

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

## Подтверждённые операции

`0x004671E0` и `0x001814D0` получают serialized class ID и возвращают его без
изменений. Переносимый `ResolveClassIDForAnalysis` воспроизводит только этот
полностью ясный hook.

Object-header reader `0x00467550 / 0x001815B0`:

1. читает восьмибайтовый object header `[class ID, "SBOO"]`;
2. пропускает class ID через identity/remap hook текущего serializer-а;
3. создаёт объект через `spRTTIManager`, если ID зарегистрирован и имеет factory.

Прежнее утверждение о проверке marker-а исправлено: оба native body заранее
инициализируют второе слово байтами `SBOO` и затем перезаписывают все восемь
байт из stream, но после чтения загружают только первое слово. Ни PC, ни PS2
не сравнивают фактически прочитанный marker. Portable
`ReadObjectHeaderAndCreateForAnalysis` сохраняет это поведение, одновременно
возвращая observed header; строгий importer может отдельно вызвать
`HasCanonicalObjectMarkerForAnalysis` и отклонить повреждённый файл.

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

## Новая зависимость для следующего класса

`spLightDataSerializer` не вызывает `spSerializer` напрямую: его constructor
идёт через `spNodeSerializer` (`PS2 0x00197540`), после чего ставит собственные
vtable `0x00490110 / 0x00490134`. При этом RTTI records и `spNodeSerializer`, и
`spLightDataSerializer` регистрируют прямой base ID `0x42429877`. Это различие
между C++ implementation inheritance и engine RTTI нельзя схлопывать. Поэтому
до полного `spLightDataSerializer` следующим dependency-классом разбирается
`spNodeSerializer`.

PS2 `0x00181150` дополнительно закрывает load-side relationship resolver.
Он читает `objectID` и inline-size, находит FAT entry, возвращает уже
materialized pointer либо ищет serializer/cache и рекурсивно создаёт inline
объект. Если pointer уже готов и inline-size ненулевой, stream просто
продвигается на этот размер. Поэтому generic load-path не требует отдельного
глобального «второго fixup-прохода»: основной FAT-цикл позднее пропускает
объекты, созданные при чтении ссылок. Точные source-level names и ownership
этого resolver-а остаются открыты.

Открыты: исходное имя secondary interface, точные signatures callbacks,
error/result enum, rollback, полный перенос relationship resolver-а и PC exact allocation.
Структура `[Class ID, SBOO]`, registry ownership и dispatch теперь закрыты;
подробности — в
[`spSerializerManager`](native-class-sp-serializer-manager.md).
