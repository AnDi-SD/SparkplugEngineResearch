# spLightSerializer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spLightSerializer](../../../Sparkplug/Code/Sparkplug/spLightSerializer.h).

Имя `spLightSerializer` присутствует в обоих executable.

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

PS2 factory вызывает constructor `spNodeSerializer` `0x00197540`, выделяет
ровно `0x14` байт и заменяет оба vptr. PC destructor `0x00471560` аналогично
восстанавливает derived vptr и передаёт разрушение в
`spNodeSerializer::~spNodeSerializer` `0x004638C0`. Собственного storage не
обнаружено.

## Методы

PC secondary write thunk `0x00471AF0` корректирует `this` на `-0x10`; PS2
secondary thunks `0x001928D0/0x001928E0` делают ту же ABI-адаптацию для
read/write. Clone создаёт пустой serializer собственной factory и использует
общий no-payload copy.

## Writer contract

Writer сначала вызывает `spNodeSerializer::write`, затем пишет отдельную
секцию света. ID и условия совпадают с доказанным light-data writer:

| ID | Значение | Условие записи |
| ---: | --- | --- |
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

Не реализованы framing блоков, stream status/error enum, byte-exact ARGB
conversion, read rollback и relationship context. Эти части нельзя достоверно
восстановить только из порядка полей.

## Открытые вопросы

1. Original header/TU path и имена secondary interface. 2. Поведение отсутствующего field 8 на всех loader/factory путях. 4. Byte-exact float-to-ARGB rounding/clamping. 5. Stream status enum, rollback и relationship error handling.
