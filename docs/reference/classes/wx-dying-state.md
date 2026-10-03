# wxDyingState

`wxDyingState` — состояние персонажа с selector `11`. Восстановлены собственные
методы PC и соответствующие scalar-ветви PS2. Общие операции состояния
используют [wxCharacterState](wx-character-state.md).

| Свойство | PC | PS2 |
| --- | --- | --- |
| Class ID | `0xBCC87DA1` | `0xBCC87DA1` |
| Физическая база | `wxCharacterState` | `wxCharacterState` |
| Размер native объекта | `0x40` | `0x40` |
| Собственное поле | byte `+0x3C`, default `0` | то же |
| Entry, slot `+0x1C/+0x24` | `0x00518DF0` | `0x002CB7B0` |
| Update, slot `+0x30/+0x38` | `0x00518EA0` | `0x002CB660` |
| Permission `+0x34/+0x3C` | всегда false | всегда false |
| Permission `+0x38/+0x40` | всегда true | всегда true |

Исходное диагностическое сообщение называет byte `+0x3C`
`m_bSentDyingOverMsg`. Constructor обнуляет его; inherited copy не переносит
runtime state. Clone создаёт default объект, а inherited reset сохраняет
собственный byte назначения.

Entry обнуляет byte до вызова consumer speed `1`. Он преобразует packed key:
сначала `(key & 0xFF818000) | 0x18000`, затем выбирает поле вида анимации.
Если `owner->entity` имеет kind `24` и owner byte `+0x224` равен нулю,
применяется `(key & 0xF0FFFFFF) | 0x800000`; иначе `key &= 0xF07FFFFF`.
В PS2 соответствующие поля расположены по `owner+0x130->0x158` и
`owner+0x230`; PC использует `owner+0x124->0x14C`.

Затем entry разрешает animation handle, вызывает общий queue с
`mode=false, interrupt=true`, сохраняет новый pending и вызывает virtual
update. Старый pending предварительно не освобождается.

Update всегда вызывает внешний control reset: PC `owner+0x12C`, virtual
slot `+4`; PS2 `owner+0x138`, slot `+0xC`. Это отдельная virtual операция,
а не прямое обнуление control word. После reset ненулевой собственный byte
прекращает update. Иначе completion query с флагом consumption `true`
может очистить completion records; null pending считается complete.

При completion последовательно отправляются packets `0x27D2` с payload
`{0x13,0}` и `0x2729` с `{0,0}`. Каждый раз заново читается target `owner+0x24`;
null target пропускает только соответствующую доставку. Packet имеет вид
`{code,0,0,0,state,0,payload0,payload1}`. Только после обеих отправок собственный
byte устанавливается в `1`, затем вызывается исходная диагностическая функция.

Portable реализация находится в `Winx/Code/wxDyingState.*`. Borrowed owner,
consumer, control, менеджер анимаций, получатель сообщений и диагностика
доступны через явно выделенный `wxDyingStateHost`. Отсутствующий host — ошибка.
Объект portable не претендует на native ABI; полный scene/runtime путь внешних
объектов этим компонентом не закрыт.
