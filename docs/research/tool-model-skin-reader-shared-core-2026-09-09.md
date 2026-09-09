# Общие readers Model и Skin для tools — 9 сентября 2026

Третий блок дневного цикла до19:00 МСК. `SmoModelDecoder` и `SmoSkinDecoder`
используют один мост к `spRenderableSerializer`, `spModelSerializer` и
`spSkinSerializer`. Самостоятельный C# разбор порядка секций/полей, UInt32,
палитры и16 float матриц удалён. Для связи ID используется существующий индекс
документа; отдельный словарь всех Node для каждого Skin больше не создаётся.

## Реализация и исходное поведение

Общие циклы полного reader получили явную `InspectionForAnalysis`. Проходы
Renderable→Model→Skin те же, скалярные значения назначаются настоящему
`spModel`/`spSkin`. Режим инспекции хранит позиции, ID, inline-size ссылок и
буквальные матрицы. Палитра из выдуманных Node не создаётся; частичный Skin
не получает подставные bone owners. Обычный полный reader сохраняет прежние
`ReadFieldReference`/`ReadSequenceReference`, типы и канонических владельцев.

Общий host-помощник `Analysis/PC/spReferenceInspection.h` использует исходный
`spSerializer::ReadReferencePrefixForAnalysis`, ограниченно пропускает inline
тело и применяется также инспектором материалов. Это аналитическая обвязка,
не класс игры или альтернативный загрузчик ресурсов. Материалы повторно
сверены с9 сохранёнными original captures; старые журналы блока2 не переписаны.

Исходные PC Model4938F0→Renderable reader и Skin491170 уже восстановлены.
Новый probe Model дополнительно подтвердил whole-DWORD alpha normalization:
значение256 даёт true. Поля priority/alpha могут приходить отдельно, в любом
порядке; неизвестные поля пропускаются, повторные scalars используют последнее
значение. Renderable null material/fog очищает назначение. Model может не иметь
поля mesh, но явно заданная null mesh-ссылка отклоняется reader.

Skin читает weight count без предположения1–4, принимает пустую палитру,
повторный bone ID и замену палитры следующим полем. Матрица читается буквально,
включая неаффинные, необратимые, signed-zero, NaN и infinity биты. Null bone
отклоняется после потребления матрицы. Original при замене массивов теряет старые
аллокации; portable контейнер освобождает заменённую память — прежняя явно
обозначенная host-граница. Алгоритмы reconstructed reader не исправлялись;
удалены лишние ограничения приложения и добавлена наблюдаемость общего reader.

## Проверки

`research/validate_tools_model_skin_reader.py original` сохраняет11 свежих
ограниченных исходных запусков: Model empty/priority-only/alpha-wide/repeat;
Skin empty/one/repeat-bone/repeat-field/clear/raw-bits/null. EXE и транзитивные
зависимости проверяются по хешам. Каждый guest:64КиБ arena,100000 инструкций/2с
на вызов,30с на процесс. Skin probe использует настоящие inline/shared Node,
учитывает native borrowed lifetime и явно освобождает выявленные исходные leaks
после завершения оригинального teardown. Это не выдаётся за исправление игры.

`compare`: все11 source captures, writer bytes и результаты инспектора совпали.
Четыре дополнительные host-проверки отклонили неверный kind, нехватку секций
и count за пределами поля. C++ SkinSerialization119/MaterialSerialization554/
FullLoader213 прошли; команды `--capture` дополнительно проверяют матрицы,
weights, ID и отсутствие подставной палитры во всех7 Skin случаях.

Viewer/Importer/LVLcreator CoreTests собираются без ошибок и предупреждений.
Четыре новые managed-проверки покрывают нетипичные Model scalars, необязательные
inherited Skin поля, повторные кости, специальные биты Matrix4x4 и очистку.
FormatTests на выбранных pristine PC файлах:

| Файл относительно Media | Проверок |
| --- | ---: |
| Levels/Alfea/Alfea01.smo | 37101 |
| Levels/Alfea/Alfea02.smo | 36326 |
| Menus/igmenu_opt_pc.smo | 9330 |
| Menus/igmenu_opt_ps2.smo | 3005 |
| Characters/Icy/Icy.smo | 1577 |

Логи: `local-data/results/tools-core-cycle-20260909-1900/model-skin-reader/`.
Фиксация: `research/tools-core-model-skin-reader-block-2026-09-09.json`.

## Границы

- Общий reader допускает Model/Skin без mesh-поля. Старое представление C#
  потребителя требует bound MeshData и явно отклоняет такой случай. Это
  ограничение DTO, не игры; fictitious mesh не подставляется.
- C# связывает используемые конкретные MaterialData/Fog/MeshData/Node wire
  классы. Native runtime поддерживает соответствующие базовые семьи; метаданные
  не доказывают материализацию неизвестного concrete ресурса.
- Nullable scalars и masks C# сохраняют различие между отсутствующим полем и
  полем после общего reader. Native info также содержит действительные defaults.
- Матрицы не исправляются при чтении. Исполнение render backend на NaN/Inf
  не проверено и не заявляется.
- Нет проверки whole-level runtime, запуска PS2 ELF, полного корпуса, UI
  переработки или релиза. Три редакторских случая остаются отложенными пользователем.
