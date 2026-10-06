# spTemplate

`spTemplate` (`0x6D86570A`) имеет физическую и регистрационную базу `spNamedObject`. PC сохраняет точный путь `Z:\Sparkplug\Code\Sparkplug\spTemplate.cpp`. [Исходники](../../../Sparkplug/Code/Sparkplug/spTemplate.h) восстанавливают собственные dependency/copy policies над списком [`spTemplateObject`](sp-template-object.md); file loading и runtime attachment пока остаются отдельными границами.

PC factory `0x0059F630` выделяет `0x60` bytes и вызывает constructor `0x0059F540`, primary table — `0x00703D24`. PS2 factory `0x00153D70` выделяет `0x50` bytes, primary table — `0x0048E070`. Native constructors устанавливают пустой descriptor list `+0x14/+0x18/+0x1C`, обнуляют opaque word `+0x20`, поля `+0x24/+0x28`, создают container `+0x2C`, пустую строку `+0x38` и runtime поля после строки. PC использует 28-byte MSVC string, PS2 — 12-byte string, поэтому последующие поля и полный размер различаются на `0x10`.

Dependency check `0x0059EBD0` / `0x00153880` проходит descriptors в порядке списка. Для state `3` он получает template через loaded object `+0x1C` и его pointer `+0x14`. Равный target pointer или равный полный resource path означает зависимость; иначе он рекурсивно обходит найденный template. States `0..2` не образуют такую dependency edge. PC сравнение учитывает полную длину строки и bytes; совпадающий префикс не равен целому пути. State `3` dispatcher `0x005FDC21` также обращается через этот owner pointer.

Copy `0x0059F1B0` / `0x00153680` сначала отклоняет dependency. Затем очищает destination descriptors и вспомогательный container, копирует opaque word `+0x20`, отдельно клонирует каждый source descriptor и добавляет ненулевые результаты в исходном порядке. Inherited Named copy не вызывается: имя destination и его resource path сохраняются, новое имя clone остаётся пустым. Loaded state, runtime root и constructor-only поля не переносятся. Сам [`spTemplateObject`](sp-template-object.md) имеет собственный узкий copy contract, поэтому его descriptor name тоже не наследуется.

Portable список владеет descriptors через shared owners и запускает общую реализацию их clone. `spTemplateInstance::SetTemplateOwnerForAnalysis` — явная host injection известного native pointer; это не восстановленный entry создания instance. Отсутствующая state/owner информация даёт неизвестный dependency result и отказ copy до очистки destination. Recursion bound и запрет повторного добавления одного descriptor в тот же host list — host policies; оригинал такого ограничения обхода не содержит.

Собственные runtime entry известны отдельно от portable реализации. Имена операций в таблице описывают поведение; исходные имена методов не восстановлены.

| Операция | PC | PS2 |
| --- | ---: | ---: |
| Подсчёт descriptors | `0x0059E910` | `0x00153920` |
| Создание instance | `0x0059E7A0` | `0x00153570` |
| Передача input serializer | `0x0059E930` | `0x001535C0` |
| Поиск parent node | `0x0059E990` | `0x00153450` |
| Переключение runtime state | `0x0059FBA0` | `0x00152920` |

Подсчёт проходит `next +0x18` до null, а не читает сохранённое число `template +0x1C`. Создание instance вызывает его factory (`0x005FB870` / `0x00155C90`), записывает `instance +0x14 = template`, затем передаёт входной boolean его runtime entry (`0x005FC1D0` / `0x00154A60`). Таким образом, owner pointer, который читает dependency check, устанавливает сам template; это не произвольное поле descriptor.

Serializer wrapper создаёт [`spTemplateSerializer`](sp-template-serializer.md), записывает `spTemplateManager +0x20 = template`, вызывает serializer с template и input, удаляет временный serializer, обнуляет manager `+0x20` и возвращает полученный boolean. Здесь `+0x20` менеджера — временный контекст текущего template. Восстановление полного чтения файла требует собственного parser/serializer contract; wrapper сам не устанавливает успешность разбора.

Runtime switch пишет boolean в `template +0x54` на PC или `+0x44` на PS2 и также устанавливает текущий контекст manager `+0x20`. Повторное включение уже включённого template сначала вызывает ту же операцию с `false`. Затем идут несколько проходов descriptors: создание/удаление loaded objects зависит от state; для states `2/3` отдельный проход связывает nodes и переносит transform. На обычном завершении контекст manager обнуляется. PC branch при невозможности загрузить ресурс возвращает `false` раньше этого завершающего обнуления; rollback этого контекста нельзя предполагать.

Поле `template +0x28` содержит runtime root [`spNode`](sp-node.md). При включении null поля оригинал вызывает Node factory (`0x00421E20` / `0x001A9160`), удерживает результат через 16-bit counter `+0x08` и задаёт имя `Template Root`. Parent resolver возвращает этот root для `ParentID == -1`; иначе ищет descriptor по `ID +0x12C`, принимает loaded `spNode` либо root `spTemplateInstance +0x24`, затем при непустом имени разрешает дочерний node. Тип `+0x28` не следует смешивать с FAT.

Native constructor автоматически добавляет template в [`spTemplateManager`](sp-template-manager.md). Destructor (`0x005A0490` / `0x00153950`) сначала вызывает runtime switch с `false`, если runtime byte ненулевой; затем очищает и отпускает node `+0x28`, снимает регистрацию, удаляет descriptors и освобождает вспомогательные raw allocations, строку и compiler containers. Portable компонент не воспроизводит этот runtime manager/16-bit intrusive owner protocol и не заявляет законченный load→instance→unload сценарий. Поддержанный компонент ограничен descriptor dependency/copy graph и владением его host storage; наличие RTTI factory не означает закрытие native constructor/destructor или всех методов класса.
