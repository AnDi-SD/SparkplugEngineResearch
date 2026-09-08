# Аудит и очистка ядра Viewer, 2026-09-08

Проверены исходники Viewer, его зависимости и потребители общего Core.
Удалены второй C# SAN decoder/sampler и повторные общие методы; часть операций
подключена непосредственно к восстановленным классам. Операции записи вынесены
из зависимостей просмотрщика. **Полного отсутствия смысловых дублей ещё нет:**
оставшиеся случаи V-01/V-03/V-04/V-06 приостановлены по правилам пользователя.
Интерфейс не изменялся, выпуск пакета не выполнялся.

Игра остаётся источником истины. В этом изменении файлы `Sparkplug/` и `Winx/`
не менялись. Добавлен только прикладной мост к существующим методам.
Исторические CP138-досье и проверочный пакет 0.7.0 не переписывались.

## Что изменено

| Область | Общая реализация и результат |
| --- | --- |
| SAN: чтение, PRS, cubic, quaternion, поиск интервала | `spSerializerManager` → `spAnimationSerializer` → `spAnimation` / `spAnimTrack`. `SmoAnimationDecoder` теперь адаптирует данные C++; `SmoAnimationChannel` и его C#-математика удалены. Это распространяется и на Exporter. |
| Ключи, созданные инструментом | Передаются в существующий `spAnimTrack`; самостоятельной линейной интерполяции в C# нет. Нативная копия владеет ключами; канал удерживает владельца. |
| Привязка имён | Один `SmoAnimationBinding.SelectRoles` для scene adapter и снимков экспорта. Выбор первого конфликтующего именованного канала — явная политика инструмента, не реконструкция actor binding. |
| Заголовки полей SMO | `SmoDataBlockReader` вызывает `spDataBlockSerializer::ReadHeaderForAnalysis`. Мост читает закреплённую память без копии SMO и без выделения буфера на каждый заголовок. Метаданные raw inspector сохраняют закодированный ID terminator. |
| Локальная матрица узла | `SmoNodeTransform.LocalMatrix` вызывает настоящий `spNode`; отдельная композиция S/R/T удалена. |
| Матрицы skin | Статическая подготовка `SmoSceneBuilder` и анимированная сцена используют один `spSkin::ComposePaletteMatrixForAnalysis`. Выбор исходной bind-позы пока остаётся прикладным, см. V-04. |
| Повторные helpers | Проверка типизированной ссылки сведена в `SmoNodeDecoder.TryDecodeTypedRelationship` для LensFlare/Particle/Text. Сравнение путей находится в `LogicalAssetMatcher`. Неиспользуемые `NativeDebuggerHarness.PathsEqual` и `LinearDifference` удалены. |
| Запись и редактирование | Восемь файлов перемещены в `SmoViewer.Editing`: data-block writer, mutation transaction, leaf replacement, texture writer, placement writer, property schema/codec, output installer. Они нужны другим инструментам, но Viewer на Editing не ссылается. |

Зависимость направлена `Core → Sparkplug interop → C++`. Обратной ссылки
`Sparkplug interop → Core` больше нет. `SparkplugSceneRuntime` как адаптер SMO
находится в Core, сохраняя пространство имён API. Editing зависит от Core;
прямые потребители Editing подключены явно. Исходники операций не копировались.

ABI моста — **2**, платформа текущего адаптера — Windows x64. DLL теперь нужна
всем приложениям на общем Core. Обновлены документация сборки и `companionFiles`
в release manifest, включая вложенные инструменты suite. Это изменение будущего
состава пакетов, а не публикация или сборка нового релиза.

## Проверка всего состава Viewer

`research/ViewerCodeAudit` использует Roslyn из установленного SDK, читает только
исходники и строит граф `ProjectReference`. Реализации за пределами цепочки
Viewer не выдаются за код просмотрщика. Имена проверяются также у потребителей
общей библиотеки; кандидаты на удаление сопоставлены с XAML.

- Инвентаризация: **161 → 164** C#-файла в каталоге Viewer, включая другие
  инструменты и тесты. Это не число игровых классов.
- В зависимостях приложения: **118 → 113** C#-файлов, **34 816 → 32 141** строк.
  Основная часть уменьшения — перенос примерно 2,3 тыс. строк записи в Editing;
  перенос не считается удалением алгоритмов из всей системы.
- Группы полностью одинаковых тел методов от 60 токенов: **2 → 0**.
- Оставшиеся 57 private-кандидатов по единственному употреблению имени в C# —
  все обработчики XAML. Они сохранены.

Ноль совпавших тел не доказывает отсутствия одинаковой логики, записанной иначе.
Поэтому оставшийся код рассмотрен по назначению:

| Группа файлов / классов | Назначение и состояние |
| --- | --- |
| `SmoAnimation*`, `SparkplugAnimation*` | Общий C++ reader/sampler; C# хранит API, метаданные, binding policy и расписание выборок для целевых форматов экспорта. |
| `SmoDocument`, `SmoObjectFieldReader`, class/field/layout registries | Каталог, сырые поля, диагностика и таблицы для inspector. Заголовки полей уже C++; каталог и присвоение семантики ещё C#. Нельзя называть это готовым native loader. |
| `SmoNode*`, `SmoRenderNode*`, `SmoModel*`, `SmoSkin*`, binding/shared-instance resolvers | PRS/world/palette runtime подключён к C++. Чтение SMO-графа и выбор bind-позы остаются прежними, V-03/V-04. |
| `SmoMesh*`, `SmoTexture*`, vertex layouts, material decoders/resolvers | C# ещё читает содержимое ресурсов и выбирает материалы/текстурные связи. Вывод пикселей, vertex buffers и OpenGL — наш backend. Полная связь с native material/texture graph не завершена, V-03/V-06. |
| Bounding volumes, Collision, Partition/Octree/BSP/Zone/Portal, Navigation | Существующие C#-декодеры данных для inspector и вспомогательной геометрии. Перенос в общий граф требует поддержанных native владельцев и section readers; V-03. |
| Light/Fog/SkyBox/LensFlare/Particle/Text/UV/material controllers | C#-интерпретация полей пока сохраняется; таблица полей не равна исполнению оригинального класса. Повторный relationship helper объединён, сама миграция не объявлена завершённой. |
| `SmoAlpha*`, diffuse/UV/color analyzers, GUI classification | Диагностика и эвристики инструмента. Не доказательство поведения игры; V-01/V-06. |
| `SmoSceneBuilder`, picker; шесть файлов GUI | Подготовка входов renderer, камера, выбор, слои, диалоги и вызовы общего ядра. Локальные helper/bind-pose алгоритмы отдельно отмечены в V-04. |
| Четыре файла `Rendering.Wpf` | Общий OpenGL 3.3 backend, WPF thumbnails и viewport math. Это разрешённый современный платформенный слой; переносить старый DirectX backend не нужно. Texture frame selection требует V-06. |
| 24 файла `SmoNativeValidator.Core` | Отдельный инструмент проверки в игре, используемый существующей панелью. ОС/debugger/process/path/log код не является второй реализацией просмотрщика. Не удаляется вместе с панелью в задаче без изменения UI. |
| Corpus, Inspect, Editing, тесты | Отдельные потребители Core. Запись перенесена, но не переименована в реконструированные serializers. Исторические тестовые реализации не подключаются к приложению. |

## Нестандартные случаи и предложения

### V-01 — диагностические режимы связаны с UI: отложено

`SmoAlphaDecalDepthAnalyzer.ApplyOverlayLossSimulation` намеренно обнуляет alpha
как диагностическую симуляцию. Есть также эвристики прозрачных поверхностей и
импортированных run names. Это собственные инструменты диагностики, не игровой
renderer. Удаление существующих режимов меняет пользовательское поведение.

Пользователь уведомлён. Предложение: сохранить нужную диагностику в отдельном
модуле с явным назначением; решение об удалении режимов принять вместе с UI.
Не переносить эти эвристики в реконструкцию и не считать игровыми правилами.

### V-02 — неверные ограничения старого SAN-декодера: разрешено по игре

Два старых теста требовали отклонять одинаковые времена и повторные PRS-поля.
Выполнены ограниченные вызовы оригинального PC-кода:

- `probe_viewer_san_equal_times.py`: reader `43DB90`, sampler `479290`, времена
  `[0,0]`, `[0,0,1]`, `[0,1,1]`; 15 точек совпали с DLL точно.
- `probe_viewer_san_repeated_role.py`: целый field reader `43ECC0` принимает
  повторные поля перед именем. Два пустых поля дают пустой канал; два непустых
  дают значение последнего поля `(4,5,6)`.

Тесты исправлены по этим результатам. Код Sparkplug не подгонялся под C#.
Это конкретные синтетические случаи, не общее разрешение любого malformed SAN.

### V-03 — полная замена SMO readers упирается в неполный native graph: отложено

`spNodeSerializer::ReadNodeFieldsForAnalysis` прямо отклоняет `Collision`:
`spCollisionInfo ownership/payload is not reconstructed yet`. Есть также
границы derived section readers, resource ownership и platform texture inputs.
Пропускать поля, обрезать граф или подставлять мнимые объекты ради успешной
загрузки запрещено. Старые readers поэтому ещё остаются видимым техническим
долгом; они не названы тонкими обёртками готового native loader.

Предложение: выделить минимальный набор владельцев/полей для просмотра,
восстановить недостающее по игре и отдать приложению read-only представление
реальных ресурсов через один мост. Сначала закрыть Node/collision ownership,
затем geometry/skin/material/texture graph. Переписывание ради упрощения или
скорости — только отдельным обоснованным предложением с проверкой по игре.

### V-04 — вспомогательные объекты и bind-поза содержат эвристики: отложено

В `MainWindow.xaml.cs` auxiliary roles выбираются по индексам
`85/88/91/92/95/119/120`, control/service markers — по именам. Локальный
`TryResolveNodeWorldMatrix` имеет fallback «поле 0 размером 12 — позиция»,
не привязанный к доказанному классу. В Core похожий обход использует
inverse-bind matrices и физическую вложенность как дополнительный источник.
Такой обход не равен `spNode::UpdateWorldForAnalysis` с раздельным наследованием
position/orientation/scale. Само обращение локальной матрицы и skin-композиции
к C++ не устраняет это различие.

Пользователь уведомлён. Предложение: сохранить raw inspector, убрать фиксированные
номера из семантического определения объектов и получать helpers по подтверждённым
классам/ссылкам. Отдельно определить, где инструменту нужна bind-поза, а где authored
world pose; последний путь получать целиком из native scene. Не менять молча
состав слоёв и не выдавать предположения за классы движка.

### V-05 — NaN в cubic coefficients: исследовано, защита сохранена

Старый тест ожидал конечную позицию при шести NaN в служебных коэффициентах
одного packed cubic ключа. Вызов оригинальных `43ECC0` / `479290` через
`probe_viewer_san_repeated_role.py --unused-coefficients` дал `(NaN,NaN,NaN)`
при установленном флаге позиции. Сырые биты каждого компонента — `7FC00000`.

Существующая portable-проверка всех входных чисел на конечность сохраняется.
Это защитная граница адаптированной реализации; не утверждение, что игра сама
отклоняет файл. Восстановленный исходник не менялся, старое ожидание исправлено.

### V-06 — texture sequence и material heuristics: отложено

`SmoTextureBindingResolver.TryDecodeTextureSequence` группирует кадры по похожим
именам, берёт последнее время из сырых смещений и при неудаче назначает 8 fps.
GPU adapter затем равномерно делит время на длительность кадра. Между тем
реальный `spTextureTrack::EvaluateForAnalysis` выбирает по массиву конечных времён:
ранние границы исключительные, последняя включительная. Наличие C# decoder
`SmoAnimTextureControllerDecoder` рядом не означает, что этот путь его использует.

Пользователь уведомлён. Предложение: после подтверждения material/controller
ownership передавать реальные ссылки и времена в `spAnimTexController` /
`spTextureTrack`, а backend должен получать выбранную текстуру. Fallback по
именам/8 fps не считать правильной игровой анимацией; его судьбу решить явно.

## Проверки и воспроизведение

Все проверки адресные; полного прохода по тысячам файлов не выполнялось.
Компиляция — максимум два процесса, offline restore из локального NuGet cache.

| Проверка | Результат |
| --- | --- |
| Пять выбранных существующих C++ suites | Все прошли |
| Shared SAN | 2 686 проверок; 195 оригинальных PRS-поз; max error `2.3841858e-7`; финальный запуск около 0,13 с |
| Формат, синтетические contracts и выбранный Bloom SMO | 1 472 проверки |
| ABI, владение, seek/rebind, 6 реальных SMO | 2 273 проверки; около 0,76 с; peak working set 49,2 MB |
| Настоящие скрытые обработчики окна, OpenGL host | 8 359 проверок; 43 позы |
| Независимый NumPy FK по сохранённым PC PRS | 419 mesh/time сравнений; все в прежнем допуске; max error `0.000556154209` |
| NativeValidator helpers/guards | 296 проверок; игра не запускалась |
| Контрольная компиляция потребителей | Viewer/GuiTests, InteropTests, FormatTests, Inspect, Exporter, Importer, SmoLVLcreator, TextureTool — успешно |

GUI-проверка и NumPy comparison проверяют реальные обработчики и CPU mesh mirror;
это не проверка каждого GPU-пикселя, всех эффектов игры или системных диалогов.
Source inventory отдельно подтверждает отсутствие ссылки Viewer на Editing.

Локальные отчёты лежат в `local-data/results/viewer-core-audit-20260908/`;
логи — `.codex-tmp/viewer-audit-*`. Компактный проверяемый индекс и результаты
новых original-PC probes — `research/viewer-core-audit-2026-09-08.json`.
Исходные данные старых проверок берутся из фиксированных shared-san-v1 и
viewer-sparkplug-core-20260908 manifests, их SHA проверяются самими стендами.

```powershell
# После обычной сборки соответствующего проекта:
dotnet research/ViewerCodeAudit/bin/Release/net8.0/ViewerCodeAudit.dll . local-data/results/viewer-core-audit-20260908/recheck-inventory.json
dotnet tools/SmoViewer/SmoViewer.FormatTests/bin/Release/net8.0/SmoViewer.FormatTests.dll --san-keys local-data/results/tool-cycle-20260908-1900/shared-san-v1/input.json local-data/results/viewer-core-audit-20260908/recheck-san.json
dotnet tools/SmoViewer/SmoViewer.Sparkplug.Tests/bin/Release/net8.0/SmoViewer.Sparkplug.Tests.dll local-data/results/tool-cycle-20260908-1900/shared-san-v1/input.json local-data/pc-pristine/Media
```
