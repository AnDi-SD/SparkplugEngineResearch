# Общие классы и читатели навигации

Блок 17 цикла до 19:00 МСК 9 сентября. Навигационные ресурсы загружаются
в реальные восстановленные классы; инспекторы используют тот же ResourceGraph,
что и модели, материалы и Node. Алгоритмы движения, поиск маршрута и authoring
writers этим блоком не объявляются готовыми.

## Подтверждённые классы и методы

| Класс | Class ID | База | PC factory / reader |
|---|---|---|---|
| spNavigationGraph | 188A161F | spRenderNode | 41A6A0 / 446280 |
| spNavigationSet | 74F9013E | spNode | abstract, без factory / 448800 |
| spMeshNavigationSet | 7297173C | spNavigationSet | 41A700 / 448FE0 |
| spNavigationPortal | 385662AA | spNode | 41B2E0 / 447750 |

Serializers имеют собственные подтверждённые RTTI: 08703965, 5C2C4113,
050A5628, 33695A39. Graph наследует существующий RenderNodeSerializer,
а MeshSet — NavigationSetSerializer. Отдельных C# реализаций этих readers нет.

Оригинальные читатели сохраняют повторяющиеся ссылки и их порядок. Graph
записывает byte ordinal в Set/Portal; повторная ссылка меняет индекс того же
объекта. При этом добавление Portal в Graph **не устанавливает portal.graph**.
Ссылки Graph/Set/Portal заимствованные, без увеличения refcount. Portal graph
и endpoint pointers конструктор оставляет нетронутыми: portable backing
содержит явные флаги известности, а не выдуманные значения по умолчанию.

Матрица NavigationSet — оригинальное не-RTTI двухбитное хранилище
447BE0/447C60/447CA0. CRT initializers 6D31E0/6D3200/6D3230 задают маски и
сдвиги. Allocation stride равен ceil(columns/4), но индекс элемента линейный
`row*columns+column`, без межстрочного padding при доступе. Записывается
`value & 3`. Контроль 3×3 дал packed `e43c02`; native getter и общий класс
вернули одинаковые значения. Соседи сохраняют дубликаты, enabled — сырой byte.

Graph создаёт ячейки с nextPortal=255 и пустыми alternatives. Reader не
доказывает связность маршрутов, не требует reserved=0, равенства числа sets
размеру таблицы, строгого порядка полей или полного набора path records.
Старые C# проверки этих условий удалены из загрузки. Исторические совпадения
в корпусе остаются результатом анализа данных, а не правилами игры.

MeshSet::SetMesh 444010 использует существующий spMeshBV и embedded
spCollisionInfo. Позиции преобразуются обратным текущим Node PRS. Min/max
сохраняют накопленные границы при повторной привязке. Sphere определяется
первой строго более далёкой парой текущего вызова; пустой или совпадающий набор
сохраняет предыдущую sphere. Исходный O(V²) обход сохранён. Identity и
position(1,2,3)/scale(2,-3,4) совпали с оригиналом по всем raw float bits.

## Подключение инструментов

Native bridge копирует navigation state из уже загруженных объектов в закрытый
JSON DTO, кешируемый на время жизни ResourceGraph. Это транспорт между языками,
не игровой serializer. Матрицы читаются исходным getter, bounds/sphere передаются
как UInt32 bits. JSON не содержит алгоритмов навигации или разбора SMO.
SmoLoadedResources снимает данные до освобождения того же native handle.

Три прежних C# reader заменены тонкими проекторами. Идентичность ссылок приходит
из native objects; encoding/inline size для инспектора читаются существующим
spSerializer prefix reader. Добавлена bounded sequence inspection для двух
ссылок Portal. Неполные Portal/Set могут существовать в native graph; старый
полный inspection DTO явно сообщает, если не способен их представить.
Навигация по-прежнему не означает запуск gameplay scheduler.

## Проверки

- Шесть original-PC reader captures: Graph/Set/Portal, empty и values.
- Три original-PC relationship captures и два состояния привязки MeshBV.
  Все 11 результатов совпали с общим C++; нативные allocations освобождены.
- Шесть scalar snapshots через публичный ABI совпали с теми же original
  captures. Sequence prefix: 3 положительных случая и 3 guards.
- Gardenia02, RedF01, battle_02: 10 228 объектов, 13 MeshSet, 3 Graph,
  10 Portal, 20 623 matrix cells; 21 533 managed checks.
- ABI проверяет canonical IDs, индексы Graph, наличие всех actual classes,
  стабильность кеша и четыре случая неверного буфера на каждом из трёх уровней.
- Native FullLoader 213, ReadReference 298, NavigationSerialization 34;
  пять managed consumer builds успешны. Прежний synthetic navigation fixture
  теперь проверяет принятие raw selector/diagonal values без выдуманного
  контроля маршрута. Контрольная сборка не является релизом.

DLL блока: `6E2E7BB98869EA661482ACCC15D84F4FCF30BA535B4E8B10CB1E87AB8356C0F5`.
Локальные результаты: `local-data/results/tools-core-cycle-20260909-1900/navigation-readers/`.
Состав и SHA256 фиксируются в `research/tools-core-navigation-block-2026-09-09.json`.

## Ограничения и отдельные случаи

**NAVIGATION_TABLE_REPLACEMENT:** original repeated populated Graph resize
в ограниченной проверке дошёл до duplicate release. Общий reader явно
отклоняет повторный field2 после создания непустой таблицы. Без подтверждения
нельзя заменять это произвольным сохранением или очисткой. Пользователь уведомлён.
Оригинальный ошибочный ввод не повторялся.

Host limits: Graph table до 256, Set nodes до 4096, degree до 255, matrix
до 1 048 576 cells, MeshBV binding до 4096 vertices; конечный inverse и
до 16 MiB inspection JSON. Эти guards не выдаются за условия оригинала.
Полные clone/copy, index/write и navigation routing runtime ещё недоступны.

`test_world_navmesh.smo` остановлен на TextureData ID5/cliff_01 внутри Model,
до навигации; source-only texture wrapper исследуется отдельным блоком.
Диагностика inline reader дополнена ID/class, SectionCursor сохраняет первую
ошибку. Поведение принятия файла не менялось. BMS_02 всё ещё требует полноценный
OcclusionVolume::Init; его capped оригинальный вызов не подменялся success.
MaterialColorController/VisibilityManager остаются прежними зависимостями.

Новых PS2 executions и полного corpus scan в этом блоке не было. Более ранние
PS2 structural proofs сохранены; свежая проверка реконструкции выполнена по PC.
