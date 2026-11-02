# AIAction: настоящий virtual reset объекта команд

## Объект команд

PS2 Character setup `2A9700` при собственном word154==1 выделяет74,
вызывает `3731D0` и сохраняет возврат в character13C. Конструктор записывает
таблицу494710 и вызывает её настоящий reset. PC конструктор `599430`
записывает703090 и вызывает `5992E0`; прямой caller `4E2B57` сохраняет
указатель в своём поле18. Общность reset и layout подтверждена отдельно.
Эти два пути создания не объявляются одной и той же ветвью startup:
PC конструктор выставляет byte18=0, выбранный PS2 конструктор —1.
Это не доказательство платформенной ошибки или различия всех конструкторов.

На обеих платформах reset обнуляет:

- words04,08,0C,10,14,6C;
- bytes19..31 включительно,5C..60 включительно,64,70.

Остальные байты74-byte записи сохраняются, в том числе vtable,byte18,
координатные и промежуточные поля. Reset не является memset всей структуры.

## Вызовы AIAction

| Метод | Действия после настоящего command reset |
| --- | --- |
| GolemAttack, StormyAttack, YetiAttack v11 | own word3A8/3AC=0 |
| IcyAttack v11 | profile byte510=1, затем own word3A8/3AC=0 |
| Idle v10 | вызывает собственный v12 независимо от наличия command |

PC общие exits `5B79D0`, Icy `5BE2C0`; PS2 `24DC70`, `266540`,
`270CA0`, `259160`. Icy использует существующий global profile
765AD4/49FC7C; ленивое создание профиля не включено.

## Общая реализация PC Reset

Общий [исходник](../../../Winx/Code/wxAICommandRecordForAnalysis.h)
`wxAICommandRecordForAnalysis` хранит полные captured `116` bytes и исполняет
PC Reset `5992E0`. Это наша data facade: имя native класса, его registered
RTTI ID и физическое наследование не установлены. Компонент не регистрирует
выдуманный игровой класс и не восстанавливает native constructor или lifetime.
[PC layout](../../../Winx/Analysis/PC/wxAICommandRecordAbi.h) описывает только
наблюдённую запись и адреса отдельно от portable C++ layout.

По умолчанию Reset отказывает. Явный `PcCaptured703090` профиль дополнительно
проверяет raw PC word `0..3 == 703090` до всех записей. Этот word служит
аналитическим identity tag и не разыменовывается как C++ vptr. У исходного
Reset такого чтения нет; проверка ограничивает domain переносимой facade
тем receiver, для которого настоящий caller выбирает `703090` slot `04`.

Общие source destinations и их порядок следуют PC телу: сначала words
`04/08/0C/10/14`, затем byte `1E` перед `19..1D`, далее остальные bytes
`1F..31` в исходном, местами не возрастающем порядке, bytes `5C..60`, `64`,
word `6C` и byte `70`. Всего 38 native записей. Source покрывает те же byte
ranges в том же порядке; это не утверждение о native ABI или количестве
машинных stores portable C++. Остальные raw bytes, включая prefix, byte `18`
и padding, сохраняются.

Первый прямой source consumer —
[`wxStormyAttackAIAction` Exit](../../reference/classes/wx-stormy-attack-ai-action.md):
он разрешает живой borrowed command через storage-only host, вызывает общий
Reset и только после Completed обнуляет own target `3A8`. Нет callback,
подменяющего Reset готовым успехом или изменённой записью. Его Entry и Exit
профили допускаются отдельно. Missing storage, default-профиль и несовпавший
table tag дают аналитический отказ до соответствующих writes; это не модель
native fault или rollback.

Существующие foreign Reset contracts Hurt, Idle, Troll и IceGargoyle пока
сохраняются отдельно. Наличие общей реализации не квалифицирует автоматически
их адаптеры, native startup, PS2 source parity, owning lifetime или gameplay.

Для [`wxMikaelWandringAIAction` Entry](../../reference/classes/wx-mikael-wandring-ai-action.md)
facade также предоставляет узкие raw storage APIs для word08, word04 и
byte1B. Они проверяют тот же явный профиль и table tag до соответствующей
записи, сохраняют остальные байты и не меняют Reset с его38 destinations.
Это наши операции над captured данными, а не новые native методы командного
класса. Mikael сам вычисляет desired yaw, записывает08, затем выбранное04,
а после возврата собственного slot48 — byte1B=1. Нет callback с готовым
результатом heading, facing или движения. Настоящий producer Mikael3A4
не установлен; эта116-byte запись выбрана явным borrowed входом.

[`wxYetiAttackAIAction` whole slot54](../../reference/classes/wx-yeti-attack-ai-action.md)
использует отдельную узкую запись byte `23`: phase0 ставит `1`, phase1 — `0`
после собственных полей и, при нулевом deadline, чтения существующего
`wxGameTimer`. Это не Reset всей команды и не новая native command method.
Проверка явного профиля и raw table tag выполняется до каждой такой записи;
остальные bytes и исходный Reset остаются прежними.

[`wxFrogAttackAIAction` Exit](../../reference/classes/wx-frog-attack-ai-action.md)
предоставляет второй независимый потребитель narrow storage: он читает
собственный cached pointer `3E8`, обнуляет byte `1D`, повторно читает cache,
обнуляет byte `20`, затем свой target. Facade хранит именно предоставленную
живую запись, без callback результата и без вызова полного Reset. API для
`1D/20` применяют те же аналитические profile/tag admission и сохраняют все
остальные bytes. Native класс команды и полный producer Frog Entry от этого
не становятся квалифицированными.

[`wxTrollAttackAIAction` Exit](../../reference/classes/wx-troll-attack-ai-action.md)
и [`wxKnutAttackAIAction` Exit](../../reference/classes/wx-knut-attack-ai-action.md)
используют целые списки узких byte stores. Troll очищает `1D/20/21/22/23/24`,
Knut — `1D/5C/20/21/22/23/24`; перед каждым store заново читается
`own20 → owner144 → actor130`, после всего списка обнуляется target `3A8`.
Добавленные `21/22/24/5C` APIs проверяют прежние profile/raw-tag admission и
сохраняют остальные bytes. Это наши операции над данными, не native методы
командного класса; исходные Reset, `04/08/1B/23/1D/20` остаются прежними.
Поздний аналитический отказ сохраняет завершённый префикс списка. Ни один
потребитель не заменяет список полным Reset или callback готового результата.

## Исходная квалификация PC и PS2

Основные результаты исходного native исследования:8 constructor calls,24 exit calls,16 Idle entry calls;
48 полных возвратов,40 реальных reset entries и8 perception entries.
PS2 выполнил104 явно записанных SQ/LQ; исходные слова не менялись,
SP/RA/S0..S4 и upper64 восстановлены. Используются прежние ограниченные
CPU profiles; универсальный EE CPU и весь игровой runtime не заявляются.

Отдельный PC scout пытался связать command через Character с original factories Character/Node и literal полями. Вызов кандидата4F3ED0 остановился на598E3E при чтении0 после перехода в контейнерный helper. Нормальный контекст и полный контракт этого защищённого пути не доказаны; нельзя считать причиной установленную ошибку игры или завершённый command startup. Отказ/source сохранены, этот путь отложен. Для текущих независимых reset проверок никакой успешный startup не подставлялся.

10 platform updates по5: четыре exits и Idle entry, PC и PS2 независимо.
В этом исходном исследовании новых C++ классов, закрытых классов и отдельного
балла объекту команд не было. Общая PC data facade описана выше; исходная
native class identity от её наличия не появляется.
Ранее изученные v12/perception повторно не засчитываются.
