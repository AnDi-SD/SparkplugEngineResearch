# PC actor: capacity, queries и fade-stop (CP67)

2026-09-07, pristine WinxClub.exe SHA-256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Original names классов сохранены; новые API names аналитические.

## Ёмкость playback

Ctor `5A3500` читает global `741654` по `5A3570` и вызывает `5A1600`.
Начальное PE значение40. Init `576820`, вызываемый по `40E388`, записывает
**2** по `5769C2`. Однако отдельный caller `51BD60` вызывает `5A1600(19)`
по `51BD81`, получая actor из `[owner+18]+128`. Следовательно, два слота
не являются универсальным invariant для всех actor. Эти внешние callers
установлены статически; весь игровой startup здесь не исполнялся.

`5A1600(count)` сначала публикует `actor+2C`, освобождает прежний `+28`,
затем запрашивает `count*60h`. Временные конструкторные defaults стираются
полным memset; окончательно записываются только индексы `+44`. Нулевая
ёмкость всё равно делает allocation request0, который внешний fixture
возвращает ненулевым. Повторная настройка уничтожает все прежние state
значения и **не перепривязывает** evaluator inputs. Её нельзя считать
безопасным resize активной анимации.

Выполнены whole factories с0/2/19/40 и reset40→19/2. Проверены все96 bytes
каждой native state. Source передаёт текущее default значение в обычный
constructor (включая clone factory) и предоставляет reset с отдельными
host guards. На нативном пути исходное поле weight намеренно изменено
перед reset и действительно стирается.

## Запросы

- `5A14C0(animation)` возвращает **первую** запись с таким pointer независимо
  от running/counter. Для NULL может вернуть первый свободный slot.
- `5A1500(animation)` возвращает true, если существует matching pointer с
  ненулевым unsigned binding counter `+48`. Флаг `running+4C` не проверяет.
  Существование записи и наличие используемой привязки — разные условия.

Source `FindPlaybackForAnalysis` / `HasUsedAnimationForAnalysis` сохраняют
эти различия, включая query(NULL). Возвращённая state view заимствована и
становится недействительной после успешного reset/уничтожения actor.

## Команда fade-stop

Весь `5A16D0(animation,duration,fallbackRate)`, ret0Ch:

1. Найти первый matching pointer, даже если запись уже inactive.
2. `+20 = 1/duration`, когда duration>0; иначе raw fallbackRate.
3. Установить fade mode `+10=3`, threshold `+58=0`, stopAfterFade `+3C=1`.
4. Остальные bytes неизменны. Не происходит немедленного Stop/rebind,
   event или flush. Missing pointer — no-op; duplicate меняет только первый.

Rate не масштабируется текущим weight. Отрицательный finite fallback
сохраняется: при последующем tick он может увеличивать weight. Existing
tick реализует наступающую позже остановку при weight<0. Positive duration2
и3, zero/negative duration, missing/inactive/duplicate отдельно сравнены.

## Перенос и проверки

`spActor` получил Set/GetDefaultPlaybackCapacity, ResetPlaybackCapacity,
FindPlayback, HasUsedAnimation и FadeOutAndStop с суффиксом ForAnalysis.
Source default остаётся40 до явного вызова конфигурации; обнаруженная
игровая запись2 не применяется скрыто ко всему portable приложению.

`pc-actor-controls`: **14 exact captures,171 native assertions**. В capture
сверяются20 установленных state полей плюс identity анимации, capacity и
оба query. Неизвестные state поля не представлены выдуманными source
members; их memset проверен отдельно по native bytes. Max8647 instructions,
5904 bytes arena; все factory/array/animation/manager allocations освобождены.

Host-only: capacity≤40 для нынешнего API; reset запрещён при controller
bindings, active states и по контракту при внешних retained views. Source
строит replacement до освобождения старого массива, сохраняя его при
allocation failure. Fade требует nonnull animation и finite inputs. В
оригинале локальные такие guards не найдены: NULL может выбрать пустой
slot, exceptional float/overflow и unsafe active reset остаются вне host
контракта. Эти ограничения не выданы за нативное поведение.

Whole Start с полностью занятыми двумя слотами и связная fade-stop/tick
последовательность проверены в [CP68](native-pc-actor-control-pipeline.md);
third-input invariant для actor19/40,
полный startup и callback reentry по-прежнему открыты.

```powershell
python research/native_workbench.py run pc-actor-controls --deadline-utc 2026-09-07T16:00:00Z
```
