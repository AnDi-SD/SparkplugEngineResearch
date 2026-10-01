# `wxFrogHurtState`

`wxFrogHurtState` (Class ID `022D000D`) — потомок `wxCharacterState` с selector `10`. [Общая реализация](../../../Winx/Code/wxFrogHurtState.cpp) сохраняет собственные hooks PC/PS2. Исходный размер — `3C` на обеих платформах, дополнительных полей нет. Переносимый класс не является overlay игрового ABI.

| Операция | PC | PS2 |
| --- | ---: | ---: |
| Фабрика | `403AA0` | `3F15A0` |
| Vtable | `6F9910` | `499720` |
| Вход, PC slot `1C` | `520C10` | `2F4220` |
| Обновление, PC slot `30` | `523690` | `2F4210` |
| Проверка перехода, PC slot `34` | `523770` | `2F43A0` |

Вход сначала очищает byte `1A` внешнего control-объекта, затем меняет ключ на `(key & F0008000) | 8000`. После lookup запускает анимацию с `mode=false`, `interrupt=true` без освобождения старого pending handle. Новый handle сохраняется после playback, затем вызывается виртуальное обновление `30`. Результат — `true`; once-флаги не меняются.

Обновление только обнуляет control word `4`. Переход `34` вызывает completion consumer с `(handle, true)` и потреблением записей, включая случай пустого handle; target не используется. Slot `38` возвращает `false`: общий PC hook и эквивалентный собственный PS2 leaf. Прочие hooks, включая выход и reset, принадлежат [wxCharacterState](wx-character-state.md).

Copy унаследован от `spBaseObject` и пуст. Clone получает selector `10`, начальные once-флаги и пустые привязки; деструктор не останавливает анимацию. [wxCharacterControlStateHost](../../../Winx/Analysis/Host/wxCharacterControlStateHost.h) предоставляет control по `owner+12C` на PC или `owner+138` на PS2. Смысл byte `1A` и полные внешние анимационные службы остаются за обязательным host-контрактом без успешных заглушек. [C++ проверка](../../../Winx/Tests/wxControlStateTests.cpp) охватывает порядок операций, наследуемые hooks и clone. Полная игровая сцена остаётся внешней зависимостью.
