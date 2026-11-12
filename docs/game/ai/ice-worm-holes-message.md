# IceWormHoles: raw сообщение в профиле NoNodeCalls

PC `wxIceWormHolesAIAction` получает адрес сообщения через primary cell `04`.
Обработчик сначала читает полный code DWORD `+0`. Только code `00002739`
читает ещё DWORD `+18` и проверяет его low byte (`AL`).

| Допущенный вход | Изменение captured action |
| --- | --- |
| code `0000281C` | byte `+3C4 = 1` |
| code `0000281D` | byte `+3C4 = 0` |
| code `00002739`, payload DWORD `+18` с нулевым low byte | DWORD `+3A8 = 2` |
| Другой code DWORD | Все 968 байт сохраняются |

Сравнения code используют все 32 бита: `0001281C`, `FFFF281D` и
`80002739` относятся к другим кодам. Для `2739` payload `00000100`,
`80000000` и `FFFFFF00` имеют нулевой low byte; проверка всего DWORD на ноль
изменила бы поведение. Полный payload читается четырьмя байтами, хотя ветвь
зависит от одного байта. Остальные bytes supplied-сообщения не читаются.

Code `2739` с ненулевым low byte исключён из профиля до изменения captured
state. Оригинальная ветвь вызывает foreign Node-операции, использует FPU
и вызывает `4365E0`. Подмена этих операций пустыми вызовами отсутствует.
Профиль не квалифицирует весь обработчик или все ветви класса.

Наш plain facade хранит supplied968B отдельно от native layout и принимает
собственный `std::array<uint8_t,28>` по значению. Это value view, покрывающий
два DWORD-чтения, а не установленный native класс или полный размер сообщения.
Native pointer ABI, aliasing, reentry, fault/rollback и доставка не следуют из
такого входа. Encoded pointer words captured action остаются инертными bytes;
обработчик профиля не разыменовывает их. Регистрационная база `wxAIAction`
не используется как доказательство физического C++-наследования.

Профиль по умолчанию отказывает. Явный PC-профиль дополнительно проверяет
captured primary и границу `2739/AL` до captured-записей. `ResultForAnalysis`
сообщает наш допуск; native C++ return type неизвестен, EAX диагностический.
Аналитическая трасса включает code-чтение, условное payload-чтение и одну
либо ноль записей с исходной шириной. Admission-проверки, PCs, stack и EAX
не являются этой portable трассой.

Повтор сообщения снова выполняет выбранную запись, даже если значение уже
совпадает. Между вызовами вызывающий может явно заменить message bytes,
raw DWORD `3A8` или byte `3C4`; следующий вызов снова выбирает ветвь по сообщению.
Собственные поля не читаются для решения. Нет generic dispatcher binding,
Node provider, RTTI factory или переносимой замены защищённого constructor.
Constructor/startup, graph ownership, lifetime, PS2, runtime и gameplay
остаются за границей этого captured-value профиля.

[Идентичность класса и captured offsets](../../reference/classes/wx-ice-worm-holes-ai-action.md).
