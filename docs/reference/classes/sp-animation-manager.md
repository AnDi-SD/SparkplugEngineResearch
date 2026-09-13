# spAnimationManager

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spAnimationManager](../../../Sparkplug/Code/Sparkplug/spAnimationManager.h).

## Идентичность и layout

Class ID **`0x5D214CC1`**, direct root `spBaseObject` **`0x415352A1`**.
Registration block `0x006D35B0..0x006D35D5`, record `0x0075F888`;
getter `0x00454360`. Global singleton — `0x0075F880`.
Factory `0x00454640` защищён; исполняемый resolved body `0x013DD4E0`
выделяет **`0x2C`**, вызывает constructor `0x00454540 -> 0x013CA630`.

| Offset | Подтверждённая роль |
| ---: | --- |
| `0x00..0x0F` | physical `spBaseObject` prefix |
| `0x10` | frame counter, initial **1** |
| `0x14` | next name slot, initial **1** |
| `0x18` | MSVC map allocator state/padding, не инициализируется |
| `0x1C` | map sentinel pointer |
| `0x20` | number of distinct name entries |
| `0x24 / 0x28` | borrowed controller head / tail |

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

Controller attach `0x004545F0`:

## Frame и `spController`

Отличия безопасности явные:
