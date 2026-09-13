# spAnimTexControllerSerializer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spAnimTexControllerSerializer](../../../Sparkplug/Code/Sparkplug/spAnimTexControllerSerializer.h).

Статус: восстановлены identity, RTTI/lifetime, раздельный PC/PS2 ABI, внешний
field, внутренний track payload и relationship pass. Portable-класс моделирует
только подтверждённый порядок и размеры фиксированных сегментов; переменный размер
inline relationship намеренно не выдаётся за четыре байта.

## Wire grammar

Serializer всегда создаёт единственный внешний field 0
`esfAnimTexControllerBase` типа 7. Внутри записываются строго три последовательных
сегмента:

1. один `UInt32 frameCount`;
2. непрерывный массив `float time[frameCount]` размером `4 * frameCount`;
3. `frameCount` relationship-записей на texture.

Relationship не имеет постоянного wire-размера: ссылка может быть внешней либо
содержать inline-объект. Reader использует один `frameCount` для обеих параллельных
коллекций. Native target layout на обеих платформах:

| Offset | Содержимое |
| ---: | --- |
| `+0x3C` | указатель на массив времён |
| `+0x40` | указатель на массив texture relationships |
| `+0x44` | общий frame count |

Передаваемый relationship Class ID — `0x2F281E13`, то есть базовый `spTexture`.
Конкретный inline-объект может быть `spTextureData` или платформенным наследником; это не сужает объявленный базовый тип ссылки. Отдельный virtual pass
обходит все элементы `+0x40` до `+0x44` и индексирует/разрешает каждую связь.
