# PC spNode: world-update и защищённые переходы

Этап 2026-09-05, продолжение [animation runtime](native-pc-animation-runtime.md).
Pristine PC SHA-256:
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Original member/method spelling не найден; названия операций ниже аналитические.
PS2 не исследовался. Игра не запускалась, её файлы не изменялись.

## Снятая неопределённость на входах

Общий `.rld`-обработчик `0x00888940` удалось пройти на неизменённых PC-байтах
в Unicorn 2.1.4. Он обновляет указатель перехода **в гостевой памяти** и
передаёт управление инструкции, уже присутствующей в исходном `.text`:

| Entry | Protected pointer → resolved target | Подтверждённая операция и возврат |
|---|---|---|
| world `0x00421420` | `0x013B1510 → 0x00442FA6` | `mov ebx,[esi+0xB0]`; jump `0x0042142E` |
| quaternion `0x00420640` | `0x013B13CC → 0x004023A9` | `or dword [esi+0xB0],1`; jump `0x00420657` |
| affine `0x00461D70` | `0x013B162C → 0x004061E8` | `sub esp,0xC0`; jump `0x00461D76` |

Для этих fixtures не нужны startup, импорт Windows DLL, игра или PS2-аналог.
Это не доказывает, что любой защищённый entry в EXE имеет такой же контракт.
Три соответствующих прежних вопроса о скрытых preambles закрыты.

## Полный transform-путь `0x00421420`

Native signature: `this=node`, один stack argument inherited flags, `ret 4`;
virtual byte offset `+0x30`. Порядок:

1. Если `flags & 0x300000`, вычисляется billboard orientation, **до dirty gate**.
   Camera берётся из `spEngineCore` singleton `0x00755274`, поле `+0x1C`.
2. При `(flags | inheritedFlags) & 1` пересчитываются cached PRS.
3. В этой dirty-ветви проходится collision vector `+0x68..+0x6C`, каждый объект
   передаётся `this` в `0x004651E0`.
4. Всегда обходятся children; их vslot `+0x30` получает
   `(текущие flags | inheritedFlags) & ~2`. Не проверяется enabled `0x200`.
5. После детей собственные flags очищаются `&= ~7`.

Без parent: world position/scale копируются из local, orientation тоже, кроме
billboard-пути. С parent:

| Mask | Включён | Выключен |
|---|---|---|
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

## Affine builder `0x00461D70`

Полная signature: `this=output Matrix4`, stack arguments = position pointer,
orientation Matrix3 pointer, scale pointer, `ret 0x0C`.
`0x00462680` расширяет Matrix3 до affine Matrix4; `0x00426B00` умножает
`diagonalScale * orientation`; translation пишется в элементы `12..14`.
Последний столбец `[0,0,0,1]`. Это закрывает сам builder перед известным
skin palette `inverseBind * boneWorld`, но не весь renderer/frame путь.

## Исходники и проверки

`spNode.*` теперь содержит world caches, exact inheritance gates, virtual-tree
эквивалентный обход, billboard и affine output. Ownership остаётся host
`shared_ptr`; collisions и scene registrations в этом классе ещё не представлены.
Raw local setters не подразумевают native side effects: invalidation вызывается
явно. `spNodeController` исправлен: **rotation-only** direct/blend тоже ставит
dirty bit, что доказал resolved setter.

```powershell
python research/probe_pc_node_world.py
python research/test_pc_instruction_emulator.py
```

179/179 guest-instruction checks: три настоящих protected entries, quaternion
setter, 32 PRS/matrix fixtures с восемью masks, трёхуровневый virtual tree,
clean/inherited dirty, шесть camera inputs × три billboard flags × четыре
контекста, порядок collision calls. Только collision callee заменён явно
обозначенным recording seam; его реализация этим не проверяется.

`SparkplugNodeWorldTests`: 83 portable checks, включая rotation-only→world.
Harness guard tests: 7/7 (hash gate, внешний deadline/failure propagation,
arena bound, unmapped access, запрет syscall, instruction cap).

Harness отображает не более 64 MiB image + два 64 KiB arena; для каждого вызова
100 000 инструкций / 2 секунды, для дочернего Python-процесса 30 секунд.
Fault не вызывает автоматического расширения памяти. Нет OS/API forwarding.
Это ограниченная эмуляция, **не Windows Sandbox и не игровой тест**.
Зависимости устанавливаются отдельно, автоматически ничего не скачивается;
[официальный API Unicorn](https://www.unicorn-engine.org/docs/tutorial.html).

Открыты: outer frame caller; dirty bits `2/4` по смыслу; collision/scene API,
новые unresolved PC entries и полный serializer runtime. Исходные пути
`spNode.h/.cpp` inferred, не подтверждены новым source-string evidence.
