# `spAnimationManager`: PC frame, имена и lifetime

Checkpoint ночного цикла 2026-09-05/06, PC-first; PS2 deferred.
Статус `substantial`, не полная интеграция с engine main loop.
Original class name/ID доказаны. `Code/Sparkplug/spAnimationManager.cpp/.h` —
inferred placement, методы `ForAnalysis` и поля — analytical names.

## Идентичность и layout

Class ID **`0x5D214CC1`**, direct root `spBaseObject` **`0x415352A1`**.
Registration block `0x006D35B0..0x006D35D5`, record `0x0075F888`;
getter `0x00454360`. Global singleton — `0x0075F880`.
Factory `0x00454640` защищён; исполняемый resolved body `0x013DD4E0`
выделяет **`0x2C`**, вызывает constructor `0x00454540 -> 0x013CA630`.

| Offset | Подтверждённая роль |
|---:|---|
| `0x00..0x0F` | physical `spBaseObject` prefix |
| `0x10` | frame counter, initial **1** |
| `0x14` | next name slot, initial **1** |
| `0x18` | MSVC map allocator state/padding, не инициализируется |
| `0x1C` | map sentinel pointer |
| `0x20` | number of distinct name entries |
| `0x24 / 0x28` | borrowed controller head / tail |

Primary vtable `0x006E6E30` содержит **семь** slots:
`4545D0, 5B7A00, 4546A0, 40ECE0, 454360, 408350, 408370`.
С `0x006E6E4C` начинается строка `spAnimationManager`, не продолжение vtable.
Byte-exact evidence types и offset assertions добавлены в `Analysis/PC/SparkplugAbi.h`.

Constructor ставит singleton на себя. Destructor `4542E0 -> 13D8B50`
**безусловно** обнуляет global, освобождает map nodes/sentinel и вызывает root
destructor. Controller list он не уничтожает и не обходит: original lifetime
требует удалить controllers раньше manager. Два параллельных manager не образуют
стек singleton’ов: уничтожение старого тоже обнуляет global нового.

Clone `0x004546A0` создаёт новый manager, регистрирует source/result в clone
registry и вызывает root no-payload copy. Name map, counters и controller list
не копируются. Новый constructor меняет singleton как обычно.

## Реестр имён

`454370 -> 13B8300` принимает C string; native `std::string`/`std::map`
исполняются целиком. Сравнение **чувствительно к регистру**. Пустая строка
допустима. Для нового имени возвращается прежний `nextSlot`, затем счётчик
увеличивается. Для существующего возвращается его прежний ID и увеличивается
reference count. Это **не hash имени** и не локальный индекс одной анимации.

`453B10 -> 13D8400` находит имя, уменьшает references и удаляет node после
последней ссылки. Неизвестное имя — no-op. Освобождённый номер не переиспользуется:
следующая загрузка того же имени получает новый монотонный ID. Значение EAX
после unbind — scratch/iterator return, не доказанный bool status.

Node/sentinel имеют extent **`0x34`**: left/parent/right `+0/+4/+8`, std::string
state `+0x0C`, inline chars или heap pointer `+0x10`, length/capacity `+0x20/+0x24`,
**slot `+0x28`**, **references `+0x2C`**, color/is-sentinel bytes `+0x30/+0x31`.
Inline capacity 15; строки 16/80 bytes проверены с настоящим heap-string путём.
Tree mechanics — third-party MSVC STL, не новый Sparkplug class и не отдельные
engine coverage units.

Controller attach `0x004545F0`:

1. Получает evaluator через controller virtual `+0x24`.
2. Проверяет original `IsKindOf(0x5DAF152D)` через evaluator virtual `+0x18`.
3. При несовпадении возвращает 0, не получает имя и не меняет slot.
4. Повторно получает evaluator, через controller `+0x2C` берёт имя, вызывает
   bind и записывает ID в evaluator `+0x10`; возвращает этот ID.

Detach `0x00453E90` берёт имя через `+0x2C` и вызывает unbind. Он **не сбрасывает**
evaluator slot и не повторяет RTTI-check. Null evaluator/name contract не доказан.
PE initializers в guest не запускаются: positive RTTI-test использует fixture
единственного proven class-ID word в record `0x00768E90`; original IsKindOf
исполняется. Это не заявление о выполнении полной startup registration.

## Frame и `spController`

`0x004535A0` читает `spEngineCore` global `0x00755274` (lazy factory если null),
сохраняет float `engine +0xB8` **один раз**, увеличивает manager frame и идёт
head→controller next `+0x14`. При enabled byte `+0x10` вызывает virtual `+0x1C`
с захваченным delta. Next читается **после** callback, не заранее. Empty list
всё равно увеличивает counter; `0xFFFFFFFF -> 0` проверен. Изменение engine delta
в первом callback не меняет delta следующих controllers этого frame.

Native controller constructor `423010 -> 4C4210` требует уже существующий
manager; выставляет enabled=1, next/prev=0 и вызывает register
`453450 -> 405C10` (useful body `405C22`). Tail получает новую запись.
Destructor `423080` вызывает unregister `453480 -> 44AE20`: корректируются
head/tail и соседние links. Поля удалённого controller не очищаются.
Проверено удаление actor’ов из середины, головы и хвоста.

Защищённый copy `423100 -> 419AA0` вызывает root no-payload copy и переносит
**только enabled byte `+0x10`**, не links. Actor clone `0x005A3680` использует
именно этот inherited copy: fresh 40 slots, default speed=1, applies/advance=true,
пустые bindings. Source enabled сохраняется; actor-specific settings не копируются.
Новая копия самостоятельно добавляется в список manager.

## Исполнение и ограничения

`research/probe_pc_animation_manager.py`: **326/326 assertions** менеджера,
list, clone/controller-copy, attach/detach и настоящего manager→actor tick.
Внутренние assertions borrowed LifetimeFixture не прибавляются к этому числу.
Pristine inspector — **20/20**; portable manager — **49/49**, **CTest 9/9**.
После изменений повторены прежние 93 actor cases: **1857/1857 comparisons**.
`research/probe_pc_san_registry.py`: **33** checks на `bbush.san` (5 tracks),
**69** на `bflower.san` (17 tracks). На каждом: два original SAN reader-а
одновременно используют общие slot IDs/references, уничтожение одной animation
сохраняет вторую, последующее полное освобождение и reload дают новые IDs;
все tracked native allocations освобождены.

Host C++ использует original-named `spAnimationManager`, intrusive controllers,
реальный reconstructed `spActor` и track evaluator. Отличия безопасности явные:

- native C-string/allocator assumptions ограничены длиной, числом записей и
  переполнением counters; frame — finite delta, no recursive entry, ≤4096 visits;
- без manager разрешены standalone controllers для isolated tests; native
  constructor разыменовывает global без такой проверки;
- owner guard и очистка ссылок защищают host от manager-before-controller teardown;
  native этот порядок не обслуживает;
- current-controller removal имеет host cursor guard, не доказанный native
  callback lifetime contract. Сам manager должен жить до завершения frame;
- engine delta — явный host argument; [outer PC frame](native-pc-engine-frame.md)
  теперь исполнен и связывает его с delta второго `spTaskTimer` по engineB8;
- attach API принимает evaluator/name после двух native controller getters;
  полный owned actor tree binder добавлен [следующим checkpoint](native-pc-actor-owned-runtime.md);
- прежний portable SAN-reader сохраняет **non-owning resolver seam**; последующее
  [продолжение](native-pc-actor-binding.md) добавило отдельный owned-binding API
  на том же reader core и RAII leases с weak manager lifetime. Нормальный путь
  сверён original/portable на двух SAN; reversed-lifetime guards — host-only.

Все guest calls ограничены 100 000 instructions /2 s, process ≤30 s, memory arena
64 KiB. Явные allocator/char_traits/stream/name-owner/CRT/queue boundary fixtures
не считаются восстановленными engine methods. DLL/OS forwarding отсутствует,
игра/D3D не запускались. Приложения importer/viewer и game assets не менялись.

```powershell
python research\inspect_animation_manager.py
python research\probe_pc_animation_manager.py
python research\probe_pc_san_registry.py bbush.san
python research\probe_pc_san_registry.py bflower.san
```

Следующие связанные вопросы: actor input binding/priority capacity и ownership,
tree registration/nonempty teardown, внешний engine frame call, полноценная
FFPS/FAT loader transaction и renderer integration. Никакой PS2 реализации или
готовности importer/exporter этот checkpoint не объявляет.

Актуализация checkpoint5: outer app4C2D60→core41CD50→manager4535A0 исполнен,
bootstrap owner3C и inline task-timer deltaB8 доказаны. Actor owned tree/Start/
Rebind/Stop также уже перенесены. Открыты upstream capacity/borrowed-resource
lifetime, native event dispatch и frame→scene/world/render связь; portable app/
core целиком ещё не подключён. [Подробная граница](native-pc-engine-frame.md).
