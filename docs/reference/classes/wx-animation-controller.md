# wxAnimationController

Игровой контроллер команд `spActor`, истории запусков и событий анимации. Class ID `CD2B2876`; регистрационная и физическая база — `wxEntity` (`796A1869`). Реализация находится в [wxAnimationController.cpp](../../../Winx/Code/wxAnimationController.cpp), интерфейс — в [wxAnimationController.h](../../../Winx/Code/wxAnimationController.h).

Класс владеет actor, но не ресурсами анимации и не персонажем. Восстановленная логика подключает ещё отсутствующую базу `wxEntity` и внешние объекты через обязательный [host-интерфейс](../../../Winx/Analysis/Host/wxAnimationControllerHost.h). Встроенный переносимый запрос использует существующий `spActor::StartRequestForAnalysis`; отдельной копии проигрывателя или алгоритма смешивания здесь нет.

## Идентичность и layout

Все адреса, размеры и смещения в таблицах шестнадцатеричные.

| Свойство | PC | PS2 |
| --- | --- | --- |
| Размер | `188` | `1A0` |
| Размер физической базы | `124` | `130` |
| Constructor | `004FB210` | `002A7290` |
| Factory | `00401700` | `003F74A0` |
| Vtable | `006F670C` | `0049B450` |
| Destructor / deleting destructor | `004FB430 / 004FB6C0` | `002A71F0` |
| Clone | `004085D0` | `003F73E0` |
| Copy | `004FB4B0` | `002A7070` |
| Notification | `004FB780` | `002A6DA0` |
| Actor flags update | `004FB510` | `002A6FA0` |

Точные структуры отделены от переносимого C++-объекта: [PC layout](../../../Winx/Analysis/PC/wxAnimationControllerAbi.h), [PS2 layout](../../../Winx/Analysis/PS2/wxAnimationControllerAbi.h).

| Поле | PC | PS2 | Constructor |
| --- | --- | --- | --- |
| Borrowed character | `124` | `130` | null |
| Owned actor | `128` | `134` | null |
| Встроенный request, `44` байт | `12C` | `138` | Собственный constructor запроса |
| Mark события 9 | `170` | `17C` | null |
| Mark события 3 | `174` | `180` | null |
| Old / recent animation handle | `178 / 17C` | `184 / 188` | null / null |
| Счётчик запусков, uint32 | `180` | `18C` | 0 |
| Однократное разрешение actor | `184` | `190` | 1 |

Constructor сначала вызывает `wxEntity(true)`. Actor создаётся позднее при инициализации, а не вместе с контроллером.

Request содержит animation, mode, reverse byte, weight, fadeMode, fade-in duration/rate, fade-out duration/rate, transitionDuration, loop callback/cookie, timeMultiplier и initialTime по смещениям `00..34`. Начальные mode и weight равны **0**, fade-in и fade-out duration — **0.5**, timeMultiplier — **1**; остальные перечисленные значения нулевые. Это defaults встроенного запроса, не defaults переносимого `spActor::StartRequestForAnalysis` вообще.

Native слово request `38` constructor не записывает, byte `3C` становится 1, слово `40` — 3; padding также не обнуляется. Их исходные имена не установлены. Собственные команды контроллера не обращаются к этим трём полям; переносимый API запроса представляет подтверждённые поля существующего actor API и не заявляет native ABI.

## Инициализация и владение

Инициализация ставит forceEnable=1. Если actor отсутствует, создаётся `spActor`, ему передаётся root из `[entity24]+24`, затем контроллер добавляется в его наблюдатели. После этого всегда повторяется поиск персонажа по class ID `0003CC73` на inherited entity `24`; результат заменяет character. Повторная инициализация существующего actor не повторяет bind и подписку.

ReleaseActor сначала вызывает StopAll на actor. После возврата повторно читает собственное поле actor, при наличии уничтожает его и обнуляет поле. Destructor выполняет ту же последовательность и вызывает destructor `wxEntity`. История, отметки и счётчик при отдельном ReleaseActor не сбрасываются.

Copy сначала вызывает Copy базы. При false собственных действий нет. При успехе **уничтожает actor получателя без отдельного StopAll**, затем обнуляет его поле. Остальные собственные поля не копируются и не сбрасываются. Поэтому clone после создания и Copy сохраняет собственные начальные значения и не имеет actor; Copy в уже используемый объект сохраняет его request, marks, историю и count. Реализация Copy базы остаётся обязательной внешней зависимостью.

## Команды actor

| Команда | PC | PS2 |
| --- | --- | --- |
| Start | `004FB620` | `002A6CF0` |
| Stop | `004FB2D0` | `002A6CE0` |
| Fade-stop | `004FB2F0` | `002A6CD0` |
| StopAll и сброс count | `004FB310` | Собственная PC-обёртка |
| Reverse restart | `004FB3A0` | Отдельный адрес здесь не установлен |
| Установка actor timeMultiplier | `004FB6B0` | Отдельный адрес здесь не установлен |

Start выполняет действия в следующем порядке:

1. При ненулевом младшем byte interruptPrevious и ненулевом old вызывает `actor.Stop(old, false)`. До этого вызова история и запрос не меняются.
2. После callback заново читает recent, записывает old=recent, recent=newAnimation.
3. Обновляет только request.animation, mode, reverse=0 и fadeMode. Остальные параметры запроса сохраняются.
4. Ставит actor bytes `1C=1`, `24=1`, затем вызывает Start с указателем на встроенный request. Вызов может изменить request, например weight.
5. После возврата увеличивает count на 1 modulo 2³² и возвращает результат actor. Отказ `FFFFFFFF` также увеличивает count.

Stop передаёт suppressEvent=false. Fade-stop передаёт duration и fallbackRate=0. StopAll останавливает actor и **после** callback ставит count=0; marks и history сохраняются.

Reverse restart ищет playback данного animation. При отсутствии ничего не меняет. При наличии ставит request.reverse=1 и initialTime=`playback.field54 - playback.field34`, вызывает Stop(animation,false), после возврата записывает request.animation и вызывает Start. При этом mode, fadeMode, история и count собственным кодом не изменяются. Поля playback названы по смещениям, чтобы не приписывать разности неподтверждённые единицы времени.

Команды требуют допустимого actor. Их native обёртки не добавляют автоматическую инициализацию или проверки null.

## Отметки и события

Переносимое сообщение хранит code, payload18 и animation1C; `originalContext` позволяет адаптеру сохранить полный внешний контекст. Это не byte-exact native сообщение.

| Code | Действие |
| --- | --- |
| 3 | Обновить history/count, записать mark3, затем передать исходное сообщение наблюдателям |
| 9 | Обновить history/count и записать mark9; пересылки нет |
| 11 | Доставить тег текущему состоянию персонажа, затем звуку |
| `1C` | Инициализировать actor и связь с персонажем |
| Прочие | Игнорировать |

Для code 3/9 сначала сравнивается old с animation1C. При совпадении очищается old и уменьшается count; иначе проверяется recent и выполняется аналогичное действие. Даже если оба handle совпадают, очищается только первый. Сравнение с null также участвует: сообщение с null handle может уменьшить нулевой count до `FFFFFFFF`. Mark записывается независимо от совпадения с историей.

Для code 11 при отсутствующем character ничего не делается. Иначе выбирается текущее состояние через state machine персонажа и вызывается его обработчик. После callback контроллер **заново читает character**, затем audio emitter и variant. Variant берётся из byte `20D` PC / `219` PS2 внешнего объекта персонажа; при отсутствии этого объекта передаётся 0. Отсутствие emitter пропускает звук. Обнуление character самим callback не превращено в безопасный ранний выход: оригинал предполагает допустимый объект при повторном чтении.

`HasMarked(animation, consume)` возвращает true для null animation. Для остальных сначала проверяет mark3, затем mark9. При consume!=0 совпадение с mark3 очищает его и совпадающий mark9; совпадение только с mark9 очищает его. `ClearMarks(animation)` независимо очищает обе совпадающие отметки. Эти операции не изменяют историю и count.

## Переключение actor flags

Метод использует flags из `[entity24]+0C`. При существующем character с полем `148/154`=0 маска запрета равна `08`; иначе — `1A`. Разрешение действует при forceEnable!=0 либо отсутствии выбранных flags.

- При разрешении, ненулевом recent и actor.enabled1C=0 ставятся actor.enabled1C=1 и changed24=1.
- При запрете и actor.enabled1C!=0 ставятся enabled1C=0 и changed24=1.
- Остальные сочетания сохраняют исходные actor bytes, включая ненулевые значения, отличные от 1.

ForceEnable всегда сбрасывается, даже при отсутствующем actor или recent. Метод всегда возвращает false; аргумент не используется. Имя переносимого virtual `vfunc_34` использует PS2 byte-offset; PC соответствует `2C`.

## Границы реализации

Переносимый C++-класс наследует существующий `spNamedObject`. Недостающие `spEntity → wxEntity` представлены регистрационными метаданными и обязательными host-вызовами; это не разрешение на native casts к отсутствующим C++-базам. Исходные имена методов и заголовка неизвестны, `ForAnalysis` — наши аналитические имена.

Host должен сохранять actor identity, владение, изменяемый request и порядок callback. Особо существенны повторное чтение recent после Stop и character после state callback. Создание и bind actor, доставка событий, Copy базы и взаимодействие с аудио не подменяются заглушками успеха в восстановленном коде. Полная интеграция со сценой требует адаптера к существующему движку.

PC содержит защищённые входы команд и constructor; переносимый код описывает их установленное поведение. PS2 layout и собственные методы рассматриваются отдельно; полная EE-транзакция и все крайние режимы x87 не заявляются.

Связанные контракты: [подготовка запуска](../../game/characters/character-animation-playback.md), [события](../../game/characters/character-animation-events.md), [actor flags](../../game/entities/entity-runtime-predicates.md).
