# `wxIceWormMovingState`

`wxIceWormMovingState` (Class ID `3E394575`) использует [wxCharacterState](wx-character-state.md), selector `0`, исходный размер `3C`, без дополнительных полей. [Переносимая реализация](../../../Winx/Code/wxIceWormMovingState.cpp) восстанавливает собственные entry, exit и update. Игровой ABI не равен размеру переносимого объекта.

| Операция | PC | PS2 |
| --- | --- | --- |
| Вход, PC slot `1C` | `519290` | `2ED160` |
| Выход, PC slot `20` | `5192F0` | `2ED0C0` |
| Обновление, PC slot `30` | `519350` | `2ECE70` |

Вход и выход сначала читают receiver `owner+24`. Ненулевой receiver получает packet из восьми слов: `code,0,0,0,source,0,6E,0`, где code — `27D1` при входе и `27D2` при выходе. Затем вызывается соответствующий базовый hook. Уведомление повторяется при каждом вызове, включая отсутствие pending handle; null receiver пропускает только dispatch. Базовый вход освобождает pending перед виртуальным update; выход освобождает pending и возвращает true.

Update сначала получает два borrowed объекта: PC `owner+124 -> entity+130` (motion) и `entity+12C` (optional turn); PS2 `owner+130 -> entity+13C` и `entity+138`. Motion остаётся указателем, полученным в начале метода. Если turn отсутствует, либо абсолютная разность его двух углов не превышает `0.1745f` (bits `3E32B021`), читается motion float `+4`. Значение меньше `0.2f` выбирает mode `0`; равенство и большее значение выбирают mode `10`.

Углы читаются в порядке PC `164`, `1A0` / PS2 `170`, `1AC`. PC сравнивает разность до записи в float32; PS2 выполняет float32 `SUB.S`, затем использует внешние преобразование, abs и double comparison с константой `3FC6560420000000`. Поэтому близкие к порогу входы могут выбирать разные ветви.

При выборе угловой ветви turn pointer читается повторно из владельца. PC затем читает сначала угол `1A0`, потом `164`; PS2 — сначала `170`, потом `1AC`. Разность первого логического угла и второго записывается в caller-owned float32 и передаётся внешнему helper PC `596F80` / PS2 `2930A0`. После helper отрицательное значение выбирает mode `30`, остальные значения — mode `40`. В PC unordered comparison первого порога выбирает угловую ветвь, а unordered comparison возвращённого helper значения выбирает `40`.

После выбора mode ключ маскируется `F01FFFFF`. При совпадении lookup handle с pending playback не меняется. При различии освобождается старый pending, запускается новый с `mode=true`, `interrupt=true`, затем сохраняется новый pending. Проверяется именно этот порядок; control word update не очищает.

Constructor/Clone, RTTI/factory, пустой Copy и Reset используют подтверждённую базовую модель. [wxIceWormMovingStateHost](../../../Winx/Analysis/Host/wxIceWormMovingStateHost.h) явно подключает внешние объекты, notification и **обязательный helper нормализации угла**. У helper нет придуманного fallback. Чтение полей и выбор mode находятся в общем восстановленном коде. Выбор PC либо ограниченного PS2 numeric profile принадлежит адаптеру и не добавляет поле игровому классу.

[Проверка](../../../Winx/Tests/wxIceWormMovingStateTests.cpp) проверяет entry/exit, направление чтений, повторное разрешение turn, float spill на границе helper, смену pending и lifetime. Соответствие PS2 ограничено стабильными нормальными конечными float32 и знаковыми нулями; special/denormal/overflow и EE FPU остаются открытыми. Полная переносимая нормализация, исходные имена hooks и полный игровой runtime не объявляются восстановленными.
