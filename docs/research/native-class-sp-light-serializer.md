# `spLightSerializer`: сериализация базового света

Статус: отдельный класс, обе цепочки наследования, lifetime, target ID и
writer/default contract подтверждены на PC и PS2. Потоковый codec пока оставлен
evidence-only.

Имя `spLightSerializer` присутствует в обоих executable. Оригинальный путь к
translation unit или header пока не найден, поэтому путь
`Code/Sparkplug/spLightSerializer.*` в реконструкции является осторожной
интерполяцией соседних классов, а не найденной строкой исходника.

## Идентичность и наследование

В отличие от `spLightDataSerializer`, здесь обе наблюдаемые цепочки совпадают:

- C++ construction/destruction проходит через `spNodeSerializer`;
- engine RTTI регистрирует base ID `0x4545848A`, то есть также
  `spNodeSerializer`;
- собственный Class ID — `0x06165309`;
- secondary target slot возвращает `spLight::ClassID` (`0x72444900`), а не
  `spLightData`.

Это самостоятельный serializer, а не alias и не потомок
`spLightDataSerializer`, несмотря на одинаковую грамматику световых полей.

| Свойство | PC | PS2 |
|---|---:|---:|
| Registration | `0x007606A0` | `0x004AA6F0` |
| Initializer | `0x006D3C10` | `0x00483650` |
| Factory | `0x00471590` (protected entry) | `0x00192860` |
| Allocation | observed `0x14` | exact `0x14` |
| Primary vtable | `0x006E8EDC` | header `0x0048F830` |
| Secondary vtable | `0x006E8ED0` | header `0x0048F854` |

PS2 factory вызывает constructor `spNodeSerializer` `0x00197540`, выделяет
ровно `0x14` байт и заменяет оба vptr. PC destructor `0x00471560` аналогично
восстанавливает derived vptr и передаёт разрушение в
`spNodeSerializer::~spNodeSerializer` `0x004638C0`. Собственного storage не
обнаружено.

## Методы

| Роль | PC | PS2 |
|---|---:|---:|
| registration getter | `0x00471550` | `0x00191A90` |
| write | `0x00471B00` | `0x00191FC0` |
| read | `0x00471670` | `0x00191AA0` |
| target class ID | `0x00471580` | `0x00192700` |
| deleting destructor | `0x00471650` | `0x00192710` |
| blank clone | `0x00471600` | `0x00192780` |

PC secondary write thunk `0x00471AF0` корректирует `this` на `-0x10`; PS2
secondary thunks `0x001928D0/0x001928E0` делают ту же ABI-адаптацию для
read/write. Clone создаёт пустой serializer собственной factory и использует
общий no-payload copy.

## Writer contract

Writer сначала вызывает `spNodeSerializer::write`, затем пишет отдельную
секцию света. ID и условия совпадают с доказанным light-data writer:

| ID | Значение | Условие записи |
|---:|---|---|
| 0 | type | не directional (`0`) |
| 1 | project shadow volume | `true` |
| 2 | ARGB color | не белый с допуском `0.001` для RGBA |
| 3 | attenuation | `true` |
| 4 | intensity | не `1.0` |
| 5 | range | не `200.0` |
| 6 | hotspot angle | не `0.0` |
| 7 | falloff angle | не `0.0` |
| 8 | enabled | `true` |

Числовые поля 4–7 сравниваются точно; цвет — покомпонентно с константой
`0x3A83126F`. Как и у data serializer, native writer подавляет `false` для
field 8, хотя текущий runtime constructor создаёт включённый свет.

## Portable срез

Восстановлены RTTI/factory, blank clone, target ID и
`BuildKnownWritePlanForAnalysis(const spLight&)`. План намеренно разделяет
inherited node section и собственную light section: одинаковые field IDs в
разных блоках не смешиваются. Для теста используется concrete `spLightData`,
но метод принимает базовый `spLight`, как доказывает target ID.

Не реализованы framing блоков, stream status/error enum, byte-exact ARGB
conversion, read rollback и relationship context. Эти части нельзя достоверно
восстановить только из порядка полей.

## Проверка

Последовательная сборка `ninja -j1` и оба CTest-набора проходят. Тесты отдельно
проверяют direct `spNodeSerializer` RTTI base, exact PS2/observed PC `0x14`,
target `spLight`, default-план `{node: IsAnimated, light: Enabled}`, полный
порядок изменённых полей и concrete blank clone.

## Открытые вопросы

1. Original header/TU path и имена secondary interface.
2. Прямое доказательство PC allocation size вместо полного observed extent.
3. Поведение отсутствующего field 8 на всех loader/factory путях.
4. Byte-exact float-to-ARGB rounding/clamping.
5. Stream status enum, rollback и relationship error handling.


## CP88 update, 2026-09-07

The earlier portable-plan-only and protected-factory gaps are superseded by
[complete PC codec evidence](native-pc-light-serialization.md): actual factories,
19 exact184 native assertions,4 additional native boundary cases,source
read/write and fresh-object round trips. Remaining limits are stated there.
