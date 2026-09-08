# CP131 — нужные шаблоны для полной замены персонажа

Дата: 8 сентября 2026. Восстановлена операция `import.skin`, которую
[CP130](tool-importer-textures-2026-09-08.md) перевёл в partial после текущих
отказов. Фиксированный измеритель снова **16/16**; это устранение регрессии
существующей операции, а не новая возможность или завершение всего движка.

## Причина и изменение

`SmoSkinnedVisualGraphPipeline` полностью удаляет старые визуальные ресурсы
и строит самостоятельные донорские ветви. Тем не менее прежний
`ReadTargetVisualGroups` требовал однозначную текстуру и skinned E1 layout
у каждого старого меша. Он отклонял rigid-деталь Bloom_body и неявные
material continuations Tecna, Icy и Flora ещё до построения нового графа.

Теперь рассматриваются только подтверждённые кандидаты: непосредственно
вложенный в `spSkin` base mesh, явный material этого skin и inline `TextureData`
этого material, с известным skinned E1 layout. Для этой операции достаточно
подходящего шаблона. Старые material-less участки не превращаются в шаблоны.
Правила их runtime inheritance не угадываются и глобальный binding resolver
не меняется. Дальнейшие проверки material fields, palettes и готового графа
сохраняются. Цель без подходящего шаблона отклоняется.

Второй отказ был вызван требованием inline `spFog` внутри выбранного skin.
Icy и Flora используют явную ссылку на уже существующий helper. Builder
разрешает именно decoded fog relationship и проверяет, что объект определён
до точки добавления новых ветвей в конце render node. Копирование неизвестных
fog semantics не требуется. Старые collision info и OBB остаются побайтно
неизменными, все исходные узлы скелета и их локальные transforms сохранены.

Изменения входят в рабочую версию Importer **0.6.1**; опубликованная версия
остаётся 0.6.0. SMO-донорный перенос исходных ветвей этим изменением не затронут.

## Выборка и проверки инструмента

Пять pristine targets: Bloom_body, bloom_jeans, Tecna, Icy, Flora. Контрольный
донор имеет два треугольника на Pelvis и Head, два opaque материала и отдельные
BGRA-текстуры 17×9 и 8×8. Он изолирует нужные serializer/graph зависимости.

Корректный baseline `baseline-v2` воспроизвёл четыре отказа; bloom_jeans прошёл.
В первой версии теста ошибочно сравнивался полный список deform rig после
удаления старых palettes: такой список может сокращаться при сохранении всех
узлов. Тест исправлен на проверку самих исходных узлов, transforms и logical
node links до записи baseline-v2; это не исправление production-кода.

`final-v1`: **65 проверок**, включая десять проверок отказа без material
template и сохранности предыдущего output, все прошли за **1,095 с**,
peak **76 718 080 байт**. Проверены обе donor-геометрии, отдельные NPOT-текстуры,
удаление старых visual IDs, новые bindings, skeleton hierarchy и transforms,
collision payloads и неизменность исходных файлов. Неизменённый контрольный
bloom_jeans дал те же байты, что baseline-v2.

## Оригинальный PC loader

[`probe_pc_imported_skin.py`](../../research/probe_pc_imported_skin.py)
исполняет целый оригинальный loader `0x422B50`, factories/readers и destructors
из pristine EXE SHA-256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
RTTI/startup, bounded stream/CRT/COM остаются объявленными входами стенда.
Это не запуск игры и не проверка GPU-rendering.

| Target | Объекты | Инструкции whole load | Освобождённые allocations | Arena, байт | Весь опыт, с |
|---|---:|---:|---:|---:|---:|
| Bloom_body | 115 | 3 600 285 | 1 145 | 139 904 | 13,443 |
| bloom_jeans | 114 | 3 569 619 | 1 135 | 139 312 | 13,121 |
| Tecna | 99 | 3 080 291 | 1 028 | 135 952 | 12,089 |
| Icy | 101 | 3 262 503 | 1 032 | 134 016 | 12,788 |
| Flora | 99 | 3 080 417 | 1 028 | 135 952 | 14,925 |

Все файлы прочитаны полностью, каталог опубликован целиком, engine diagnostics
отсутствуют. Все allocations и COM acquisitions освобождены. Сохранённые
collision-объекты загружаются оригинальными readers; их runtime geometry
семантически не переоценивается.

Сначала стенд остановился на отсутствующих RTTI registrations collision/OBB,
затем на лимите 1 млн инструкций во время штатного чтения узлов. Добавлен
отдельный заранее выбранный профиль `character`: 4 млн инструкций / 16 с
на вызов, heap 256 КиБ, процесс 30 с, вход до 32 КиБ / 256 объектов / двух
текстур до 32×32. Старые `micro` и `file` не изменены. Прежние остановки
сохранены; каждый новый опыт создавал свежий guest.

Для helper metadata использованы точные registration initializers
`6D39A0` (spCollisionInfo), `6D4610` (spOBBBV) и их реальные factories.
После scene teardown отдельно вызывается оригинальный deleting destructor
лениво созданного `spCollisionManager`: vtable `6E6FE4`, размер `1790h`,
RTTI record `75FB18`, engine owner slot `75DB6C`, destructor `4571C0`.
Это необходимое завершение стенда, а не реконструкция collision queries.

[`validate_imported_skin_palettes.py`](../../research/validate_imported_skin_palettes.py)
сверил строгий C# decoder с живыми объектами оригинального loader: **19 skins,
304 пары bone/matrix, 19 456 байт inverse binds** совпали точно. Также совпали
все material/fog/base-mesh/bone IDs и число influences. Проверка ссылок не
ограничилась каталогом нашего parser.

## Штатная анимация и пределы вывода

Реальный скрытый Viewer открыл пять результатов и применил blwalk, wtgl,
xiwa и wfgl: **3 144 GUI checks, 30 поз**, 3,393 с, peak 188 325 888 байт.
Независимый NumPy FK/skinning подтвердил **114 mesh/time** сравнений;
максимальная ошибка **0,0000349149**. Донорские вершины движутся во всех пяти
парах. Используется общий SAN sampler, ранее отдельно сверенный с оригинальным
PC; эти stock PRS не объявляются новым независимым native sampling proof.

GUI Release сборка Importer прошла с нулём предупреждений/ошибок. 18 проверок
execution guards и шесть проверок heap profiles подтвердили сохранность границ
старых профилей и нового 256 КиБ mapping. Полный корпус и неизменённые FBX/VMD
проверки не повторялись.

Результат подтверждает чистую замену в выбранных native layouts. Он не делает
автоматический риггер универсальным, не добавляет поддержку неизвестных
texture representations и не доказывает внешний вид в игровом renderer.
Исторические gameplay gates остаются отдельным evidence со своими inputs.
Широкий class ledger не увеличивается за эту интеграцию.

Локальные результаты: `local-data/results/tool-cycle-20260908-1900/importer-clean-targets`.
Публикуемый [validation manifest](../../research/tool-importer-clean-targets-validation-2026-09-08.json)
содержит hashes кода и точных входов/выходов; игровые bytes остаются локальными.
