# wxMinotaurAttackState

Class ID `30062CB0`, физическая и регистрационная база `wxCharacterState`.
PC и PS2 выделяют `0x40` байт; selector `10` равен `3`. Собственный byte `3C`
обнулён конструктором, padding `3D..3F` не инициализирован. Базовый Reset
сохраняет этот byte. [Заголовок](../../../Winx/Code/wxMinotaurAttackState.h),
[реализация](../../../Winx/Code/wxMinotaurAttackState.cpp).

Вход `1C` работает в две фазы. При флаге базы `1C` обнуляет byte `3C` до
освобождения pending, устанавливает `(key & F0200450) | 00200450`, ищет и
запускает анимацию с mode=false, interrupt=true. После playback сохраняет
handle, очищает флаг `1C`, обнуляет control word `4` и возвращает false.
В последующих вызовах потребляет completion-записи. При завершении отправляет
`271F` с payload0=word `0`, payload1=byte `1`, затем вызывает базовый вход,
который освобождает pending и вызывает виртуальный update. До завершения
обнуляет control word и возвращает false.

Выход `20` сначала читает внешний byte `60` (`owner124->130` на PC,
`owner130->13C` на PS2). При ненулевом byte задаёт скорость consumer `1.0`,
отправляет `271F` с payload0=word `0`, payload1=byte `0`, затем вызывает
базовый выход. Эта ветвь сохраняет once-флаг `1E`.

Иначе первая фаза выхода по `1E` задаёт скорость `1.0`, освобождает pending,
применяет `(key & FFC00450) | 00400450` и задаёт подполе bits `23..27` по
byte `3C`. PC нормализует byte в `0/1`; PS2 сохраняет младшие пять бит.
После mode=false playback сохраняет handle, очищает `1E`, обнуляет control
word и возвращает false. Последующие вызовы потребляют completion. При
завершении уведомляют получателя флагом false и вызывают базовый выход;
при незавершённой анимации очищают control word и возвращают false.

Update `30` захватывает внешний объект скорости **до изменения key**,
применяет `(key & F0000450) | 00000450` и при смене handle освобождает pending
и запускает новый с mode=true, interrupt=true. Только после playback читает
числитель и знаменатель из захваченного объекта, задаёт consumer скорость
`max(0.8, numerator / denominator)`. PC-поля — `1B0/214` объекта
`owner124->12C`, PS2 — `1BC/220` объекта `owner130->138`.

PC сравнивает широкое x87-частное до записи float32; PS2 сначала округляет
частное в float32. PC unordered-ветвь передаёт NaN-частное, PS2 conditional
compare выбирает `0.8` при ложном сравнении. Переносимый численный профиль
PS2 квалифицирован для проверенных обычных конечных входов и знаковых нулей;
общая эквивалентность x87 extended и host double не заявляется.

Общий speed wrapper использует [wxCharacterSpeedStateHost](../../../Winx/Analysis/Host/wxCharacterSpeedStateHost.h).
[wxMinotaurAttackStateHost](../../../Winx/Analysis/Host/wxMinotaurAttackStateHost.h)
передаёт заимствованные объекты и уведомления. Получатель — `owner24`;
при null dispatch пропускается. Верхние 24 бита payload1 неизвестны и не
объявляются нулевыми. Copy пуст, clone получает новые поля конструктора.

Проверка — `WinxMinotaurAttackStateTests`: двухфазные вход/выход, внешний
override, порядок callback, сохранение captured object, Reset и clone.
Полная интеграция consumer, owner и игровых получателей остаётся открытой.
