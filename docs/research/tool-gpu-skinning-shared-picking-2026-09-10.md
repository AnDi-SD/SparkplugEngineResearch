# Skin: shader arithmetic и единый источник picking positions

Источник истины — original PC `Shaders/Fixed.rfx`, SHA256
`ac6785428ba851dea88e3a6cddc03fdf620fa06d82063db83176598140803db1`.
Его полный engine event stream ранее проверен в [CP55](native-pc-rfx-corpus.md).
Текущий блок касается modern backend для четырёх явно сохранённых весов;
полный original GPU draw и material/light/shader parity не заявляются.

## Доказанная ошибка приложения

В branch `BlendWeightCount == 4` исходный HLSL складывает четыре произведения
позиции на affine bone matrix и authored weight, записывая только `Position.xyz`.
Компонента w остаётся1. Деления на сумму весов, epsilon cutoff и замены нулевой
суммы исходной позицией нет. Normal суммируется тем же способом и нормализуется.

Прежний OpenGL shader отбрасывал веса≤1e-6 и делил весь vec4 на сумму; такой же
расчёт дублировали Scene CPU picker и WPF animated picking companion.
Восстановленные `spSkin`/palette source не содержат этой ошибки и не меняются.
Современный shader исправлен по original HLSL. GPU-проверка выполняет именно
production source через transform feedback, с явными матрицами и числовыми
ожиданиями для unit, half, sum>1, negative, tiny, zero и four-affine inputs.

Адресная проба: **11 checks** на RTX3070/OpenGL3.3, 1,305с,
peak154562560B (147,4МиБ), скрытое окно. Старый production shader тем же тестом
отклонён на half-sum: ошибка координаты2. Его первая regression invocation
не завершилась за30с: новый test mode пропускал exception наружу, поэтому
harness остановил свой процесс. После top-level catch повтор завершился
ожидаемым exit1 за0,926с. Первая ошибка и логи сохранены; это дефект нового
проверочного стенда, не игры.

## Picking и экспорт

Вместо нового CPU skin evaluator подключён readback тех же GPU positions
через production vertex shader и actual common palette. CPU picker получает
готовые positions от host provider; при их отсутствии или неверном размере/
нечисловой координате skinned picking явно исключается с диагностикой.
Viewer запрашивает readback перед выбором, без повторного CPU skinning на каждом
animation frame. Тот же provider подключён к LVLcreator: его начальные palettes
не анимируются, поэтому positions кэшируются до замены сцены. Изменение world
transform не требует повторного получения локальных positions. Будущее изменение
palette потребует invalidation; это условие отмечено в коде.

Transform feedback использует тот же production vertex source и загруженную
geometry; отдельной формулы деформации нет. Позиции читаются для всех vertices,
включая неиспользованные индексами. Камера, model transform и координатная
система редактора применяются отдельно. Буфер ограничен16МиБ, выделяется по
потребности и используется повторно. Предыдущие GL program/VAO/feedback
bindings, range и rasterizer-discard сохраняются. Clear освобождает буфер
в следующем Render с актуальным контекстом.

Проверка реального Icy mesh и host boundary fixture: **22 checks**, 1,523с,
peak178012160B (169,8МиБ). Проверены исходные positions без palette, возврат
palette, независимость local capture от камеры/model, unindexed vertex,
GL-state restoration, отсутствующий/неоднозначный объект и Clear/reload.
Scene provider отдельно прошёл **14 checks**. LVLcreator собран без ошибок и
предупреждений; фактический выбор кликом в его окне в этот блок не входит.

Review обнаружил дополнительную ошибку Viewer: общий renderer key терял
`OccurrenceKey`, хотя два support slots могут указывать на один Renderable
и physical mesh. Ключ теперь сохраняет существующую пару container/slot;
geometry cache по-прежнему общий. Финальная readback проверка — **27 checks**,
1,679с, peak178860032B (170,6МиБ), включая независимые palettes/appearance,
один geometry upload на два размещения и строгий отказ ambiguous полного key.
LVLcreator уже запрещает repeated authoring своим прежним guard; его командная
адресация не расширялась. Новых игровых ID или wildcard-семантики ключей нет.

## Сквозная анимация Viewer

Скрытый OpenGL context выполняет actual load/selection/slider/render/picking
handlers окна. **7 clips, 43 poses, 408 обновлений skinned meshes**;
13124 assertions; финальный повтор после occurrence fix — 4,479с,
peak277221376B (264,4МиБ). Проверяется замена
Positions каждой companion geometry после readback; оставшиеся исходные или
устаревшие данные не могут пройти capture. Ригидный bbush учитывается отдельно.

Независимый NumPy FK проверил **419 mesh/time comparisons**, max error
`0.000553493929`, все в прежнем допуске `1e-5 + extent * 8e-6`.
Исторические имена placements содержат physical mesh ID, новые — actual
Renderable ID. Capture теперь сохраняет metadata реальных ссылок. Валидатор
принимает только полное взаимно однозначное соответствие по physical mesh;
дубликаты, недостающие и чужие file IDs отклоняются. Формулы, порядок вершин,
геометрия и допуск не изменены. Старый capture без metadata сохраняет прежний
путь точного сравнения имён.

Сначала по ошибке был выбран старый `shared-san-export-v4/input.json`.
Он воспроизвёл четыре известных расхождения Icy при1с, max0,113615:
его PRS предшествует исправлению original `4648C0` от8сентября.
Использован **уже существующий**, не изменённый этим блоком
`local-data/results/viewer-sparkplug-core-20260908/reference-input.json` с
сохранёнными original PC PRS. [Причина и прежняя проверка](tool-viewer-sparkplug-core-2026-09-08.md)
не исследовались повторно. Ошибочный capture/comparison сохранён как failed;
проверка прошла в `gui-original-prs/comparison.json`, затем после occurrence
fix повторена в `gui-occurrence/comparison.json` с тем же максимальным error.

NumPy отсутствует в системном Python; валидатор запущен встроенным Python
Blender4.5 в background с `--python-exit-code 1`. Это среда выполнения
числового валидатора, не повторный импорт модели или визуальная проверка.
OS dialogs, GPU pixels и полный lighting/material output не утверждаются.

## GLB

GLB exporter содержал ту же нормализацию и ошибочный комментарий о соответствии
игре. Его исправление сохраняет прежние near-unit weights без изменений и
отказывает с `GLB_SKIN_WEIGHT_SUM` при разнице с1 более0,0001. Это host export
граница, не правило допустимости оригинального SMO. Verified conversion для
такой деформации ещё нет; автоматическая подмена весов не выполняется.

Новый GLB test DLL со старым Core обнаружил молчаливую нормализацию и перезапись
существующего destination. Исправленный Core: **10 checks**, 1,675с; три
неподдерживаемые суммы отклонены до записи, сохранены marker bytes назначения.
Три поддерживаемых unit/near-unit output побайтно совпали с прежними GLB
(каждый131604B). Исходный SMO и переданный scene DTO неизменны.

Локальные данные: `local-data/results/tools-core-cycle-20260910-0730/gpu-skinning/`
и `glb-skin-weights/`. Числа assertions относятся к разным контрактам и не
складываются в количество проверенных файлов или процент готовности ядер.
Hashes production sources, выбранных inputs, результатов и сохранённых
неудачных запусков: [manifest](../../research/tools-core-gpu-skinning-2026-09-10.json).
