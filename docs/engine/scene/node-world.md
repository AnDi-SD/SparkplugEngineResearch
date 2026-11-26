# PC spNode: world-update и защищённые переходы

## Снятая неопределённость на входах

| Entry | Protected pointer → resolved target | Подтверждённая операция и возврат |
| --- | --- | --- |
| world `0x00421420` | `0x013B1510 → 0x00442FA6` | `mov ebx,[esi+0xB0]`; jump `0x0042142E` |
| quaternion `0x00420640` | `0x013B13CC → 0x004023A9` | `or dword [esi+0xB0],1`; jump `0x00420657` |
| affine `0x00461D70` | `0x013B162C → 0x004061E8` | `sub esp,0xC0`; jump `0x00461D76` |

## Полный transform-путь `0x00421420`

Native signature: `this=node`, один stack argument inherited flags, `ret 4`;
virtual byte offset `+0x30`. Порядок:

Без parent: world position/scale копируются из local, orientation тоже, кроме
billboard-пути. С parent:

| Mask | Включён | Выключен |
| --- | --- | --- |
| `0x10000` | local position × parent rotation + parent position | **прежняя world position сохраняется**, local не копируется |
| `0x40000` | local scale × parent scale; parent scale также применяется к position при `0x10000` | копируется local scale |
| `0x20000` | non-billboard: local matrix × parent world matrix; billboard сохраняется | копируется local orientation, **даже поверх billboard** |

Cached поля: position `+0x74`, scale `+0x80`, orientation `+0x8C`.
Parent `+0x2C`, flags `+0xB0`. Math `0x00420350/0x00420C00` использует
row-vector и обычный индексный matrix product.

## Billboard `0x004207E0`

Null camera даёт identity. Иначе forward — нормализованная отрицательная третья
строка camera world orientation, initial up — `(0,1,0)` из `0x00740328`.
Normalize helper `0x0041D2D0` обнуляет слишком короткий вектор (`length <= .001`).

- Axis `1`: сохраняется fixed up; при почти параллельных up/forward
  (`abs(abs(dot)-1) < .001`) forward заменяется отрицательной normalized
  второй строкой camera. Затем normalised right = up×forward,
  forward = normalized(right×up).
- Axis `2` и raw flag combination `3`: сохраняется camera-facing forward;
  при почти параллельном случае up заменяется normalized второй строкой camera.
  Затем right = normalized(up×forward), up = normalized(forward×right).
- `0x00420470` записывает строки `up×forward`, up, forward. В вырожденной
  ветви совпадающих входных basis vectors он использует неинициализированный
  stack local. Portable слой **не воспроизводит неопределённые значения**:
  отклоняет нечисловую/вырожденную camera basis и явно возвращает false.

Camera — уже подготовленный cache input. Её обновление и реальный frame caller
здесь не восстановлены.

## Преобразование локальной точки в мировые координаты на PC

`00420660` берёт позицию `+0x74`, масштаб `+0x80` и матрицу `+0x8C`
из подготовленного кэша узла. Он сохраняет масштабированные X, Y и Z в
расширенной точности x87. Для выходного X слагаемые идут в порядке `(Z+Y)+X`,
для Y и Z в порядке `(Z+X)+Y`. Промежуточные координаты округляются до binary32
перед прибавлением позиции. Результат записывается по порядку X, Y, Z.
[Аналитический исходник](../../../Sparkplug/Code/Sparkplug/spNodeWorldPoint.cpp).

Точный интерфейс принимает `±0` и нормальные значения с модулем от `2^-16` до
`4096` в словах binary32. Округление соответствует ближайшему чётному
(`FPCW=037F`). Исходная и выходная точки могут занимать одни и те же три слова.
Процедура считывает вход до записи результата и использует кэш только для чтения.

## Умножение вектора на матрицу ориентации на PC

При обновлении мировой позиции `spNode` вызывает `0x00420350` из двух
ветвей. Одна передаёт локальную позицию, другая предварительно умножает её
на масштаб родителя и сохраняет три компоненты в binary32. Обе передают
матрицу ориентации родителя и пишут результат в отдельный временный буфер.
В генераторе частиц к этому примитиву обращается вызов `0x0048C495`.
[Аналитический исходник](../../../Sparkplug/Code/Sparkplug/spNodeMatVec.cpp).

Для каждого столбца матрицы слагаемые складываются в порядке `(Z+Y)+X`.
Промежуточные суммы имеют 64 значащих бита, каждая выходная компонента
округляется до binary32. В обоих случаях используется округление к
ближайшему чётному (`FPCW=037F`). Интерфейс принимает нули обоих знаков
и нормальные значения с модулем от `2^-32` до `2^27`.

Архив содержит 98 прямых вызовов и 294 выходных слова. Промежуточный
вектор для `trail_mesh` вычислен по сохранённым данным узла и родителя.
Три итоговые координаты совпали с сохранённым состоянием сцены.

## Умножение двух матриц 4×4 на PC

`0x00426B00` получает левую матрицу в ECX, указатели на правую матрицу и
результат — в аргументах стека. Обе матрицы хранятся построчно. Для каждой
ячейки `(r,c)` складываются произведения `left[r,k] * right[k,c]`. Порядок
`k` зависит от ячейки. Все три суммы в каждой ячейке округляются до
64 значащих битов x87, затем `FSTP` округляет готовую ячейку до binary32.
При `FPCW=037F` обе операции округляют к ближайшему чётному. Оригинал
рассчитывает ячейки по столбцам во временных 64 байтах на стеке. Вызов
`0x0041D330` переносит в результат 16 сырых слов в построчном порядке.

[Аналитический интерфейс](../../../Sparkplug/Code/Sparkplug/spMatrix4ProductPc.h)
принимает три непересекающиеся матрицы. Допустимые входные слова содержат
нули обоих знаков или нормальные значения с модулем от `2^-64` до `2^32`
включительно. В результате возможны субнормальные значения и нули обоих
знаков. При отказе все 16 выходных слов сохраняют прежние значения.

Существующие пути skin, UV, анимации и рендерера используют отдельное
вычисление в binary64. Сохранённые кэши матриц рендерера подтверждают
порядок множителей и итоговые композиции на своих входах.

## Affine builder `0x00461D70`

Полная signature: `this=output Matrix4`, stack arguments = position pointer,
orientation Matrix3 pointer, scale pointer, `ret 0x0C`.
`0x00462680` расширяет Matrix3 до affine Matrix4; `0x00426B00` умножает
`diagonalScale * orientation`; translation пишется в элементы `12..14`.
Последний столбец `[0,0,0,1]`. Это закрывает сам builder перед известным
skin palette `inverseBind * boneWorld`, но не весь renderer/frame путь.

`spNode.*` теперь содержит world caches, exact inheritance gates, virtual-tree
эквивалентный обход, billboard и affine output. Ownership остаётся host
`shared_ptr`; collisions и scene registrations в этом классе ещё не представлены.
Raw local setters не подразумевают native side effects: invalidation вызывается
явно. `spNodeController` исправлен: **rotation-only** direct/blend тоже ставит
dirty bit, что доказал resolved setter.

Harness guard tests: 7/7 (hash gate, внешний deadline/failure propagation, arena bound, unmapped access, запрет syscall, instruction cap).
