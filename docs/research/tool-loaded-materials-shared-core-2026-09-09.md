# Реальные материалы загруженного графа в ядре Viewer

Блок9 цикла9 сентября до19:00. Исправлены обвязка/подготовка сцены;
восстановленная игровая семантика не менялась. Выпуск не выполнялся.

## Изменение

Удалён1154-строчный `SmoTextureBindingResolver` с подбором по физическому
соседству, именам/размерам групп и выдуманной равномерной анимацией.
SceneBuilder больше не копирует material data первого потребителя во все
shared instances. Каждому существующему экземпляру назначаются собственные
цвет/state/texture; Model не наследует чужую Skin palette. Diffuse=0 сохраняется.

`SmoLoadedResources` копирует неизменяемый снимок настоящего ResourceGraph:
Model/Skin, Material/Pass/Layer/MaterialTexture, DXTexture, AnimTexController.
Сохраняются canonical loader/cache IDs, NULL, все passes/layers,12 texture
states, raw float RGBA, raw alpha byte, признак инициализации specular power,
UV matrix/controllers и факт последней привязки controller к данному holder.
End-time keys не превращаются в набор равномерных кадров. C# records — DTO
адаптера, не параллельные игровые классы. Парсеры и fake referents не добавлены.

BGRA upload берётся из уже выбранной оригинальным loader CPU texture; другая
serialized representation не подставляется. Surface3/4 используют копирование
BGRA/общий raw codec для BGRX. Прочие форматы получают ошибку upload adapter.
Исходный native graph освобождается после копирования; Lazy cache на immutable
document предотвращает повторные загрузки при конкурирующих обращениях.
Это read-only снимок, не сохраняемый animation runtime.

ResolveByRenderable предназначен для экземпляров; ResolveAll оставлен как
явно ограниченный compatibility view физически сохранённого MeshData.
Полный материал также передаётся в Scene DTO.

## Проверки

Original-PC probes не повторялись: семантика восстановленных классов не
менялась. Переиспользованы доказательства readers/конструкторов блоков2–5 и
полной загрузки spatial dependencies блока8. С новым ABI прошли native suites
Material554, MaterialController488, FullLoader213.

| Файл | Model/Skin | Материалы | Texture uploads | Snapshot/scene assertions |
|---|---:|---:|---:|---:|
| Alfea01 |1141|1087|63|15401|
| Alfea02 |1008|986|85|13398|
| Icy |12|3|1|106|
| PC menu |110|108|6|1400|
| BloomX |11|7|13|127|

На выбранных файлах нет ошибок загрузки графа/снимка/texture upload. Проверены
loaded identity против metadata от общего reader, states/passes, собственный
material каждого существующего Scene instance, отсутствие выдуманного таймера.
Независимый Python-потребитель C ABI сверил dimensions/хеши168 uploads с C#;
31 негативная проверка покрыла class/ID, outputs, pass/layer range, pixel extent
и track count. Это проверки моста, не новые доказательства оригинала.

Alfea01 имеет119 групп общих meshes с разными material IDs, Alfea02 —83,
PC menu —11. Разные ID не обязательно означают различный вид. Снимок Alfea02
занял около87ms в одном диагностическом прогоне; это не FPS, не сравнение
производительности со старым resolver и не полное время подготовки сцены.

BloomX содержит три passes, два контроллера по38 end-time keys,10 различных
texture IDs и duration1.26666677. Старые тестовые ожидания искусственных
animated slots/равномерного таймера заменены проверкой этого реального графа;
проверки исходных RGB/alpha сохранены. Промежуточные ошибки тестового пути и
старого UI-ожидания сохранены в логах.

Общие regressions на шести SMO (дополнительно PS2-tagged menu metadata):
37288/36395/9355/3093/1609/1904 assertions. Viewer/Importer/LVLcreator CoreTests
проекты собраны с0 warnings/errors. Это контрольные сборки, не релиз.

## Границы

Frontend slot пока поддерживает один pass/один layer. Полные данные сохраняются,
но многопроходный материал получает MATERIAL_FRONTEND_SHAPE, а не поддельную
композицию. UV/texture update ещё не подключён; новое ядро не выдаёт UI
равномерный FrameDuration. Часть эффектов временно теряет прежнюю имитацию.
Пользователь уведомлён, крупная переделка UI отложена по его приоритету ядер.

NULL Material требует renderer default/current state. Неподдержанный factory
не заменяется metadata object: snapshot сообщает ошибку. Защищённый PC
material-color factory остаётся прежней границей; отдельная metadata inspection
по-прежнему доступна. Legacy alpha/render-state classifications ещё мигрируются.

Выбор Scene occurrences/placement пока прежний: Alfea02 содержит1008 Model/Skin,
но Scene создаёт954 mesh occurrences, включая252 shared instances. Этот блок
исправляет их материалы и не объявляет954 полным native draw list. Следующие
задачи — actual RenderNode/StaticRenderObject membership и material runtime.

Evidence: `research/tools-core-loaded-materials-block-2026-09-09.json`.
