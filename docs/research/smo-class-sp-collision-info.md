# Полный read-only разбор `spCollisionInfo`

Статус: завершён полный структурный разбор всех наблюдаемых PC/PS2-форм.
Редактирование полей остаётся runtime-непроверенным.

## Область проверки

| Корпус | Платформа | Объектов | SMO | Размер объекта |
|---|---|---:|---:|---:|
| `pc-working` | PC | 3 643 | 194 | 23–15 539 байт |
| `pc-pristine` | PC | 3 643 | 194 | 23–15 539 байт |
| `ps2-pristine` | PS2 | 3 308 | 158 | 36–15 539 байт |
| **Всего** | | **10 594** | **546** | |

Все объекты безымянные. Анализатор заново прочитал исходные SMO и объекты
внутри PS2 PCK, строго декодировал 10 594/10 594 экземпляра и аннотировал
31 286 нетерминальных полей.

## Serializer и поля

Оба executable независимо содержат `spCollisionInfoSerializer` и три enum-имени:

| Field | Имя executable | Payload | Наблюдаемость |
|---:|---|---|---:|
| 0 | `esfCollisionInfoPrimitive` | relationship к `spBoundingVolume` | 10 594 |
| 1 | `esfCollisionInfoGroup` | `UInt32` | 10 584 |
| 2 | `esfCollisionInfoTransform` | position + rotation + scale, 40 байт | 10 108 |

PC reader в ветке field 0 передаёт relationship-loader базовый class ID
`0x21CC76AF` (`spBoundingVolume`). Для field 2 он последовательно читает
`Vector3 position`, quaternion rotation и `Vector3 scale`. PS2 MIPS-код повторяет
тот же switch, базовый class ID и порядок значений. Writer обеих платформ пишет
те же три поля в порядке Primitive, Group, Transform.

Объект имеет ровно одну serializer-секцию и один завершающий пустой field 0.
Наследуемых секций у `spCollisionInfo` нет.

## Три подтверждённые формы

| Вариант | Поля | PC working | PC pristine | PS2 pristine |
|---|---|---:|---:|---:|
| `collision_info_full` | 0, 1, 2 | 3 401 | 3 401 | 3 306 |
| `collision_info_no_transform` | 0, 1 | 238 | 238 | 0 |
| `collision_info_primitive_only` | 0 | 4 | 4 | 2 |

Это варианты присутствия optional-полей, а не разные типы collider. Редкая
PC-форма без transform состоит из 236 inline `spMeshBV` под `spRenderNode` и
двух корневых ID-only ссылок на `spMeshBV`. Primitive-only форма на PC относится
к четырём inline `spOBBBV`; на PS2 — к одному `spBoxBV` и одному `spSphereBV`.

Отсутствие Group или Transform сохранено декодером явно как `null`. Мы пока не
подменяем его предполагаемым runtime-default: конструктор `spCollisionInfo` ещё
не восстановлен с достаточной уверенностью.

## Primitive relationship

Все 10 594 relationship разрешаются в реальный объект bounding volume.

| Target | Всего | PC working | PC pristine | PS2 pristine |
|---|---:|---:|---:|---:|
| `spMeshBV` | 10 162 | 3 492 | 3 492 | 3 178 |
| `spOBBBV` | 423 | 148 | 148 | 127 |
| `spBoxBV` | 6 | 2 | 2 | 2 |
| `spSphereBV` | 3 | 1 | 1 | 1 |

10 590 связей имеют inline-форму, и target является физическим дочерним объектом
`spCollisionInfo`. Ещё четыре записи — две одинаковые пары в working/pristine PC —
используют старую четырёхбайтовую ID-only форму и разрешаются в существующий
`spMeshBV`, не являющийся физическим child. Sized-reference форма в корпусе не
встречается.

`spCapsuleBV`, `spConvexBV` и сам `spBoundingVolume` зарегистрированы в обоих
executable и допустимы по базовому типу serializer, но сериализованных targets
этих трёх точных классов в исследуемых SMO нет.

## Collision group

| Значение | PC working | PC pristine | PS2 pristine | Всего |
|---:|---:|---:|---:|---:|
| 1 | 1 433 | 1 433 | 1 405 | 4 271 |
| 2 | 2 206 | 2 206 | 1 901 | 6 313 |
| поле опущено | 4 | 4 | 2 | 10 |

Поле является обычным `UInt32`; значения 1 и 2 — единственные наблюдаемые, но
декодер намеренно не превращает их в неподтверждённый enum и не запрещает другие
32-битные значения.

## Transform

40-байтовый payload имеет точный layout:

```text
0x00  Vector3    world position
0x0C  Quaternion world rotation (X, Y, Z, W)
0x1C  Vector3    world scale
```

Сериализованы 10 108 transform: 1 974 являются identity, 8 134 — non-identity.
Все компоненты конечны; длина исходного quaternion находится в диапазоне
`0.999999889..1.00000006`. Нулевых quaternion и нулевых компонентов scale нет.
Самое частое значение scale `(1,1,1)` встречается 3 213 раза в каждом PC-корпусе
и 3 121 раз на PS2; остальные близкие к единице значения сохраняют обычный
float-шум авторского transform.

Viewer нормализует quaternion для построения матрицы, но отображает разложенные
position/rotation/scale. При отсутствии field 2 существующий collision overlay
использует transform родительского node; это поведение Viewer, а не доказанный
сериализованный default движка.

## Физическая топология

| Родитель | Всего |
|---|---:|
| `spRenderNode` | 5 511 |
| `spPartitionNode` | 4 647 |
| `spNode` | 432 |
| корневой объект | 4 |

У всех 10 590 inline-форм физический child совпадает с target Primitive.
Четыре корневые PC-копии относятся к двум ID-only объектам и child не имеют.

## Согласованность корпусов

- `pc-working` и `pc-pristine`: 194/194 ресурсов имеют одинаковые
  последовательности полных serialized payload `spCollisionInfo`;
- PC/PS2: 158 общих путей, у 149 совпадает число collision-объектов;
- надёжно сопоставлены 3 075 объектов по пути и ordinal;
- primitive target type совпадает в 3 075/3 075 пар;
- variant, group и наличие transform совпадают в 3 073/3 075 пар.

Две несовпадающие пары соответствуют редким PS2 primitive-only box/sphere формам,
а не иной интерпретации layout.

## Реализация

- `SmoCollisionInfoDecoder` строго принимает три наблюдаемые формы, разрешает
  relationship и проверяет BV target/inline-child;
- `SmoSerializedFieldRegistry` и read-only Inspector показывают Primitive, Group
  и разложенный Transform;
- `research-db analyze-class ... spCollisionInfo` перечитывает все источники,
  записывает три варианта, 10 594 назначения и четыре evidence-записи;
- [`analyze_smo_collision_info.py`](../../research/analyze_smo_collision_info.py)
  формирует независимый read-only отчёт из schema v2.

## Открытые вопросы

1. Runtime-default Group и Transform при опущенных полях ещё не доказан.
2. Семантические названия групп 1 и 2 не восстановлены.
3. Runtime mutation Group/Transform не проверена, поэтому поля остаются
   read-only research data.
4. Последующий разбор `spMeshBV` завершён отдельно: он раскрыл основную
   переменную collision geometry и необязательный `wxFaceData`. См.
   [`smo-class-sp-mesh-bv.md`](smo-class-sp-mesh-bv.md).
