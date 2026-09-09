# StaticRenderObject — общий reader/writer и render support

Первый блок дневного цикла9 сентября до19:00, после17 блоков прошлого цикла.
PC EXE SHA256 `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

## Оригинальный контракт и прежняя ошибка приложения

PC factory41A7C0 создаёт StaticRenderObject10C с родителем NamedObject.
Matrix4 startup6D38C0 инициализирует общую identity, которую конструктор копирует
в обе собственные матрицы8C/CC. Serializer factory44FCE0 создаёт14h; его secondary
interface содержит **writer450140, reference indexing44FE30, reader44FE90**.
44FE30 не является header/factory reader; это уточнено по телу функции.

44FE90 field1/2 читает ровно64 байта через416E30 и независимо присваивает
world/inverse через41D330. Нет проверки affine shape, связи матриц или фиксированного
порядка. Повторные поля заменяют предыдущее значение; неизвестные пропускаются.
При отсутствии обоих полей остаются constructor defaults. Field0 читает reference
семейства Renderable, отвергает null и вызывает общий append469ED0 для каждого
вхождения. Writer всегда выдаёт field1, field2, затем все references в порядке
списка с size reservation7 и terminator. Indexer обходит тот же список.

Прежний C# decoder требовал четыре поля в порядке1/2/0/terminator, один inline
Model, affine-матрицы и одну из двух предполагаемых inverse conventions.
Это были ограничения приложения, не доказанный контракт игры. Они удалены из
чтения матриц; восстановленная игровая арифметика под них не менялась.

## Общая реализация

Добавлены `spStaticRenderObject` и `spStaticRenderObjectSerializer`. Reader,
writer, reference indexing и literal matrix field helper общие; C# вызывает
helper через additive ABI2 `spv_static_matrices` (132-byte output,12-byte descriptor).
Source принимает независимые матрицы; точная длина поля, границы stream и
явное владение resolved object проверяются host envelope.

Общая embedded support implementation не имеет восстановленного type name.
Поэтому `Analysis/PC/spRenderSupport.h::RenderSupportForAnalysis` обозначен
аналитическим именем без RTTI. Из RenderNode в него выделены прежние append,
sphere rebuild и host transfer/clear helpers; StaticRenderObject использует
те же методы. Формулы sphere и clone RenderNode не менялись. Static clone
по оригиналу наследует только NamedObject: матрицы и renderables не копируются.
Присваивание матрицы само по себе не обновляет cached sphere, append обновляет.

В C# остались DTO/metadata resolution. Список `Renderables` сохраняет порядок
и повторения sized/inline ссылок; поддерживаемые metadata consumers — Model/Skin.
Это host-граница, не ограничение оригинального Renderable family. Исторический
corpus report имеет явное single-renderable представление. Общий loader
зарегистрировал настоящий StaticRenderObject; Scene/Zone/PartitionSystem и
полная загрузка уровня этим блоком не заявлены.

Старый C# inverse authoring helper **перемещён без изменения** из decoder в
Editing и переименован `CreateLegacyStaticInverseTransform`. Его вызывают прежние
редакторские операции Viewer/Importer/LVLcreator. Он больше не выдаётся за
восстановленный метод игры и не участвует в чтении. Дальнейший перенос этой
редакторской политики отложен пользователем до работы над LVLcreator; новая
альтернативная реализация здесь не вводилась.

## Проверки и границы

`probe_pc_static_serializer.py`:5 свежих original-PC случаев — defaults,
world-only, independent inverse, non-affine, unknown/repeated. Полные scalar
reader/zero-renderable writer, literal matrix bytes и owning teardown прошли.
Renderer представлен только явным нулевым cache storage0xCA00 внутри micro
arena64KiB; constructor/device не исполняются.100k instructions/2s на вызов,
30s внешний процесс. Это не полная игра или GPU.

`validate_tools_static_reader.py` проверяет dependencies сохранённого original
результата и сравнивает его с source reader/writer и C ABI. Все5 matrices/output
совпали побайтно,3 host descriptor refusals прошли. Дополнительно host graph
загрузил один настоящий StaticRenderObject из test-only FFPS envelope; это
проверка регистрации, не новое original whole-file proof.

C++: StaticRenderObject52, RenderNode34, FullLoader213. Проверены duplicate
ownership, независимые matrices, Named-only clone, cached sphere, повторные
references и отказы null/неподтверждённого host owner. Viewer/Importer/LVLcreator
CoreTests проекты скомпилированы без предупреждений/ошибок. C# synthetic cases
проверяют defaults, non-affine/repeated/unknown, null, короткое поле и repeated
sized references. Реальные файлы и итоги перечислены в `managed-samples-final.json`.
Финальные pristine samples: Alfea01 —37095, Alfea02 —36320, PC menu —9324,
PS2 menu —2999, Icy —1571 assertions; все прошли.

При подборе выборки были две ошибки setup: одноимённый файл вне индексированного
корпуса (`local-data/Winx Club/.../Alfea02.smo`,116890479bytes) превысил прежний
host limit64MiB; working-копия Alfea02 не подходит тесту, закреплённому за SHA
pristine-копии. Никаких исходников/ожиданий ради этих входов не меняли. Финальная
выборка взята из `pc-pristine/Media`; большие сторонние копии остаются отдельной
границей host input budget. Эти неудачные логи сохранены отдельно.

Артефакты: `local-data/results/tools-core-cycle-20260909-1900/static-reader/`.
Снимок: `research/tools-core-static-render-object-block-2026-09-09.json`.
UI не перерабатывался; PS2 metadata sample не означает исполнение PS2 EXE.
