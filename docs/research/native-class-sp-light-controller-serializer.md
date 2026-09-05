# `spLightControllerSerializer`

Статус: восстановлены идентичность, RTTI/lifetime, раздельный PC/PS2 ABI, полная
таблица девяти полей, условия подавления значений по умолчанию и relationship-проход.
Portable-класс намеренно остаётся аналитическим planner-ом: настоящий stream codec и
не восстановленный target `spLightController` пока не подменяются догадками.

## Идентичность и размер

Обе сборки регистрируют `spLightControllerSerializer` с Class ID `0x70573E5E`,
прямым base `spSerializer` (`0x42429877`) и target `spLightController`
(`0x10262533`). Поле relationship допускает только `spLight` (`0x72444900`).

PS2 factory выделяет ровно `0x14` байт. PC destructor, clone и обе vtable показывают
тот же полный observed extent без derived storage. Точного исходного `.cpp`/header
path в строках двух executable не найдено.

## Поля и target layout

Reader обеих платформ принимает ID `0..8`, а неизвестный ID передаёт общему
skip-пути. Jump-table, типы read/write helper-ов и offsets совпадают:

| ID | Поле | Wire kind | Target offset | Подавляется при записи |
|---:|---|---|---:|---|
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
|---|---:|---:|
| PC | `0x0073FE98` | `0xFF000000` |
| PS2 | `0x00476CB0` | `0x00000000` |

Это реальное ABI/runtime-различие, поэтому portable planner принимает default color
явно, а evidence headers хранят две отдельные константы. Сведение их к одному
«универсальному» цвету изменило бы набор записываемых полей.

## PC evidence

Контрольный `WinxClub.exe` SHA-256:
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

- registration initializer `0x006D2F40..0x006D2F65`, register call
  `0x006D2F60`, record `0x0075EB88`;
- registration getter `0x0043C850`, target hook `0x0043C880`;
- protected factory entry `0x0043C890`, destructor `0x0043C860`, clone
  `0x0043C900`, deleting destructor `0x0043C950`;
- reader `0x0043C9B0`, relationship pass `0x0043C970`, writer
  `0x0043CC90`;
- primary/interface vtables `0x006E0198` / `0x006E018C`;
- reader jump table `0x0043CC60`, class string `0x006E0754`.

## PS2 evidence

Контрольный `SLES_532.19` SHA-256:
`198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE`.

- registration initializer `0x004831D0`, record `0x004AA030`;
- registration getter `0x001868B0`, target hook `0x00187050`;
- factory `0x001871B0`, exact allocation `0x14`, deleting destructor
  `0x00187060`, clone `0x001870D0`;
- reader `0x001868C0`, relationship pass `0x00186BF0`, writer
  `0x00186C30`;
- read/fixup/write thunks `0x00187220`, `0x00187230`, `0x00187240`;
- primary/interface vtable headers `0x0048F1D0` / `0x0048F1F4`;
- reader jump table `0x004775E0`, class string `0x0044B820`.

## Неизвестное

- исходные header/source paths и настоящие имена virtual-методов;
- полный native layout, constructor и поведение самого `spLightController`;
- допустимые значения `Type` и точная runtime-формула контроллера;
- смысл ARGB-различия PC/PS2 и выполняется ли дополнительная нормализация позже;
- деление на ноль/NaN как намеренный контракт либо побочный эффект исходного кода;
- ownership и момент применения relationship на `spLight`;
- rollback при повреждённом scalar или relationship field;
- контролируемый in-game mutation test.

Следующий класс выбирается из соседнего controller/serializer-ряда только после
проверки его регистрации и обеих реализаций; одно сходство имени не считается
достаточным основанием.
