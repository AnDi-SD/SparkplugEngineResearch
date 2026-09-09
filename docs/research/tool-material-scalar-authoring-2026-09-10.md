# Общая запись скалярных полей Material и пакетная подготовка ветвей

Четвёртый блок цикла до 07:00 МСК. Переносятся два существующих сценария
Importer: состояние материала создаваемой skinned-ветви и texture-alpha
состояние rigid материалов. UI не меняется. Это частичная запись Material,
не завершение всей перестройки графа или original whole-file Save.

## Источник логики

`spMaterialSerializer` выделяет три части уже восстановленного writer:
`WriteRenderStatesFieldForAnalysis`, `WritePassBlendFieldForAnalysis` и
`WriteTextureStatesFieldForAnalysis`. Полный writer вызывает эти же методы.
Они пишут actual Material/Pass/MaterialTexture через общий DataBlock serializer:
field0 — 11 слов, field3 — одно, field17 — первые девять texture states.
Слоты 9..11 host holder в PC поле не входят.

Подтверждение: [PC scalar](native-pc-material-scalar.md),
[standard graph](native-pc-material-standard-graph.md), адреса
`476BDC..476C2D`, `476CB1/476CC2`, `476270..476286`. Два прежних sealed
original capture дают точные сравниваемые byte slices. Новых запусков
оригинала и исправлений игровой семантики в этом блоке нет.

Тот же actual material reader дополнительно наблюдает scalar assignments:
payload-relative offset/size, порядок, actual pass/layer/holder и признак
применения. Orphan 8/17 наблюдается как пропущенный; повторные присваивания
сохраняют исходный порядок и реального получателя. Это host observation,
не выдуманные члены оригинального класса. Указатели borrowed и действуют
в пределах жизни неизменённой структуры inspected material.

## Граница редактирования

`spv_material_patch_scalars` читает полный bounded field stream в actual
`spDXMaterial`, меняет нужные значения его setters и получает payload от
общих writer slices. Host копирует эти payload в наблюдённые места. Исходные
заголовки, порядок, неизвестные поля, references, цвет и terminator сохраняются.
C# `SmoMaterialScalarWriter` возвращает объект прежнего размера; object header
также остаётся исходным. Промежуточная C# копия целого field stream не нужна:
native output копируется сразу в результат после восьмибайтового header.

Single-standard-layer сценарий требует ровно один MRS, один pass, один actual
StdLayer и одно применённое поле17. Legacy8, orphan/повторный17, другая форма
слоёв или отсутствие нужных полей дают `MATERIAL_SCALAR_SHAPE`. Это ограничение
операции инструмента; допустимость такого материала в игре не отрицается.
Rigid сценарий сохраняет прежний scope: заменяются все MRS assignments и blend
**каждого** actual pass, texture states остаются прежними.

`BuildMaterial` больше не пишет 0/3/17 самостоятельно. Замена texture references
и изменение размеров inline branches остаются следующим отдельным участком.
Rigid caller также удалил свой scalar encoder; существующая проверка результата
общим inspector сохранена.

Отдельный случай сообщён пользователю: original DX factory не инициализирует
power+B8. Полный writer без подтверждённого значения по-прежнему отказывает.
Три используемых writer slices этого значения не читают и не подставляют default.
Полное пересохранение таких материалов остаётся отложенным; неизвестные поля
и редкие header forms не переписываются канонически под видом lossless save.

## Ускорение host-подготовки

`SmoFileReferenceTrace.CaptureRanges` проверяет принадлежность документа и SHA
всего исходника один раз на синхронный пакет. Каждая ветвь сохраняет отдельные
coverage/reference checks и собственный content hash. Для N ветвей убрано
повторное чтение N полных файлов ради одинакового SHA; global mutable cache
не добавлен. Как и одиночная операция, API требует неизменяемый owned buffer.

`SmoAdditiveForestPlanner.Create` собирает и сортирует операции, затем вызывает
пакетный API один раз в соответствующем порядке. Сортировка также использует
уже построенный индекс объектов. Это host-оптимизация, не замена игрового reader.

## Проверка этапа

Native сборка и пять направленных suites прошли: MaterialSerialization653,
MaterialController488, FullLoader213, DataBlockWriter340, ReferenceReadTrace61.
Пять consumer projects собраны без ошибок/предупреждений. ABI — четыре
успешных случая с идемпотентностью и 33 проверки отказа; повторно проверены
21 dependency SHA прежних original captures и pristine EXE. Исходные MRS,
blend и LTS bytes совпали с общими writer slices. Частичная запись при ошибках
потока в оригинале не исследовалась: прежняя host bulk запись не выдаётся за
её эквивалент.

Managed scalar integration — 44 checks: multipass, повторные MRS, orphan/
legacy/repeated LTS, nonminimal headers и посторонние byte sentinels; отдельный
шаблон `vase.smo`, Material8, сохраняет в том числе inline Texture. Actual rigid
caller меняет только требуемые payload. End-to-end `CleanSkinnedTargetRegression`
на pristine Icy также прошёл через BuildMaterial (13 checks): результат имеет
101 объект и 13 110 байт. Исходные игровые файлы не изменялись.

Reference remap — 60 checks: к прежним 42 добавлены две разные ветви,
обратный порядок ranges, single/batch metadata и remap equality, отказ при
изменении owned source **вне** копируемых диапазонов. Общий FormatTests — 647.

На pristine Alfea02 (7 106 313 байт) 20 небольших covered extents дали точное
совпадение metadata. В пяти чередующихся раундах по десять повторов median
single capture — 94,866 мс, batch — 5,585 мс: **16,99 раза** именно для подготовки
этих ranges. Hash input уменьшился с 142 126 260 до 7 106 313 байт на операцию;
range hash/coverage остались. Замер целиком занял 5,743 с, process peak working
set — 58,30 МиБ. Порог скорости не является тестовым assertion; результат
не экстраполируется на полное время импорта или большие тяжёлые ветви.

Результаты записаны в новый каталог
`local-data/results/tools-core-cycle-20260910-0700/authoring-scalars/`.
Артефакты трёх предыдущих блоков не изменяются.
