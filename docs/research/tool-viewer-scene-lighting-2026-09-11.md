# Viewer: LightManager в живой загруженной сцене

В `SparkplugSceneRuntime` подключён общий `spLightManager`: источники обнаруживаются
по actual C++ RTTI, явный упорядоченный список регистрируется на принадлежащем сцене
графе. `LightSelections` возвращает обычные источники и отдельный ambient по индексу
**RenderNode-контейнера**, а не по mesh или Skin. Отдельный C# selector не добавлен.
Viewer включает контекст при создании runtime; интерфейс изменён одной строкой.

Игра и восстановленная логика не изменены. Использованы уже проверенные методы
[CP93](native-pc-skin-selected-light.md). Новые `SceneLighting.h`, C ABI и managed
фасад — host ownership/scheduling. `EnableDocumentLighting` явно задаёт политику
предпросмотра: все загруженные Light в порядке графа, активная иерархия и окончательное
обновление cache после вычисления всех поз. Это не утверждение о восстановлении
полного `spScene` frame/partition traversal. Такая политика нужна, чтобы выбор не
использовал положение предыдущего кадра при независимой перемотке.

Контекст удерживает граф, связывает существующие Light/RenderNode и живые world
spheres. Второй owner того же графа отклоняется до изменений. При очистке/уничтожении
снимаются borrowed links и targets, очищаются caches, восстанавливаются исходные
hierarchy bits. Частичный Node graph отклоняется: рекурсивная активация не должна
затронуть узлы вне сохранённого контекста. Остальные прежние host limits сохранены.

Адресная проверка `--scene-lighting` на pristine `Characters/Icy/Icy.smo` и `xiwa.san`
прошла **56 checks** за 1,147 s до записи отчёта, peak working set 39 174 144 bytes.
Actual RenderNode ID3 выбирает ambient ID119 и ноль обычных источников. Проверены
активация/отключение/пустой список, повторная конфигурация и ошибочные ID, размеры
буферов, два владельца, восстановление flags и удержание графа после закрытия внешнего
handle. Пять кадров с перемоткой дают те же Node matrices и Skin palettes, что runtime
без lighting. Native RenderNode/SkinRender suites прошли 2/2; контрольная сборка
Viewer и managed tests — без ошибок и предупреждений. Это не выпуск.

Report и hashes: [evidence](../../research/tools-core-scene-lighting-2026-09-11.json),
локальные captures в `local-data/results/tools-core-cycle-20260911-1900/lighting/`.
Исходники и фактически проверенные binaries сохранены отдельно от следующих сборок.

**Остаток:** GPU shader пока использует прежний preview light; `LightSelections`
подготовлен для следующего подключения device payload/material submission.
StaticRenderObject, полный multipass, исторический Scene scheduler и совпадение
изображения с игрой этим блоком не закрываются. Новые баллы изученности EXE не начисляются.
