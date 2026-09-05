# `spTransformTrackEval`: PC reconnaissance

Статус: `scouted`, только PC. Карточка фиксирует ближайший runtime-узел между
SAN-треками и PRS узла; она не объявляет восстановленными внутренний формат
blend input или окончательное обновление world matrix.

## Идентичность и границы

- class ID: `0x5DAF152D`;
- engine RTTI base: `spTransformEval` (`0x87B0E260`), далее `spEvaluator`;
- registration object: `0x00768E90`, initializer: `0x006D79C0`;
- protected factory: `0x005FF090`, constructor entry: `0x005FF000`;
- vtable: `0x00711404`, семь слотов;
- registration getter: `0x005FEBA0`;
- PRS evaluator: `0x005FEBB0`.

Фабрика выделяет `0x78` байт. Конструктор сначала вызывает общий evaluator
initializer `0x0040E910`, задаёт `+0x10 = 0xFFFFFFFF`, обнуляет `+0x14`, затем
инициализирует два блока с шагом `0x30`. Часть constructor control flow защищена
косвенным переходом, поэтому оригинальные имена полей пока не присваиваются.

## Наблюдаемый PC-layout

| Offset | Размер | Осторожная интерпретация |
|---:|---:|---|
| `+0x00` | `0x10` | физический prefix `spBaseObject` |
| `+0x10` | `4` | slot transform target; до name binding равен `-1` |
| `+0x14` | `4` | число активных blend inputs |
| `+0x18` | `2 * 0x30` | два входа PRS-смешивания, внутренние поля ещё не названы |

Runtime-пробы уже показали запись найденного name/transform slot в `+0x10`:
missing target получает новый slot, а duplicate names направляют два разных
evaluator в один slot. Функция `0x005FEBB0` проходит входы с шагом `0x30`,
получает отдельные position/rotation/scale и смешивает несколько активных
результатов. Для rotation она вызывает quaternion helper `0x004648C0`; position
и scale смешиваются покомпонентно. При отсутствии входов выходные флаги PRS
сбрасываются.

Обе ветви завершаются `ret 0x1C`, то есть метод принимает семь stack-аргументов.
По записям в выходы аргументы 2–4 являются соответственно адресами трёх
компонентов PRS (`Vector3`, четырёхкомпонентный rotation, `Vector3`), а аргументы
5–7 — адресами их byte-validity flags. Точный тип и смысл первого аргумента
пока оставлен неизвестным: назначать ему имя `time` только по контексту рано.

Это подтверждает сам вычислительный мост, но не доказывает, кто планирует tick:
текущий `startLevel=2` runtime-маршрут выполнял binding, однако не входил в
`0x005FEBB0`.

## Воспроизводимость и открытые вопросы

[`inspect_transform_track_eval.py`](../../research/inspect_transform_track_eval.py)
проверяет SHA pristine PC EXE, хеши пяти тел, vtable, allocation extent и
ключевые offset/stride patterns. Текущий результат: `14/14 PASS`.

Неизвестны:

- оригинальные имена и точная структура полей каждого `0x30`-байтного входа;
- точный тип первого аргумента `0x005FEBB0` и оригинальные имена шести
  подтверждённых PRS-output аргументов;
- scheduler/caller, который запускает active animation tick;
- применение локального PRS к `spNode` и вычисление world matrix;
- PS2-реализация — намеренно отложена, поскольку для этих PC-выводов не нужна.

Следующий логический шаг — найти PC scheduler/caller evaluator и только после
этого ставить runtime-probe на активном игровом маршруте.
