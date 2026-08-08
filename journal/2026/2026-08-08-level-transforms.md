# 2026-08-08 — первый transform path для составных SMO-уровней

## Вопрос

Почему `Alfea02.smo` и другие level SMO собираются как набор объектов неверного
масштаба и положения, хотя отдельные character-модели выглядят правдоподобно?

## Эталонный набор

Обязательный файл и четыре воспроизводимо выбранных level-файла:

- `Levels/Alfea/Alfea02.smo`;
- `Levels/Alfea/Alfea01.smo`;
- `Levels/Swamp/BMS_05.smo`;
- `Levels/Cloud01/cloud01_02.smo`;
- `Levels/Alfea/Alfea_night_01.smo`.

## Независимое подтверждение из WinxClub.exe

В PC executable присутствуют serializer-токены:

- `esfNodePosition`;
- `esfNodeRotation`;
- `esfNodeScale`.

В начале сериализованных node/model-объектов им соответствуют поля:

| Field type | Payload | Смысл |
|---:|---:|---|
| 0 | 12 байт | position, три `Single` |
| 1 | 16 байт | quaternion XYZW, четыре `Single` |
| 2 | 12 байт | scale, три `Single` |

Локальная матрица для row-vector conventions System.Numerics строится как
`Scale * Rotation * Translation`. Для static mesh под `spModel` матрицы
умножаются от model к родителям. Skinned character meshes пока не меняются.

## Результат строгого decode

| Файл | Meshes | UV meshes | Meshes с model transform |
|---|---:|---:|---:|
| `Alfea02.smo` | 702 | 693 | 58 |
| `Alfea01.smo` | 554 | 548 | 7 |
| `BMS_05.smo` | 646 | 640 | 3 |
| `cloud01_02.smo` | 263 | 263 | 9 |
| `Alfea_night_01.smo` | 576 | 564 | 7 |

Остальные meshes либо уже содержат координаты сцены, либо не имеют
подтверждённой model-матрицы. Глобальная нормализация отдельных объектов не
применяется.

## Material binding для уровней

Character-правила (`spSkin` chunks, render-node family и largest primary atlas)
нельзя применять к уровню. Они назначали одну texture несвязанным объектам.
Для сцен с более чем 100 meshes этот fallback временно отключён; остаются только
точные interval bindings.

Для `Alfea02.smo` число texture bindings изменилось с 700 почти поголовных до 83
точных (82 одновременно имеют подтверждённые UV). Это уменьшает визуальное
покрытие, но убирает заведомо неверные atlas. Следующий этап — разбор явных
material relation-полей level-графа.

## Открытые проверки

- визуально подтвердить порядок quaternion и matrix multiplication на шкафе и
  других заметных объектах `Alfea02`;
- определить transforms неизвестных partition/sector classes;
- восстановить level material relations без character fallback;
- отделить уже мировые vertex positions от локальных по структурному признаку,
  а не по числовому диапазону.

## Исправление: placement хранится в spStaticRenderObject

Визуальная проверка показала, что одного node transform недостаточно: правильные
отдельные модели оставались наложены друг на друга около локального начала координат.
В `WinxClub.exe` найдены имена `esrosfStaticRenderObjectTransform` и
`esrosfStaticRenderObjectInvTransform`, а в `Alfea02.smo` — 672 объекта класса
`spStaticRenderObject` (`0x56D67170`).

Начало каждого такого объекта содержит подтверждённую пару полей:

| Field type | Payload | Смысл |
|---:|---:|---|
| 1 | 64 байта | world transform, 16 `Single` |
| 2 | 64 байта | inverse transform, 16 `Single` |

Матрица записана row-major и непосредственно совместима с row-vector conventions
`System.Numerics.Matrix4x4`: translation находится в `M41..M43`. Например, у первого
`Darch_A01` координаты имеют порядок тысяч единиц — их отсутствие и складывало сцену
в одну точку. `spStaticRenderObject` является родителем размещённого `spModel`, поэтому
его матрица теперь включается в model-to-world chain.

Некоторые inverse-пары математически вырождены: движок использует нулевой scale для
скрытых объектов. Поэтому наличие второго 64-байтового поля проверяется структурно,
но обратимость не является условием принятия transform.

## Система координат и зеркальность

PC-версия Sparkplug рендерилась через Direct3D в left-handed системе координат, а WPF
`Viewport3D` использует right-handed систему. Прямая передача `(X,Y,Z)` поэтому давала
зеркальное отображение уровня. Конверсия теперь выполняется один раз после полного
model-to-world transform: `(X,Y,Z) → (X,Y,-Z)`. Поскольку это reflection с отрицательным
determinant, для каждого треугольника одновременно меняется winding `A,B,C → A,C,B`.
Так сохраняются лицевая сторона полигонов и освещение.

## Новое наблюдение после восстановления материалов

После включения настоящих texture/vertex colors стало видно, что transform path всё ещё
неполон. В `Alfea02.smo` 478 из 702 meshes получают матрицу через родительский
`spStaticRenderObject`, но заметная группа отдельных объектов остаётся наложенной около
центра сцены. Это уже не ошибка масштаба геометрии или текстуры: сами предметы выглядят
правильно, но используют локальные coordinates.

Следующая проверка должна разделить оставшиеся 224 meshes на две группы:

- геометрия, уже экспортированная в мировых координатах (полы, стены, sector chunks);
- экземпляры, placement которых находится в другой ветке SMO либо в соседних `.spl/.spt`.

Нельзя назначать им ближайшую матрицу эвристически: это снова соберёт правдоподобные
предметы в неправильные комнаты. Нужна подтверждённая relation/instance связь.

## Проверка соседних SPT/SPL и optional node scale

`Alfea02.smo` действительно загружается из `Alfea01_02.spt`: в SPT находится прямая
строка `Levels\Alfea\Alfea02.smo`. Однако SPT не содержит placement основной геометрии.
Он перечисляет игровые компоненты (`wxCabinetPuzzle`, door/cinematic triggers и другие)
и связывает их по именам с узлами SMO; transform таких component records в проверенных
записях identity. `Alfea01_02.spl`, в свою очередь, ссылается на сам SPT и внешние
gameplay templates. Следовательно, переносить координаты мебели из SPT/SPL нельзя.

Внутри SMO найдена отдельная причина части потерянных transform. `spRenderNode hat01`
содержит position type `0` и rotation type `1`, но не содержит scale type `2`: единичный
scale не сериализуется. Старый decoder требовал все три поля и отвергал position/rotation
целиком. Scale теперь optional с default `(1,1,1)`. На `Alfea02` число meshes с
неединичным world transform выросло с 478 до 488.

Оставшаяся центральная группа относится главным образом к root-level `spRenderNode`
без `spStaticRenderObject`. Часть их meshes уже содержит baked world coordinates
(например семейство `cadreZ03`), а часть выглядит как локальные prototype/gameplay assets
(`makeup`, `Muza_*`). Их нельзя одинаково трансформировать: следующий шаг — подтвердить
структурный признак prototype nodes и не добавлять неинстанцированные ресурсы в сцену.
