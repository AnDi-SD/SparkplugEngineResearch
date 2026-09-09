# Начальная поза Exporter из actual Node runtime

Блок 12 цикла 9 сентября до 19:00. Удалена отдельная реализация игровой
иерархии в Exporter: собственное рекурсивное PRS-перемножение, подстановка
inverse-bind world, fallback identity при цикле/отсутствующем Node и физический
parent вместо загруженного отношения.

Экспортный skeleton получает весь actual Node snapshot SmoLoadedResources,
включая наследников. Game/engine classes не изменены. Target-format adapter
получает local = actual world × inverse(actual parent world), затем отражает
систему координат. Это преобразование выходного формата, не второй Node FK.
Вырожденный parent явно отклоняется через EXPORT_PARENT_MATRIX_SINGULAR;
не производится незаметное переподчинение или подстановка transform.

## Проверки

На Icy, BloomX, Alfea01, Alfea02 и PC menu экспортированы skeleton/service
nodes в GLB. Сверены точные world/parent исходного export DTO с общим runtime,
затем файл прочитан независимым тестовым GLB hierarchy evaluator. Проверены
все 1985 Node, 37720 assertions. Максимальная ошибка линейной части после
target TRS roundtrip 5.364418e-7, translation 0.00048828125 игровых единиц.
Это float-погрешность выходного формата, не изменение эталона игры.

Exporter FormatTests и потребители Importer/LVLcreator CoreTests собраны
с 0 warnings/errors. Native DLL и восстановленная логика не менялись;
original probes/native suites не повторялись. Переиспользованы доказательства
actual loaded scene блока 10. Reports включают GLB и его SHA256.

## Граница

Этот блок проверяет начальную файловую позу. Полное соответствие анимации
внешнему glTF/FBX transform model, все фактические mesh occurrences и per-instance
material export остаются дальнейшей работой. Старые helpers выбора physical
mesh/rigid ancestor ещё не объявлены полным scene adapter. FBX визуально и
независимым SDK reader в этом блоке не проверялся. Релиз не упаковывался.

Evidence: research/tools-core-exporter-nodes-block-2026-09-09.json.
