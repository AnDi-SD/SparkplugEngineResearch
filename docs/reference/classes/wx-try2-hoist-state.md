# `wxTry2HoistState`

`wxTry2HoistState` (Class ID `6196635D`) — потомок [wxCharacterState](wx-character-state.md), selector `26`, исходный размер `3C`, дополнительных полей нет. [Переносимая реализация](../../../Winx/Code/wxTry2HoistState.cpp) не является игровым ABI.

| Операция | PC | PS2 |
| --- | ---: | ---: |
| Фабрика | `402FC0` | `3F32A0` |
| Vtable | `6F8D78` | `49A030` |
| Вход, PC slot `1C` | `5191C0` | `2D43B0` |
| Hook PC slot `2C` | `51EC80` | `2D4500` |
| Обновление, PC slot `30` | `523690` | `2D43A0` |
| Permission, PC slot `34` | `523770` | `2D4560` |
| Permission, PC slot `38` | `513480` | `2D4550` |

Вход задаёт `(key & F0000884) | 884`, выполняет lookup, запускает handle с `mode=false`, `interrupt=true`, сохраняет его и обнуляет control word `4`; возвращает `true`. Старый pending перед playback не освобождается; виртуальное обновление из входа не вызывается. Само обновление только обнуляет control word.

Hook `2C` сначала освобождает pending, затем читает unsigned word владельца PC `218` / PS2 `224`. При ненулевом значении вызывает helper PC `4FACE0` / PS2 `2B4010`, иначе PC `4FAD70` / PS2 `2B3F70`. Оба получают владельца. Публичные имена поля и helper неизвестны; их полный эффект принадлежит внешнему игровому объекту.

Permission `34` всегда выполняет consuming query, включая null handle. Permission `38` безусловно возвращает `true`. Остальные hooks, Reset и пустой Copy унаследованы. Clone получает начальные флаги и пустые привязки; деструктор не останавливает playback. Дополнительные зависимости задаёт обязательный [wxTry2HoistStateHost](../../../Winx/Analysis/Host/wxTry2HoistStateHost.h). [Проверка](../../../Winx/Tests/wxFlyingHoistStateTests.cpp) включает high-bit unsigned word и порядок release/read/helper.
