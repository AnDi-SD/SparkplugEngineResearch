# wxReadingState

`wxReadingState` физически наследует [`wxCharacterState`](wx-character-state.md).
Class ID — `0x6C548A6D`, selector — `27`; объект PC и PS2 имеет размер `0x3C`
и не добавляет поля к базе. Переносимый объект не имитирует нативный ABI.

Вход состоит из двух фаз. Пока базовый flag `+0x1C` ненулевой, состояние
отправляет filtered уведомление `0x27BA`, filter `0xE`, payload `{0, 0}`. Затем
переписывает caller-owned key как `(key & 0xF027FF80) | 0x200000`, ищет и
запускает анимацию с `mode=false`, `interrupt=true`, записывает pending,
обнуляет flag `+0x1C` и возвращает `false`. Старый pending перед первым запуском
не освобождается. Позднейший вход выполняет consuming completion query: отказ
сохраняет `false`, завершение вызывает базовый вход, включая освобождение pending
и виртуальный update.

Update переписывает ключ как `(key & 0xF0078900) | 0x900` и ищет handle. Если
он совпадает с pending, состояние больше ничего не делает. Иначе запускает его
с `mode=true`, `interrupt=true` и записывает pending после consumer callback.
Release старого handle и reset completion records здесь отсутствуют.

Выход также имеет две фазы, используя базовый flag `+0x1E`. Первая отправляет
главному получателю `+0x2B4` уведомление `0x2737` с bool `true` и нулевым вторым
словом payload. После уведомления состояние освобождает pending, изменяет key
как `(key & 0xF0478900) | 0x400900`, ищет и запускает анимацию с `mode=false`,
записывает pending, обнуляет flag `+0x1E` и возвращает `false`. Следующий выход
ждёт consuming completion query. После завершения он отправляет `0x2737` с bool
`false` и вызывает базовый выход. Нулевой получатель пропускает уведомление.
В нативных bool payload определён только младший байт; старшие три — выравнивание.

Общие helper определяют fade selector `0` либо `2` для запуска по предикату
владельца. Release останавливает анимацию либо использует fade `0.4` по тому же
предикату. Completion query вызывается также для нулевого handle.

Permission игнорирует переданный код, owner и pending. Оно читает глобальное
слово manager `+0x1B0` и возвращает `false` только для `0x47`. Значение этого
слова на уровне исходной игровой модели пока не названо. Остальные slots,
включая reset и пустой Copy, наследуются от базы. Clone создаёт новый объект с
начальными flags и пустым pending.

Собственные методы восстановлены при обязательных внешних consumer, manager,
owner и notification службах. Полный игровой граф этим компонентом не закрыт.
Исходники: [wxReadingState.h](../../../Winx/Code/wxReadingState.h),
[wxReadingState.cpp](../../../Winx/Code/wxReadingState.cpp); наш
[host](../../../Winx/Analysis/Host/wxReadingStateHost.h).
ABI: [PC](../../../Winx/Analysis/PC/wxReadingStateAbi.h),
[PS2](../../../Winx/Analysis/PS2/wxReadingStateAbi.h).
Проверка: [wxReadingStateTests.cpp](../../../Winx/Tests/wxReadingStateTests.cpp).
