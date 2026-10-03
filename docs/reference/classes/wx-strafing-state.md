# wxStrafingState

Class ID `22474D5C`, физическая и регистрационная база `wxCharacterState`.
Native размер PC/PS2 — `40`, selector — `0`. Собственное поле `3C` — pointer
на внешний observer, обнулённый конструктором. Clone получает constructor
defaults, базовые Copy/Reset сохраняют `3C`; destructor наследует базовый и
не заменяет отключение observer при выходе. Исходники — `Winx/Code/wxStrafingState`,
проверка — `WinxStrafingStateTests`.

На каждом входе `1C` при нулевом `3C` читает cached observer из entity
(`owner124/entity154` PC, `owner130/entity160` PS2). PC helper вызывается с
create=false: состояние не создаёт новый observer. При наличии pointer
захватывает entity control (`12C` PC / `138` PS2), отключает прежнюю binding
observer word `10`, сохраняет новую и подключает observer к ней при ненулевом
control. Затем записывает observer word `3C = 7F800000`, word `44 = 2`,
byte `64 = 0`. PC получает положительную бесконечность после float32 spill
произведения `FLT_MAX * FLT_MAX`; PS2 записывает те же bits непосредственно.

Эта привязка выполняется перед проверкой flag `1C` и может произойти при
повторном входе. При уже ненулевом собственном pointer настройка пропускается,
даже если cached observer entity изменился. Null cached pointer не препятствует
остальной логике входа.

Первый вход очищает flag `1C`, затем при нулевом entity pointer `148` PC /
`154` PS2 передаёт owner `24` сообщение `27E7` с boolean payload=true и word
`1C = 0`. Native boolean инициализирует только один byte payload `18`; его
старшие bytes не объявляются нулевыми. Portable host использует нормализованный
bool и не выдаёт его за native packet ABI.

Если entity motion (`entity130` PC / `13C` PS2, float word `4`) меньше `0.2f`,
вход задаёт `(key & F027FF8F) | 00200000`, выполняет lookup, ставит speed `2.0f`,
запускает mode=false, interrupt=true без освобождения старого pending и сохраняет
handle. Затем пишет прямой owner-control word `4 = 0.01f` и возвращает false.
Значение motion, не выполняющее сравнение «меньше», сразу вызывает базовый вход
с release и виртуальным update. Повторный вход потребляет completion: при
завершении ставит speed `1.0f` и вызывает базовый вход; иначе также пишет `0.01f`
в прямой control и возвращает false.

**Каждый выход `20`** сначала отключает собственный observer от его текущей
binding, обнуляет observer word `10` и собственный `3C`. Проверка flag `1E`
происходит после этого; отключение не ограничивается первым выходом.

Первый выход очищает flag `1E`, при нулевом entity pointer `148/154` передаёт
`27E7` с boolean=false. Motion меньше `0.2f` выбирает переход: release pending,
`(key & F047800F) | 00400000`, lookup, speed `2.0f`, mode=false,
interrupt=true и запись handle. Затем обнуляет прямой control word `4` и
возвращает false. Иначе ставит speed `1.0f` и возвращает базовый выход.
Повторный выход при completion также передаёт boolean=false при нулевом
`148/154`, ставит speed `1.0f` и возвращает базовый выход. При незавершении
обнуляет прямой control word `4` и возвращает false.

Update `30` захватывает entity motion object, суммирует его float words `14`
и `8`, сохраняет float32 и передаёт angle-normalization helper. Затем отдельно
перечитывает owner `24`/node `24`, получает строки `0` и `2` world orientation,
обнуляет вертикальную компоненту и нормализует каждый вектор. PC basis offsets
— `8C/A4`, PS2 — `90/A8`. Vector-angle helper и angle-pair helper вызываются
сначала для строки `0`, затем для строки `2`. Решение использует второй
результат angle-pair первым:

| Условие, в порядке проверки | Направление key |
| --- | --- |
| Entity motion `< 0.2f` | `0` |
| Forward pair `<= pi/4` | `10` |
| Forward pair `>= 3*pi/4` | `20` |
| Right pair `>= 3*pi/4` | `30` |
| Right pair `<= pi/4` | `40` |
| Остальные значения | `0`, обнуление прямого control word `4` |

После выбора применяется `F01FFFFF`; lookup с изменённым handle освобождает
pending и запускает mode=true, interrupt=true. Совпадение handle пропускает
только playback. Геометрические вызовы выполняются даже при малом motion.
PC верхняя граница — float32 `3F490FDB`, умноженный на `3` без float32 spill;
PS2 использует `4016CBE4`. Подтверждённый PC quiet NaN motion проходит ветвь,
в которой сравнение «меньше» ложно. PS2 scalar профиль здесь конечный.

`wxStrafingStateHost` явно отделяет заимствованные observer/control/consumer,
сообщения, сцену и общие математические службы. PC `596F80`, `597140`, `5971A0`
и PS2 `2930A0`, `292F20`, `292DB0` остаются обязательными службами с неизвестными
исходными именами. Нормализация PC `41D2D0` использует общий engine helper;
PS2 делает её inline через EE accumulator/SQRT. Host обязан предоставить
соответствующую платформенную арифметику; успешного общего fallback для неё нет.

PS2 подтверждены scalar suffix после геометрии, lookup masks, cached binding,
observer stores, порядок flags, motion/query и boolean message prefixes.
Это не подтверждение всего EE geometry пути. Permission `34` наследует true;
подключение настоящих внешних объектов и полное игровое соответствие класса
остаются открытыми.
