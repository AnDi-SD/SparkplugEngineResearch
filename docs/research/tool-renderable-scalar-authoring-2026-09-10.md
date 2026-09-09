# Общая запись Renderable alpha-sort и priority в Skin

Пятый блок цикла до 07:00 МСК. Исправлена граница authoring в Importer:
`BuildSkin` раньше искал scalar field2/3 по всему объекту, включая Model и Skin
секции. Поэтому неизвестное поле с тем же номером могло быть изменено либо
ошибочно участвовать в проверке наличия обязательного Renderable поля.
Игра разделяет три секции; исправление относится к нашему инструменту.

## Общая игровая логика

`spRenderableSerializer` выделяет из существующего writer два общих метода:
`WriteAlphaSortEnableFieldForAnalysis` и `WritePriorityFieldForAnalysis`.
Полный writer вызывает те же методы. Они всегда пишут UInt32 поля2/3:
alpha — bool0/1 из actual Renderable, priority — raw UInt32.

В actual `ReadRenderableFieldsForAnalysis` добавлены host observations только
успешно прочитанных scalar assignments: номер, offset относительно начала
Renderable-секции, размер, порядок и borrowed pointer на actual owner.
Повторные поля в игре остаются last-wins; неизвестные поля в других секциях
не становятся Renderable полями. Наблюдение не меняет reader semantics.

Подтверждение writer — PC secondary `47F7A0 → 1402630` и прежние sealed
original captures `model-empty-original.log` / `model-repeat-original.log`
из блока [Model/Skin reader](../../research/tools-core-model-skin-reader-block-2026-09-09.json).
Первый содержит `6201000000630000000000`, второй `620000000063ffffffff00`.
Это reuse ранее полученных результатов; новых original executions и
исправлений восстановленного алгоритма нет.

## Обвязка и ограничения

`spv_skin_patch_sort_scalars` использует настоящий partial `spSkin` и
`spSkinSerializer::InspectPayloadForAnalysis` для всех трёх секций. Он требует
ровно одно исходное поле alpha и priority в Renderable, применяет actual setters
и получает payload от общих writer slices. Остальные байты сохраняются.
Отсутствующие или повторные Renderable scalars дают `RENDERABLE_SCALAR_SHAPE`;
поле2/3 в Model/Skin не заменяет отсутствующее поле первой секции.

`SmoRenderableScalarWriter.PatchSkinSort` возвращает целый объект прежнего
размера. `BuildSkin` использует его как source перед существующей структурной
перестройкой. Ручная запись field2/3 и их подсчёт по всем секциям удалены.
Профили приложения остаются явными входами0/1; palette weights, padding и
texture/material/reference builders в этом срезе не менялись.

Host копирование payload/проверка общего writer header теперь разделяются
Material и Renderable bridge. C# проверка принадлежности object entry и
копирование native output также общие (`SmoScalarTemplateEdit`). Это служебная
обвязка, не дополнительный игровой serializer.

## Проверки

Native5 suites прошли: SkinSerialization193, RenderNode34,
MaterialSerialization653, FullLoader213, DataBlockWriter340. Пять consumer
projects собраны без ошибок/предупреждений. Managed Renderable —44checks:
три секции, перестановки, повторные неизвестные поля, широкие headers,
идемпотентность, неизменность source, отсутствие/повтор/усечение настоящих
scalars, foreign object entry и неподходящий класс.

Material regression44 повторно прошла после выделения общей host обвязки.
Icy end-to-end13 прошёл через оба новых scalar writer; его 101-object/13110B
выход побайтно совпал с предыдущим проверенным блоком. Общий FormatTests647.
Независимая ABI проверка —6 успешных Skin случаев с идемпотентностью,
19 проверок отказа и2 Material регрессии;1,372с. Проверены17 original
dependencies и pristine EXE. Bone references и raw NaN/signed-zero matrix bytes
остались исходными. Итог фиксируется в manifest блока.

Результаты — новый локальный каталог
`local-data/results/tools-core-cycle-20260910-0700/authoring-renderable/`.
При проверке доступной RAM получено704084KiB; дополнительные .NET процессы
принадлежат VS Code пользователя. Они не останавливались. Managed сборка
этого блока использовала один worker; native сборка — прежние два.
Отдельный [аудит FAT-записи](tool-container-writer-boundary-2026-09-10.md)
зафиксировал предложение, ожидающее решения пользователя; его реализация
не включена в этот блок.
