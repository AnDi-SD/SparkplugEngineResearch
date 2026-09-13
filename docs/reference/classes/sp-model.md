# spModel

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spModel](../../../Sparkplug/Code/Sparkplug/spModel.h), [spRenderableSerializer](../../../Sparkplug/Code/Sparkplug/spRenderableSerializer.h).

### Идентичность

`spModel` имеет class ID `0x763277DB`, direct base `spRenderable` (`0x4FDA4542`) и factory на обеих платформах.

| Факт | PC | PS2 |
| --- | ---: | ---: |
| initializer | `0x006D3EE0` | `0x00482380` |
| base-mesh setter | `0x00479E20` | `0x0015A5A0` |
| render body | `0x00479DC0` | `0x0015A640` |
| observed/exact extent | `0x60` | `0x58` |

### Два поля модели

После platform-specific base расположены только:

| Поле | Семантика |
| --- | --- |
| base mesh | intrusive relationship к базовому `spMesh` |
| projection group | `u32` |

Конструктор PC и PS2 задаёт пустую mesh-ссылку и projection group = 3. Отсутствующее сериализованное поле нельзя автоматически трактовать как эффективный ноль.

Обе copy functions копируют intrusive base-mesh reference и projection group.
Setter заменяет reference с обычным decrement/delete/increment contract и
после этого вызывает runtime-mode recomputation. Clone создаёт новый объект,
регистрирует пару в clone manager и вызывает virtual copy.

### Связь с рендерингом

PC `0x00479DC0` и PS2 `0x0015A640` реализуют одинаковую трёхфазную схему:
вызывают inherited pre-render slot, передают `baseMesh` в renderer interface
slot `9`, затем вызывают inherited post-render slot. На PC передаётся поле
`+0x58`, на PS2 — `+0x50`; concrete renderer bodies находятся по
`0x004BC670/0x001FF6A0`. Это закрывает разрыв от scene traversal до
платформенного mesh backend без присвоения выдуманного original method name.

Важная платформенная поправка: полный classifier `0..8` доказан только в PS2
body `0x00159F80`. Соответствующий vtable slot PC указывает на no-op
`0x0048EAA0`; PC debug/dump body `0x00479B00` занимает другой slot и не может
считаться classifier-ом. Пока material/pass types не реконструированы,
portable setter вызывает virtual hook: базовый `spRenderable` обнуляет mode,
но PC `spModel` оставляет его неизменным, как `48EAA0`. PS2 правила не
проецируются на PC. То же различие сохранено в copy, включая existing destination.

Bounds slots делегируют данным base mesh. PC `0x00479D20/0x00479D40/
0x00479DA0` и PS2 `0x0015A770/0x0015A700/0x0015A6D0` возвращают sphere,
min/max и validity либо нулевые значения при пустой связи. Sphere/min-max
getters **не проверяют** validity byte mesh28; это отдельный getter. `spModelSerializer` задаёт следующий контракт: reader запрашивает class ID
`spMesh` (`0x3F077B6C`). Загружаемый объект может быть concrete `spMeshData`, но
заужать само поле до этого leaf-класса было неверно.

### Границы описания

Добавлены RTTI/factory, clone/copy, renderable properties, base-mesh ownership,
projection group и раздельные PC/PS2 ABI structs/tests. Material/fog
типизированы широко (`spBaseObject`) как ownership facade, хотя concrete
`spMaterialData` и `spFog` уже имеют отдельные карточки; base mesh типизирован как
`spMesh`, а concrete `spMeshData` принимается полиморфно.

Остаются неизвестными source path, исходные имена draw/bounds slots,
PS2 classifier rules как original enum и
семантика каждого projection group. Platform mesh/vertex resources уже
частично восстановлены; следующий обязательный разрыв находится в concrete
material/fog types и pre/post-render state.

### Итог

`spModel` — двухсекционный renderable-контейнер. Первая секция принадлежит
унаследованному `spRenderableSerializer` и связывает объект с материалом, туманом
и параметрами порядка отрисовки. Финальная секция `spModelSerializer` связывает
его с обязательным `spMeshData` и необязательной projection group.

Все объекты именованы. В исследовательской базе декодированы 705 928
нетерминальных полей, всем 118 720 объектам назначен подтверждённый вариант.
Writer не включён: семантика чтения подтверждена, безопасность мутаций — нет.

### Serializer-секции и поля

Пустой field `0` завершает каждую секцию. Поэтому одинаковый номер поля в разных
секциях не означает одинаковую семантику.

| Секция от конца | Field | Ключ | Payload | Обязательность |
| ---: | ---: | --- | --- | --- |
| 1 | 0 | `renderable.material` | object relationship → `spMaterialData` | optional |
| 1 | 1 | `renderable.fog` | object relationship → `spFog` | optional |
| 1 | 2 | `renderable.alpha_sort` | `UInt32`, только `0` или `1` | обычно присутствует |
| 1 | 3 | `renderable.priority` | `UInt32` | присутствует вместе с field 2 |
| 0 | 0 | `model.base_mesh` | object relationship → `spMeshData` | обязательно |
| 0 | 1 | `model.projection_group` | `UInt32` | optional |

`esfModelBase`, `esfModelProjectionGroup`, `esfRenderableMaterial` и
`esfRenderableFog` присутствуют и в PC `WinxClub.exe`, и в PS2 ELF рядом с
регистрациями `spModelSerializer` и `spRenderableSerializer`. Названия
`AlphaSortEnable` и `Priority` дополнительно согласуются с таким же
унаследованным renderable-блоком `spSkin`.

У самого `spModel` нет position/rotation/scale. Размещение приходит от
`spNode`/`spRenderNode` либо от world transform содержащего
`spStaticRenderObject`; inline mesh/material bytes не являются transform-полями
модели.

### Семь вариантов присутствия полей

Это варианты одной структуры, а не семь разных runtime-классов.

| Вариант | Renderable fields | Model fields | PS2 pristine |
| --- | --- | --- | ---: |
| `model_full` | 0,1,2,3 | 0,1 | 37 450 |
| `model_no_projection` | 0,1,2,3 | 0 | 31 |
| `model_no_fog` | 0,2,3 | 0,1 | 51 |
| `model_no_fog_no_projection` | 0,2,3 | 0 | 0 |
| `model_no_material` | 1,2,3 | 0,1 | 78 |
| `model_no_material_no_projection` | 1,2,3 | 0 | 0 |
| `model_legacy_compact` | 0,1 | 0 | 0 |

Два legacy-объекта находятся в
`levels/gardenia/test_world_winx.smo`: `ramp-000` и `world_box-000`. Они
одновременно опускают `AlphaSortEnable`, `Priority`, `ProjectionGroup` и хранят
все три связи в четырёхбайтовой ID-only форме. Это валидный старый layout, а не
повреждение файла.

### Object relationships

Каждая присутствующая связь разрешается ровно в ожидаемый class ID. Нулевых и
неразрешённых ссылок нет; отсутствие material/fog выражается отсутствием поля.

| Связь | Отсутствует | ID-only | Sized reference | Inline SBOO |
| --- | ---: | ---: | ---: | ---: |
| material | 124 | 2 | 5 028 | 35 401 |
| fog | 54 | 2 | 40 229 | 270 |
| base mesh | 0 | 2 | 18 654 | 21 899 |
| material | 124 | 2 | 5 028 | 35 401 |
| fog | 54 | 2 | 40 229 | 270 |
| base mesh | 0 | 2 | 18 654 | 21 899 |
| material | 78 | 0 | 5 381 | 32 151 |
| fog | 51 | 0 | 37 366 | 193 |
| base mesh | 0 | 0 | 16 979 | 20 631 |

Это объясняет прежние «одинаковые строки» и зависимость соседних объектов:
модель хранит не свободное имя ресурса, а сериализованную связь по object ID;
для inline-варианта размер и type hash должны согласоваться с записью object
directory. Произвольная строка может не вызвать немедленный crash, но перестаёт
описывать тот же объектный граф.

### Числовые поля

`AlphaSortEnable`:

- PC: `0` — 23 349, `1` — 17 204, поле отсутствует у двух legacy-моделей;
- PS2: `0` — 23 132, `1` — 14 478.

`Priority` использует значения `0..30`, но диапазон разрежен. Самое частое
значение — `1` (32 615 PC и 31 948 PS2); затем идут `0`, `2`, `4`, `3`, `8`,
`7` и `18`. Редкие значения нельзя сводить к Boolean или маленькому enum.

`ProjectionGroup` содержит только `0` и `3`:

| Отсутствует | `0` | `3` |
| ---: | ---: | ---: |
| 2 934 | 35 047 | 2 574 |
| 31 | 34 874 | 2 705 |

### PC/PS2-сопоставление

В 215 ресурсах совпадает число моделей, что даёт 32 067 ordinal-пар:

| Совпало | Различалось |
| ---: | ---: |
| 31 549 | 518 |
| 32 036 | 31 |
| 31 946 | 121 |
| 32 007 | 60 |
| 32 023 | 44 |
| 32 067 | 0 |
| 32 067 | 0 |
| 31 141 | 926 |

Совпадение material/fog при платформенных различиях mesh подтверждает, что это
отдельные связи renderable-слоя, а не части mesh payload. Различия base-mesh
имён и numeric state фиксируются как платформенные данные, а не ошибки parser-а.

### Физическое окружение

Непосредственные родители PC-моделей: `spStaticRenderObject` — 20 469,
`spRenderNode` — 13 489, `spPartitionRenderable` — 6 541, `spSkyBox` — 54 и два
корневых legacy-объекта. На PS2: 20 913, 9 739, 6 907 и 51 соответственно.

Parent relation задаёт хранение/размещение, а поля самой модели задают визуальные
ресурсы. Поэтому одинаковый `spMeshData` может безопасно использоваться
несколькими reference-only моделями с разными `spStaticRenderObject` transforms.

### Открытые вопросы

1. Проверить в runtime, сохраняет ли PS2 отсутствующий field 1 constructor
   default `3`, и отдельно восстановить default защищённого PC constructor.
2. Проверить в игре безопасные fixed-size изменения `AlphaSortEnable`,
   `Priority` и `ProjectionGroup` отдельно на PC и PS2.
3. Проверить замену material/fog/base-mesh ссылок с сохранением class ID,
   object ID, inline size, directory bounds и parent topology.
4. До этих проверок не включать writer для `spModel`, несмотря на полный
   read-only decode.
