# PC actor: capacity, queries и fade-stop

## Ёмкость playback

Ctor `5A3500` читает global `741654` по `5A3570` и вызывает `5A1600`.
Начальное PE значение40. Init `576820`, вызываемый по `40E388`, записывает
**2** по `5769C2`. Однако отдельный caller `51BD60` вызывает `5A1600(19)`
по `51BD81`, получая actor из `[owner+18]+128`. Следовательно, два слота
не являются универсальным invariant для всех actor. Эти внешние callers
установлены статически; весь игровой startup здесь не исполнялся.

## Запросы

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

`spActor` получил Set/GetDefaultPlaybackCapacity, ResetPlaybackCapacity,
FindPlayback, HasUsedAnimation и FadeOutAndStop с суффиксом ForAnalysis.
Source default остаётся40 до явного вызова конфигурации; обнаруженная
игровая запись2 не применяется скрыто ко всему portable приложению.

Host-only: capacity≤40 для нынешнего API; reset запрещён при controller
bindings, active states и по контракту при внешних retained views. Source
строит replacement до освобождения старого массива, сохраняя его при
allocation failure. Fade требует nonnull animation и finite inputs. В
оригинале локальные такие guards не найдены: NULL может выбрать пустой
slot, exceptional float/overflow и unsafe active reset остаются вне host
контракта. Эти ограничения не выданы за нативное поведение.
