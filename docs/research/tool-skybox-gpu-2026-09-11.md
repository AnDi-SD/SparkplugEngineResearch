# SkyBox: общий camera-parent transform и отдельный GPU pass

11 сентября 2026, блок 17. Viewer и LVLcreator теперь выводят SkyBox перед
обычными объектами. Прежний backend пропускал его и возвращал SKY_PASS_PENDING.
Новый pass использует существующие общие материалы, textures, геометрию и
alpha ordering. Исходные render occurrences остаются прежними.

## Основание и граница реализации

PC game blocks `5DA6CA..5DA727`, `5F9F10..5F9F6A` прикрепляют SkyBox к
DefaultCamera. Original `49E440` выполняет inherited world update и только
потом копирует local orientation в world orientation. Scene `45EC70` выполняет
Sky/его alpha flush до ordinary supports. Это уже подтверждено в
[scene special managers](native-pc-scene-special-managers.md) и
[whole SceneRender](native-pc-scene-render-runtime.md).

`SkyPreview.h` — **host transform projection**, использующий настоящие общие
`spNode` и `spSkyBox`. Это не второй алгоритм transform, не native clone и не
перенос всего SkyBoxManager. Временный camera parent и SkyBox получают
наблюдённые local PRS/flags; world считает общий `UpdateWorldForAnalysis`.
Члены, children, bounds и light caches исходного графа не перепривязываются.

У Node без inheritance-position original оставляет прежнюю world position.
Поэтому DTO сохраняет и её; временный SkyBox сначала получает эту начальную
world position через обычный root update. Подмена её local position или нулём
была бы неверна. Новый C ABI `SpvSkyPose` имеет 76 bytes;
`spv_graph_sky_pose` читает настоящий объект, `spv_sky_camera_world` принимает
finite unit-scale camera world и возвращает affine matrix. Неверные extents,
nonfinite inputs и неединичный camera basis отклоняются до записи output.

C# хранит immutable pose в общем loaded-container/scene DTO. При подключённом
runtime свежая поза читается из его настоящего графа. Backend получает
camera parent обратным преобразованием game-world → LH camera view;
учитываются reflection и turntable. Не вводится условный «куб в позиции камеры»
или искусственный перенос всей геометрии на far plane.

Sky opaque и Sky alpha проходят до обычных opaque/alpha. Общий native alpha
sort используется отдельно для двух фаз, без повторной реализации в C#.
Материалы сохраняют свои depth/blend rules; дополнительный depth clear не
придуман. Пока backend вообще не применяет Fog, Sky остаётся без Fog. Actual
native release Fog references при dedicated draw известен, но новая transform
projection не воспроизводит этот mutation/lifetime side effect исходного графа.
Полная Scene visibility, manager reparent transaction и fog backend остаются
отдельными незавершёнными задачами.

## Проверка одним связанным пакетом

Свежий original PC guest: **6 worlds / 90 raw world words / 25 checks**.
Исполнены actual Scene, DXCamera и SkyBox factories, Attach, SceneManager world
update и teardown. Проверены translation, camera rotation, local orientation,
signed scale и различные inheritance flags. Все 360 original PRS bytes сохранены;
матрицы, упакованные по ранее подтверждённому Node affine convention, совпали
с новым C ABI. Это PRS-to-matrix сравнение, не снятие original GPU frame.
Все tracked native allocations освобождены; scoped guest — 4,20 s суммарно.

| Реальная сцена | Sky placements | Все Mesh placements | Sky alpha |
|---|---:|---:|---:|
| DatingAssets/mini_level_date_02 | 1 | 97 | 0 |
| Challenges/race_01 | 1 | 882 | 0 |
| Gardenia/Gardenia02 | 1 | 772 | 0 |
| Sky/Sky_02 | 2 | 44 | 1 |

На всех четырёх сценах camera translation `(16,8,-32)` сохраняет Sky-only
frame побайтно, поворот изменяет его, toggle восстанавливает точный frame.
Проверены mixed scene, separate alpha, live graph, Clear и GL errors.
Два настоящих скрытых окна — Viewer и LVLcreator — подключили все 97
placements контрольного уровня. Menu/Text и Icy shader control прошли.
Native SkyBox и NodeWorld: 2/2 suites, 99 checks. GPU working sets в первых
полных запусках — примерно 194–252 MB на процесс, без сканирования корпуса.

На race_01 сохранилась прежняя явная NULL_MATERIAL_RENDER_CONTEXT для Model[2].
Она не подменяется восстановленным default state; geometry preview этой модели
остаётся отдельной host policy прежнего material блока.

## Открытый случай редактора

Команда LVLcreator, передающая только изменённую **world matrix** SkyBox,
недостаточна для нового parent-relative просмотра. Без свежей local pose
backend выдаёт `SKY_EDIT_POSE` и пропускает затронутое небо, вместо молчаливого
игнорирования правки. Возврат authored placement снимает диагностику; это
проверено. Следующая редакторская задача — передавать common local Node pose
после transform command. Инспекция/сохранение исходных occurrences не менялись.
Camera-follow picking и полноценные SkyBox editor controls пока не заявляются.

## Ускорение подбора проверок

Один большой SQLite join по SkyBox/direct_fields выбрал неудачный план через
частый `field_type=0` и был остановлен. Новый
[селектор](../../research/select_sky_gpu_samples.py) сначала использует class
index (43 PC объекта), затем unique file/object index для полей. Он выбрал
22 файла без чтения их SMO payloads; последний bounded запуск занял 0,030 s.
Первый быстрый отчёт ошибочно считал Node section 0 вместо Sky section 1;
исправлена только query semantics, сохранён новый отдельный отчёт. Полный
корпус не перечитывался; в GPU пакет добавлен один минимальный случай с двумя Sky.

[Manifest](../../research/tools-core-skybox-gpu-2026-09-11.json) привязывает
selected sources, binaries и raw captures. Результаты лежат в
`local-data/results/tools-core-cycle-20260911-1900/sky-gpu/`.
Изученность EXE не повышается за подключение backend. PS2 runtime execution
в этом блоке отсутствует; прежние PC/PS2 file comparisons остаются в
[классовом досье](smo-class-sp-sky-box.md).
