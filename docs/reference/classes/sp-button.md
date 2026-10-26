# spButton

`spButton` (`0x2CD270BB`) наследует [spWidget](sp-widget.md)
(`0x7CB52800`). [Общие исходники](../../../Sparkplug/Code/Sparkplug/spButton.h)
восстанавливают pressed selection и известный copy/clone contract.

PC extent `0x60`, PS2 `0x5C`. Собственные поля — owned C string, raw pressed
byte и borrowed pressed Node. Native constructor выделяет для строки два
байта и записывает NUL только в первый; pressed и Node равны нулю. Portable
`std::string` сохраняет пустое строковое значение и владение, а не byte layout
allocation. Смысл строки и исходное имя поля здесь не угадываются.

## Pressed selection

Проверка прежнего selected Node и его рекурсивное выключение совпадают с
Widget. Если прежний Node существует, но уже выключен, операция немедленно
возвращается.

При ненулевом pressed byte и непустом pressed Node выбирается этот Node,
включая случай disabled Button. В этой ветви selection index **не
перезаписывается**. Если pressed Node отсутствует, выполняется обычный
Widget выбор disabled/highlighted/normal, включая normal fallback и запись
index. Выбранный Node получает raw visible byte и recursive flag `1`.

## Нотификации

Известная PC-часть Notify добавляет к Widget следующие операции:

- При `0x0F` и ненулевом enabled читается слово `+0x18` объекта из входного
  payload. Если оно нулевое, текущему target посылается временный frame
  `[0x19,0,0,0,this,0,ownedString,0]`, затем pressed присваивается `1` и
  вызывается виртуальный Refresh. Ненулевое слово пропускает эту own-часть.
- При `0x10` или `0x12`, ненулевых enabled и pressed, pressed сначала
  обнуляется, затем вызывается Refresh.
- После own-части тот же исходный message передаётся Widget Notify. Он
  заново читает enabled и code. Поэтому `0x0F` может сначала отправить новый
  frame `0x19`, а затем переслать исходный `0x0F`; callback, сбросивший enabled,
  предотвращает вторую пересылку. `0x12` может вызвать Refresh дважды.

Pressed `1` записывается после helper callback; последующий Refresh читает
актуальные pressed и state Nodes. Widget-часть заново читает enabled и target.
Helper получает указатель строки перед чтением target и до callback; после
возврата этот payload самим helper больше не разыменовывается. Временное
сообщение заимствует его синхронно, без дополнительной копии или strong
reference. Доступ к opaque input word `+0x18` выделен в provider и не объявляет
исходный тип события.

При `0x1C` Button сначала ищет точное буквальное имя-фрагмент `PUSHED`
с recursive/substring flags `1` и сохраняет borrowed pressed Node. Если он
найден, Node рекурсивно выключается и всем его коллизиям присваивается group
`4`. Затем выполняется весь Widget-протокол `0x1C`, включая поиски normal,
disabled, highlighted и базовый `GUICollision`. Pressed byte и прежний
selection index этот путь не меняет.

## Копирование и владение

Copy сначала выполняет Widget copy, затем освобождает прежнюю строку
destination, создаёт независимую копию исходной C string и копирует pressed
byte с borrowed pressed Node. Изменение или уничтожение исходного Button
не уничтожает строку destination. Widget visible byte по-прежнему не
копируется. Native allocator exceptions и точное heap ABI не реконструированы.

Destructor освобождает собственную строку перед teardown Widget/GUIObject.
Borrowed state Nodes не освобождаются как самостоятельные owned поля.

`SetButtonStateForAnalysis` — host input для populated copy/selector, а не
восстановленный game setter. Квалификация `0x1C` относится к стабильным
графам фактических Node; изменения графа во время обхода и неизвестные
derived overrides остаются открытыми. Полный PS2 Notify, нормальный GUI
startup и визуальная работа GUI также требуют отдельной проверки.
