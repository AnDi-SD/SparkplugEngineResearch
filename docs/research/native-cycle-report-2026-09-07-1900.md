# SMO/SAN: отчёт цикла до 19:00 МСК, 7 сентября 2026

Срез на момент закрытия CP98. Последующее снятие ограничения heap для двух
lit first-generation сценариев описано [отдельно](native-research-strategy-2026-09-07.md)
и не входит в суммы этого цикла. Пути `local-data/` и `.codex-tmp/` в отчёте —
локальные артефакты; игровые файлы и сборки не публикуются в Git.

За цикл восстановлены и связаны новые части PC-логики загрузки, анимации,
скиннинга, материалов, освещения и записи геометрии. Главный результат —
проверенные цепочки от прочитанных SMO/SAN-данных до исходного draw, а также
экспорт MeshData из восстановленного C++, который принимает original PC reader.

Период: 07:48–19:00 МСК. Исследовательские срезы **CP51–CP98**:
**419 точных сравнений original/source, 11 476 native assertions**;
отдельно четыре native-only граничных сценария с 38 assertions.
Повторные регрессии в эти суммы не включены. Это число проверенных сценариев,
а не доля восстановленных инструкций или готовность всей игры.

## Что теперь воспроизводится

**RFX и создание шейдера при первом обращении.** Восстановлены parser,
effect template, переменные и константы RFX, обработка XML-событий,
формирование shader source, reflection-параметры и заполнение cache после
компиляции. Проверены настоящие `Fixed.rfx` и `Bumpmap.rfx`; для большого XML
явно отделена внешняя библиотека декодирования. Небольшой RFX прошёл связную
цепочку file → original metadata/regex → XML callbacks → compile boundary.
Первый draw вызывает генерацию; повторный использует созданный shader.
Подробности: [RFX pipeline](native-pc-rfx-pipeline.md),
[shader compile](native-pc-shader-compile.md),
[корпус RFX](native-pc-rfx-corpus.md).

**SAN управляет тем же Skin, который затем рисуется.** Прочитанный `bbush.san`
поступает в actual Actor/AnimationManager; tick меняет ту же кость, которая
участвует в palette и draw. Проверены четыре времени, clone/alias ownership,
Start при ограниченной ёмкости, fade и отложенный Stop. Последовательно
добавлены scene world update, actual decoded mesh, собственный материал Skin,
fog, текстура и первая генерация shader. Каждый следующий сценарий сохраняет
объекты предыдущих стадий там, где заявлена связная цепочка.
Подробности: [SAN/Skin](native-pc-skin-san-render.md),
[scene/mesh](native-pc-skin-scene-mesh-render.md),
[материал](native-pc-skin-owned-material-render.md),
[fog](native-pc-skin-fog-generated-render.md),
[текстура и генерация](native-pc-skin-texture-generated-render.md).

**Освещение из настоящих SMO-записей.** Девять неизменённых компактных Light
записей представляют девять выбранных форм полей корпуса. Восстановлены
Light/Data readers и writers, world/dirty-bit протокол, обновление scene light
cache и выбор источника для RenderNode. Этот cache передаёт тот же DXLight
в Skin draw и восемь shader constant rows. Проверены directional/point/spot,
ambient, disabled, shadow-фильтрация и отказные device/post-callback варианты.
Девять Light/Node-графов прошли index → write → repeat/null → fresh reference
read; aliases после загрузки указывают на канонические объекты.
Подробности: [корпус света](native-pc-light-corpus.md),
[selected light → Skin](native-pc-skin-selected-light.md),
[граф при сохранении и чтении](native-pc-light-graph-roundtrip.md).

**Alpha queue и геометрия.** Воспроизведены постановка Skin в очередь,
ограничения ёмкости/флагов, сортировка и flush, включая изменение очереди
в callback. Отдельная связка исполняет enqueue → whole flush → тот же Skin
с прочитанной сеткой. Исправлены PC bounds: sphere использует все вершины,
а AABB — native primitive-count/u16 traversal, в том числе при u32 indices.
Это наблюдаемое поведение PC, не исправленная геометрическая формула.

**Полная локальная запись MeshData.** Выполнены оба original writer:
общий `42B170` и DX `429EA0 → 013BC5E0`, по 30 точных сравнений.
В C++ добавлены virtual writers с существующим SerializerManager policy.
Общий writer в режиме 1 пишет только terminator. DX writer сохраняет native
поле всегда, а cross-поле — в режимах 0/2. Planning header учитывает
расширение packed component20, но payload остаётся упакованным.
В 10 дополнительных цепочках именно source-written bytes принял whole
original PC reader; совпали все vertex/index bytes и DX-метаданные.
Подробности: [DX writer](native-pc-mesh-writer.md),
[общий writer](native-pc-mesh-base-writer.md),
[original принимает экспорт](native-pc-mesh-writer-roundtrip.md).

Последний срез подключил оба mesh writer к общему reference protocol:
actual indexing выделяет один ID, затем сохраняются header/payload, FAT
metadata, повторная и null-ссылки. Четыре exact-сравнения подтвердили весь
этот путь; source MeshReaderTests —346/346.
[Mesh graph writer](native-pc-mesh-graph-writer.md).

## Исправления, найденные сравнением с EXE

- Matrix → quaternion преждевременно округлял промежуточные значения.
  Перенос подтверждённых промежуточных операций в double восстановил точные
  биты выбранных случаев, включая реальную SMO-запись Light.
- Constructor Light должен выставлять dirty bit 8. Без него ambient light
  с пустой Node-секцией не проходил правильное первое обновление.
- Scene light selection зависит от флагов после Node update: перемещение
  может обновить DX payload, сохранив предыдущий selection cache.
- Light writer пропускает false Enabled; fresh constructor включает свет
  снова. Read/write cycle воспроизводит эту особенность оригинала.
- Material mode, сохранение renderer state byte, packed Skin color,
  invalid fog type и texture/declaration lifetime согласованы с original
  порядком обращений и возвратами, включая игнорируемые HRESULT.

## Изменение учёта

Знаменатель не менялся: **279 PC / 245 PS2 класса** в workflow-v2.
У каждого класса равный вес; неподтверждённые классы остаются в знаменателе.

| Область | До цикла | После цикла |
|---|---:|---:|
| PC SMO/SAN workflow, 279 классов | 44,1075% | **45,1935%** |
| Оценённые PC-классы workflow | 187 | **190** |
| PC весь каталог, 733 класса | 16,9045% | 17,3179% |
| PC движок, 329 классов | 37,3891% | 38,3100% |
| Исторический прямой набор SMO/SAN, 37 классов | 60,8108% | 61,2162% |
| PS2 workflow, 245 классов | 31,1633% | 31,1633% |

Прирост PC workflow — **1,0860 процентного пункта**. Изменены оценки 25
классов; для трёх ранее неоценённых PC-классов появились первые оценки:
Parser, PCEffectTemplate, PCRFXFileLoader. PS2 в этом цикле не исследовался.
Полный список до/после находится в
[машинной сводке](../../research/native-cycle-summary-2026-09-07-1900.json).

Обязательные критерии полного восстановления: **0/7 завершены, 6 частично,
1 открыт**. Ни один класс не объявлен полностью закрытым. Рост процентов
отражает накопленные свидетельства; его нельзя переводить в оставшиеся часы
или готовность приложения.

## Проверка и воспроизводимость

- Полная C++ сборка успешна; **CTest 61/61**, 47,95 секунды.
- **61/61** тестов учёта, scope и bounded runner. Исправлена устаревшая
  проверка конкретного первого элемента очереди: теперь она проверяет
  разделение PC/PS2, а отдельный тест — порядок зависимостей.
- Заключительная проверка: **8 профилей / 103 exact cases**, все успешны:
  selected light, texture/generated Skin, Light graph, RFX pipeline,
  оба mesh writer, writer/original-reader roundtrip и mesh graph writer.
  Здесь99 повторных сценариев и4 новых CP98; повторные не прибавлены к419.
- Все **48 импортированных манифестов цикла** совпали с сохранёнными хешами;
  94 различных evidence paths существуют. Текущие хеши доказательств для
  классов этого цикла совпали. Три исторические ссылки указывают на карточки,
  дополненные после фиксации старого хеша; исторические манифесты не переписаны.
- Scope audit: **81** записанная пара class/platform, отсутствующих членов
  и неверных catalog references — **0**.

В итоговой арифметике исправлена ошибка промежуточного cumulative итога:
CP51–61 дают 861 assertions, а не 961. Итог 11 476 получен суммой всех 48
строк `countsByCheckpoint`; локальные результаты и число сравнений прежние.

Original EXE SHA256:
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Пределы не повышались: 100000 instructions / 2 секунды на original call,
30 секунд на дочерний процесс, 64 KiB arena, 32 KiB на engine allocation.
Оригиналы EXE/ресурсов не изменялись; игра и настоящий GPU не запускались;
публикации и коммитов не было.

Воспроизведение основных последних проверок:

```powershell
python research/native_workbench.py run pc-mesh-writer
python research/native_workbench.py run pc-mesh-base-writer
python research/native_workbench.py run pc-mesh-writer-roundtrip
python research/native_workbench.py run pc-mesh-graph-writer
python research/native_workbench.py run pc-skin-selected-light
python research/native_workbench.py run pc-skin-texture-generated-render
python research/audit_native_scope_dependencies.py
python research/audit_native_cycle_20260907_1900.py
```

Точные report paths и результаты каждого child сохранены в машинной сводке.
Хеш wrapper script в историческом run report не заменяет хеши всех его
транзитивных helpers или исторической C++ сборки.
Отдельный [снимок текущих файлов](../../research/native-cycle-fingerprints-2026-09-07-1900.json)
фиксирует конечные C++/Python sources и четыре использованных test binaries.
Он относится к завершению этого цикла, не к историческим прогонам.

## Что ускорило исследование

Адреса исполненных инструкций и runtime jump targets использовались для
нахождения relocated protected bodies без повторного полного анализа EXE.
Общие fixtures позволили добавлять следующую original-стадию к уже проверенной
цепочке, сохраняя её объекты и реальные ownership-переходы.

Для тесных render-сценариев применены повторное использование освобождённых
блоков, подтверждённые моменты освобождения временных входов и отдельная
явная страница внешнего COM-интерфейса. Общий лимит engine arena сохранился.
Граничные прогоны без успеха зафиксированы отдельно; увеличения лимита или
подстановки успеха внутренних engine-вызовов не было.

Для корпуса выбирались неизменённые представители разных форм полей с
проверкой file/slice SHA256. Независимые регрессии запускались параллельно,
сборка — по затронутым целям и затем полностью для общего CTest.

## Что остаётся следующим

1. Замкнуть полный native FFPS save: producer FAT/file-index, экспортные
   registrations, offsets и все графовые связи. Сейчас подтверждены локальные
   payload/reference writers и отдельные roundtrips.
2. Получить whole-file оригинальные загрузки сложных textured/skinned/level
   SMO с внешними зависимостями, расширив проверенные семейства ресурсов.
3. Совместить уже прочитанный и выбранный свет с первой генерацией shader
   в одном whole draw; аналогично — alpha queue с первой генерацией.
   Последние корректно ограниченные попытки упёрлись в размещение памяти
   при неизменном лимите64KiB. Успеха этим ветвям не приписано:
   [lit generation boundary](native-pc-skin-decoded-light-generated-boundary.md),
   [queued generation boundary](native-pc-skin-queued-generated-boundary.md).
   Следующий ограниченный эксперимент: валидный shader fixture с reflection
   только реально используемых констант выбранного light-варианта, затем
   отдельно уже поддержанный assembly-template. Это гипотезы уменьшения
   временных allocations, ещё не успешные original-прогоны; размер arena
   и сохранность живых engine-объектов должны остаться прежними.
4. Продолжить lifetime/error/device-loss и произвольные графы; отдельно
   настоящий display. Наблюдённые library/COM/stream границы остаются явно
   заданными fixtures. Выбранные точные x87 результаты не являются общей
   эмуляцией 80-битной арифметики.

Полная хронология: [журнал цикла](../../journal/2026/2026-09-07-pc-smo-san-1900.md).
Учёт и provenance: [JSON-аудит](../../research/native-cycle-audit-2026-09-07-1900.json),
[JSON-сводка](../../research/native-cycle-summary-2026-09-07-1900.json).
