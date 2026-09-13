# spLightControllerSerializer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spLightControllerSerializer](../../../Sparkplug/Code/Sparkplug/spLightControllerSerializer.h).

Статус: восстановлены идентичность, RTTI/lifetime, раздельный PC/PS2 ABI, полная
таблица девяти полей, условия подавления значений по умолчанию и relationship-проход.
Portable-класс намеренно остаётся аналитическим planner-ом: настоящий stream codec и
не восстановленный target `spLightController` пока не подменяются догадками.

## Идентичность и размер

PS2 factory выделяет ровно `0x14` байт. PC destructor, clone и обе vtable показывают тот же полный observed extent без derived storage.

## Поля и target layout

Reader обеих платформ принимает ID `0..8`, а неизвестный ID передаёт общему
skip-пути. Jump-table, типы read/write helper-ов и offsets совпадают:

| ID | Поле | Wire kind | Target offset | Подавляется при записи |
| ---: | --- | --- | ---: | --- |
| 0 | `Enabled` | boolean byte | `+0x10` | `false` |
| 1 | `Color1` | ARGB word | `+0x2C` | platform default color |
| 2 | `Color2` | ARGB word | `+0x30` | platform default color |
| 3 | `Type` | unsigned word | `+0x68` | `0` |
| 4 | `Frequency` | float | `+0x48` | `1.0` |
| 5 | `Amplitude` | float | `+0x50` | `1.0` |
| 6 | `Offset` | float | `+0x58` | `0.0` |
| 7 | `Pitch` | float | `+0x5C` | `0.0` |
| 8 | `Light` | relationship type 7 | `+0x6C` | никогда |

При чтении `Frequency` значение также инвертируется и кладётся в `+0x4C`:
`inverse = 1.0 / frequency`. Поле `Light` всегда открывается и закрывается writer-ом,
даже если все scalar-поля равны значениям по умолчанию. Отдельный virtual pass
индексирует/разрешает указатель `+0x6C`; это не scalar payload.

## Подтверждённое различие платформ

Условия writer-а одинаковы, но исходный цвет различается:

| Платформа | Адрес константы | Default ARGB |
| --- | ---: | ---: |
| PC | `0x0073FE98` | `0xFF000000` |
| PS2 | `0x00476CB0` | `0x00000000` |

## Неизвестное

- исходные header/source paths и настоящие имена virtual-методов;
- полный native layout, constructor и поведение самого `spLightController`;
- допустимые значения `Type` и точная runtime-формула контроллера;
- смысл ARGB-различия PC/PS2 и выполняется ли дополнительная нормализация позже;
- деление на ноль/NaN как намеренный контракт либо побочный эффект исходного кода;
- ownership и момент применения relationship на `spLight`;
- rollback при повреждённом scalar или relationship field;
- контролируемый in-game mutation test.
