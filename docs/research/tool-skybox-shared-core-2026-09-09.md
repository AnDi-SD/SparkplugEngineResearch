# SkyBox на общих классах Sparkplug

Блок 16 цикла до 19:00. Восстановлен `spSkyBox` 7A7124AF → RenderNode,
подключён к ResourceGraph. В игре **нет отдельного SkyBoxSerializer**:
initializer 6D4B00 создаёт RenderNodeSerializer 469040 и регистрирует его
для SkyBox с masks FF/3. Наш загрузчик использует ту же пару.

## Логика и доказанная ошибка реконструкции

Старый общий RenderNodeSerializer требовал exact RenderNode на read/index/write.
Это было чрезмерное ограничение реконструкции: оригинальный initializer и
исполнение reader/writer на настоящем SkyBox доказывают использование потомка.
Проверка заменена на подтверждённое семейство RenderNode. Сам формат и
reference resolver не переписаны; отдельный класс сериализатора не создан.

Три original/source случая (inline, repeated alias, prebound) совпали по
состоянию, количеству ссылок и всем байтам обратной записи. Исполнялись настоящие
SkyBox/Model factories, registration initializer, inherited readers, indexers,
writers и destructors; все выделения освобождены. 1..3 inline Model — наблюдение
старого корпуса, а не ограничение читателя: он принимает Renderable references.
Metadata SkyBox adapter теперь использует общий RenderNode projector; его
самостоятельная inline-only/1..3 проверка удалена.

PC49E440 сначала выполняет RenderNode world update, затем безусловно копирует
local orientation в world orientation. Порядок относительно bounds/light cache
сохранён. Две новые original/source raw PRS captures совпали побайтно, включая
чистый dirty bit. Родитель и SkyBox созданы настоящими factories и связаны
настоящим Attach; arena 52640 bytes. Первые два world probe имели неверный
offset scale в тестовом входе, дали bounded memory fault и сохранены; после
проверки ABI используется +30, успешен третий запуск. Это исправление fixture,
не игрового метода. Лимиты guest не повышались.

Обычный support draw SkyBox true/no-op; отдельный pass снимает все Fog references
до вызова базового support с force=false, даже когда SkyBox disabled. Это
перенесено с использованием имеющегося SetFog; backend callback обозначает
ещё неподключённый renderer boundary. Доказательства original draw CP9
переиспользованы, source test проверяет порядок, release, alpha-sort и failure.
SkyBox clone/copy явно unavailable до нужного инструментам сравнения; он не
наследует clone, создающий обычный RenderNode.

## Результат для инструментов

Native container kind=3 и SmoSceneMesh.ContainerKind сохраняют назначение SkyBox.
Общая Scene содержит его геометрию и реальные occurrences для экспорта/инспекции.
Текущий OpenGL обычный model pass пропускает SkyBox; Scene выдаёт
SKY_PASS_PENDING. Camera-follow manager/отдельный frontend pass остаются далее,
их результат не подменяется обычной моделью. Layout UI не менялся.

| Реальный PC файл | Объекты | Scene occurrences | SkyBox / members |
|---|---:|---:|---:|
| Challenges/race_01 | 3187 | 882 | 1 / 1 |
| DatingAssets/mini_level_date_02 | 212 | 97 | 1 / 1 |

Две полные сцены, 979 occurrences; 18 SkyBox-specific checks и 7350 общих
occurrence checks. race_01 также загружает 47 actual Octree. Уровни BMS_02,
Gardenia02 и RedF01 продвинулись до следующих отсутствующих классов:
OcclusionVolume 43D24430 и NavigationGraph 188A161F. Выбор дополнительных
контролей по индексу также показал LensFlare 435370B5 в battle_01. Ничего из
этих файлов не удалено, неизвестные классы не подменены.

Native suites: FullLoader213, RenderNode34, SceneSerialization469, SkyBox13.
Пять managed consumer builds прошли без warnings/errors. После подключения
SKY_PASS_PENDING/backend фильтра они собраны повторно. Native build attempt1
исправлен после ошибки в тестовом API, attempt2 successful; логи сохранены.

DLL SHA256 `2B6FC0F318853EFF0CA7BA91B8BA90FE0CAC75311711B15B2DB4EEC986E3E0C5`.
Нового PS2 исполнения, визуальной UI проверки и релиза нет.
Evidence: `research/tools-core-skybox-block-2026-09-09.json`.
