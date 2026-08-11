# 2026-08-11 — SAN/ANM и проигрывание анимаций в SmoViewer

## Корпус

Исследована `local-data/pc-pristine/Media/Characters/Bloom`: 30 ANM, 168 SAN,
21 SMO. SAN scan совместно со SMO проверил 189/189 FFPS-файлов без ошибок.

## Результат

ANM оказался текстовой картой gameplay-состояний на имена SAN. SAN содержит один
объект `0x56EE563A`, длительность и именованные position/rotation/scale curves.
Подтверждены layout ключей, времена, Vector3, quaternion XYZW и семантика пустой
curve как bind-компоненты.

SmoViewer получил автопоиск рядом с моделью, ручную загрузку SAN/ANM и папки,
фильтруемый список, timeline, play/pause и покадровые кнопки. Поза собирается по
именам node, иерархии skeleton и bind-local transforms. Skinned positions и
normals пересчитываются на CPU по `inverseBind * animatedBoneWorld`; overlay
skeleton и attachment points следуют текущей позе.

## Исправление полной rig-иерархии

Первый вариант рассчитывал animation world только для palette bones. На
`bloom_jeans.smo` это складывало персонажа: `Pelvis [70]` и `Spine_01 [71]`
имеют промежуточных родителей `C-lowerRoot [111]` и `C-upperRoot [112]`.
Исправленный path вычисляет позу для всех `spNode` по `esfNodeChild`, включая
control rig, и лишь затем берёт матрицы palette bones для skinning.

## Фильтр совместимости

Фильтрация не пытается угадать персонажа по короткому имени SAN. Источником
категории служит ANM, который действительно ссылается на клип. Пользователь
включает наборы галками; SAN может входить в несколько ANM, а клипы без ссылок
явно отделены в группу `<папка> · без ANM`. Ручной выбор общей папки рекурсивен,
поэтому одним списком можно управлять наборами Bloom, AdvBloom, Bird и других.

## Dragon crash

`Dragon.smo` содержит 10 skinned meshes формата `0x097E` и 3 rigid meshes
`0x0940`. Rigid geometry находится внутри skin-поддеревьев, но не имеет массивов
blend weights/indices. Первый animation loop ориентировался только на наличие
ancestor `spSkin` и получал `IndexOutOfRangeException` на `dnaf.san`. Исправление
проверяет подтверждённые skinning arrays перед CPU deformation. Исключение pose
path дополнительно останавливает playback и выводится в журнал вместо завершения
WPF-приложения.

Дополнительная проверка mesh `[49] Horns_03` выявила отдельную ошибку чтения:
его `spRenderNode [46]` сериализует position, но опускает quaternion. Decoder
ошибочно отбрасывал весь transform вместо identity rotation и применял только
bind-world `B_Head`, дважды унося геометрию вверх. После поддержки position-only
узлов world translation mesh стала практически нулевой, а центр перешёл из
ошибочного `(≈0, 690, -349)` в `(≈0, 342, -187)`, рядом с остальными рогами.
При анимации rigid детали перемещаются через ближайшую bind-кость формулой
`modelWorld * inverseBoneBind * animatedBoneWorld`.

После добавления rigid animation обнаружено WPF-ограничение: geometry этих meshes
всё ещё замораживалась как обычная статическая `MeshGeometry3D`. Запись новых
`Positions` выбрасывала `InvalidOperationException: object is read-only`.
Теперь `Freeze()` применяется только к действительно статической geometry;
skinned meshes и rigid attachments ближайшей bind-кости остаются изменяемыми.
