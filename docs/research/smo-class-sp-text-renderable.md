# Полный разбор `spTextRenderable`

`spTextRenderable` (`0x19A745D7`) наследует `spRenderable`. Строго декодированы
20 PC-объектов из двух копий `Menus/menu.smo`; все 10 пар побайтно равны. PS2
executable подтверждает тот же serializer, хотя в доступном PS2 SMO-корпусе
объектов нет.

После inherited material/fog/alpha-sort/priority собственные поля сериализуются
в необычном порядке:

| Порядок | Field | Значение |
|---:|---:|---|
| 1 | 4 | обязательная inline relationship на `spFont` |
| 2 | 0 | `UInt16 byteCount` + строгая UTF-16LE строка |
| 3 | 1 | ARGB `UInt32` color |
| 4 | 2 | optional `Single` wrap width |
| 5 | 3 | optional `UInt32` alignment |

Наблюдаемые значения: text `"0"`, color `0xFFD59AE5`, wrap/alignment опущены.
Все nested font/atlas chains полностью проверены. Два варианта отражают только
то, владеет ли вложенный font atlas или ссылается на общий atlas.

Неизвестны alignment enum и runtime-поведение wrap, потому что оба поля
ненаблюдаемы. Equal-length text/code-page тесты и последующая материализация этих
optional fields включены в
[`smo-runtime-validation-plan.md`](smo-runtime-validation-plan.md).
