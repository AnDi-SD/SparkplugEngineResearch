# `spSubscriptionManager`: исправление по оригиналам PC и PS2

10 сентября исследование конструкторов `wxGameFlowState` выявило две ошибки
прежней реконструкции: перепутанные PS2 Subscribe/Unsubscribe anchors и
рассылку в порядке добавления вместо порядка указателей. Изменён общий класс
`Sparkplug/Code/SparkBase/spSubscriptionManager`, отдельной реализации для
инструментов не добавлено. Это исправление доказанного расхождения с игрой.

## Оригинальное поведение и доказательство

| Операция | PC | PS2 |
|---|---:|---:|
| Subscribe | `416150` | `10E3B0` |
| Unsubscribe | `4163A0` | `10E210` |
| Dispatch | `415A20` | `10DE90` |
| Factory | `4165B0` | `10E750` |

PC-проверка исполняет original protected Subscribe entry и остальные методы,
с allocator/SEH обвязкой и тремя явно заданными borrowed получателями.
Callback только регистрирует вызов; подписки из callback не меняются.
10 сохранённых checkpoints: пустая группа, обратное добавление трёх адресов,
подавление duplicate, другая группа, неизвестный key, удаление среднего,
его повторное добавление, повторное удаление отсутствующего, удаление пустой
группы и всех групп. В обоих случаях с тремя элементами вызовы идут по
возрастанию адреса. Factory/add/remove/count/dispatch/destructor завершились;
51 из 51 игровых выделений освобождено, весь probe — 1,035 с.

PS2 доказан независимо чтением original instructions: Subscribe `10E3B0`
ищет/создаёт группу и вызывает insertion helper `10EC00`. Его `sltu` по
адресам `10EC38/10EC78` сравнивает указатели без знака; ветви обходят дерево
и подавляют равенство. `10E210` удаляет найденного подписчика и пустую группу.
Это статическое исследование PS2, запуск полной PS2-рассылки не заявляется.

PC вызывает virtual callback в `415AB6`, затем increment в `415ABD`;
PS2 — `10DF74`, затем `10DF7C`. Прежняя обвязка делала increment до callback
и необоснованно обещала стабильность итератора. Восстановлен исходный порядок;
поддержка изменения подписок во время callback остаётся открытой. Snapshot,
deferred queue или другое новое поведение не вводится.

После удаления последней группы manager остаётся жив: его собственные
32 байта и 32-байтный sentinel пустого дерева. Original destructor удаляет
оба. Дополнительная проверка одного `wxMenuGameFlowState` после удаления
оригинала и clone связывает два оставшихся выделения именно с global
`75537C` и его tree head `+18`; group count `+1C` равен нулю.
Эта атрибуция проверена для Menu, на остальные классы автоматически не перенесена.

## Исправление и адресная проверка

Общий `map<key,list<object*>>` заменён на `map<key,set<object*>>`, что сохраняет
проверенный порядок адресов и duplicate suppression. Исправлены PS2 anchors,
добавлен PC Subscribe anchor. `ForAnalysis` API по-прежнему получает key
отдельно: оригинальный Dispatch читает его из notification `+0C`; это явно
обозначенная аналитическая проекция, не объявление исходной сигнатуры.

Новый C++ regression сначала упал на старой реализации:
`original subscription dispatch follows pointer order, not insertion order`.
После исправления проверяется обратное добавление и удаление/возврат среднего
подписчика. Проверяется только `SparkBaseTests`, без сборки приложений или релиза.
При проверке обнаружена ошибочная локальная кодировка MSVC `/showIncludes`
prefix в старом CMake cache: исправлен только ignored cache; затронутые
translation units явно перекомпилированы, чтобы не доверять пропущенным deps.
Итоговый `SparkBaseTests` прошёл 1/1 за 1,37 с (CTest 1,43 с).

## Evidence и открытые границы

Каталог: `local-data/results/native-cycle-20260910-1900/subscription-audit/`.
`pc-run1.json`, сохранённый `probe_pc_subscription_contract.py`,
`windows/capture.json`, `windows3/capture.json`, `windows4/capture.json`,
`shared-before.log` и `shared-verified.log`; предыдущий dossier сохранён
как `prior-class-dossier.md`. Отдельный Menu capture:
`game-flow-family/pc-wxMenuGameFlowState-run2.json`.

Не установлены original container typedef/method names, полные notification
types, гарантия destructor-unsubscribe для каждого подписчика, reentry и
mutation lifetime. Неизменяемая группа и успешное освобождение проверенного
manager не доказывают эти дополнительные свойства. Общий класс не закрыт.
