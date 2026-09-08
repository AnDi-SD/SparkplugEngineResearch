# PC/portable render mesh в общем tools core

Седьмой блок цикла до 07:30 МСК. Собственные C# readers E0/E1, вычисления
расположения VB, таблица byte offsets и дублирующие PC metadata readers удалены.
Общее ядро вьювера, экспортёров и редакторов использует восстановленные
`spMeshDataSerializer`, `spIndexBuffer`, `spVertexBuffer` и `spDXMesh`.
Интерфейс не перерабатывался. PS2 native packet metadata пока остаётся следующим
отдельным переносом; готовность всех typed decoders не заявлена.

## Одна цепочка чтения

Выделенный `spMeshDataSerializer::ReadBuffersForAnalysis` содержит существующую
ветку original PC429A40: прочесть и пропустить влияние пяти planning values,
прочесть настоящий IB, затем настоящий VB. Whole-field reader и прямой инспектор
поля используют один метод. Callback получает read-only наблюдения offsets,
field ID и planning values после успешной штатной инициализации mesh. Ни один
новый reader формата в C# или host bridge не добавлен.

Whole reader выбирает field1 при PC mask2; portable reader использует field0.
Пробный разбор альтернативного поля после ошибки удалён. Наличие обоих полей,
unknown field и повторный selected field обслуживает сам original-derived
whole reader. Инспектор может явно запросить отдельное поле через ту же ветку
чтения; это самостоятельная инспекция leaf, не fallback runtime load.

`RenderMeshView` — собственная обвязка приложения. Она создаёт настоящие классы,
вызывает их методы и копирует DTO. Статический CPU renderer/declaration cache
общий с ResourceGraph; COM/device/Windows legacy startup не вызываются.
Чтение отдельного mesh не включает combiner. Оригинальные CPU-классы с DX в имени
не заменяют наш OpenGL/Vulkan backend и не требуют DirectX.

`SmoVertexLayoutRegistry` сохраняет список editor presets, но все offsets и
stride получает из настоящего spVertexBuffer. Host mapping присваивает полям
DTO роли position/normal/color/UV/weights/bones. Значения normals не нормализуются
при чтении, отсутствующие веса не вычисляются. Преобразование strip в triangle
list остаётся общей операцией современного backend/экспортёров; исходные индексы,
primitive type и original primitive count доступны отдельно.

## ABI и ограничения потребителя

ABI2 дополнен mesh read/destroy/info/vertices/indices и vertex-layout запросом.
Whole stream и portable/PC leaf читаются ограниченно, до32 MiB. Режимы metadata
используют те же original readers/initializer и не проверяют optional attributes
по правилам отображения. SafeHandle владеет объектом до batch копирования DTO;
исходный pinned span не сохраняется.

Существующие managed editing DTO пока требуют UInt16 source indices и primitive
type triangle list/strip. Native reader сохраняет UInt32 buffers, но managed view
выдаёт явный отказ, пока соответствующие writers не перенесены. Modern attribute
view принимает отсутствие blend weights либо четыре authored weights; остальные
weight combinations дают явную диагностику. Эти ограничения принадлежат host,
не приписаны оригиналу. Metadata mode доступен независимо от такого attribute view.

## Отмеченный нестандартный случай: packed combiner byte size

В [старом досье](native-pc-dx-materialization.md) уже зафиксировано: packed
combiner path дважды добавляет12 bytes/vertex к native member64h; фактическая
аллокация при этом меньше. Существующий C++-срез сохраняет фактический размер.
Это расхождение **не исправлялось молча** и сообщено пользователю.

Последующее сопоставление исходного стенда и свежий `dx-packed` вызов подтвердили:
при **standalone** загрузке native member равен84, stride28, исходный stride16;
новый tools bridge совпадает. Ветка combiner сюда не подставлялась. Whole
ResourceGraph использует hook/combiner, однако новый typed mesh view получает
свой mesh через standalone original reader. Выдавать byteSize комбинированного
объекта за точное native member состояние пока приостановлено.

Рекомендация для затронутой ветки: после проверки всех относящихся веток разделить
в реконструкции original cached member и собственный размер физического storage.
Повторять original metadata quirk можно без копирования за пределами вектора;
современный backend должен использовать фактическую длину буфера. Неиспользуемое
сейчас чтение этого cached member не задерживает перенос нужных инструментам данных.

## Проверки

- Пять **свежих original PC / tools ABI сравнений** на одинаковых входах:
  dx-cross, dx-native, dx-both, dx-packed, dx-unknown. Original factories,
  actual buffer read/copy/declaration и штатное освобождение прошли; все
  отслеживаемые original allocations и COM fixture references освобождены.
  Вход содержит заведомо неверный planning header; геометрия определяется IB/VB.
  Сопоставлены indices, positions, normal/UV либо packed bone values, stride
  и размеры. Лимиты:64 KiB guest,100k instructions/2s на вызов,30s child.
  Это bounded COM fixture, не работа GPU и не whole-file original proof.
- C++ FullLoader: **213 checks** после выделения общего reader.
- C# FormatTests: **9 282 assertions** на PC меню, **2 957** на PS2 меню,
  **1 505** на bloom_jeans. Синтетика проверяет ignored planning header,
  неизменённую длину normal, actual offsets, отсутствие fallback, strip count,
  truncation и следующий успешный read. Старый PS2 fixture ошибочно использовал
  PC platform mask; исправлен fixture, не original selector.
- Whole resource graphs PC меню1 225/Bloom121 сохранили metadata и Node relations.
- C++ и C# контрольные сборки прошли; первый C++ compile потребовал добавить
  include spVertexBuffer, без изменения восстановленных алгоритмов.

Данные/логи локально:
`local-data/results/tools-core-cycle-20260909-0730/render-mesh/`.
Снимок: `research/tools-core-render-mesh-block-2026-09-09.json`.
Следующие нужные срезы — PS2 packet metadata и оставшиеся resource DTO/writers.
