# 2026-08-11 — skeleton/palette UI и аудит исходников Butermix

## Вопрос

Почему `bloom_jeans.smo` не показывает кости рук в основной palette и как сделать
структуру skinning понятной в SmoViewer?

## Корпус и метод

Проверен локальный `bloom_jeans.smo`: все `spSkin` декодированы отдельно, а
активные blend indices сопоставлены с ненулевыми весами. Дополнительно построчно
сопоставлена предоставленная Butermix копия раннего проекта с текущим SmoViewer.

## Наблюдение

- файл содержит 95 `spNode`, 6 `spSkin` и 6 skinned mesh;
- каждая PC palette содержит 16 локальных slots;
- основной body mesh хранит ноги, корпус, голову и clavicle;
- arm/hand/finger bones распределены между `bloom_jeans-001…004`;
- один и тот же slot number в разных `spSkin` обозначает разные node object IDs.

## Решение

SmoViewer получил панель, объединяющую кости по object index и показывающую все
локальные palette references. Bind pose, attachment points, прозрачность модели и
mesh influence вычисляются из текущих strict decoders и vertex weights.

## Аудит Butermix

В копии найдены batch GLB exporter, переход SharpGLTF на `ToGltf2` и заготовка UV
Editor. UV-отрисовка и texture export не реализованы, а transform path устарел,
поэтому код не переносился напрямую. Направление работы и авторство Butermix
зафиксированы в `tools/SmoViewer/ACKNOWLEDGEMENTS.md` и changelog будущего релиза.

## Вспомогательные слои

По запросу отдельно исследованы индексы `85`, `88`, `91`, `92`, `95`, `119`,
`120`. Первые три collision-volume узла получили каркасные oriented boxes;
`SubMaster` и потомки отображаются как control/IK rig; `movement_tracker` и
`BLOOM` — как markers. `Ambient01` оставлен информационным объектом, поскольку
в нём нет пространственных данных, а class hash `0x5E6402DF` пока не опознан.
Панель переименована в «Слои просмотра», чтобы не ограничивать её skeleton UI.
