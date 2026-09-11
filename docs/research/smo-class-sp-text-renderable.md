# `spTextRenderable`: данные и границы чтения

Обновление11 сентября: [CPU runtime reader/layout восстановлены](tool-text-runtime-2026-09-11.md)
вместе с Font measurement; metadata и runtime используют один field reader.
Ниже сохранён прежний wire-профиль. GPU text и writer остаются открытыми.

Уточнение 10 сентября 2026: прежний вывод о UTF-16LE/Single был ошибкой
нашего decoder, скрытой единственной строкой `30 00`. Подтверждены byte-string
reader PC416DC0/441C10 и UInt32 wrap. Общий serializer предоставляет metadata
inspection; text setter/layout и полноценная runtime-загрузка этим не заявляются.
[Оригинальное evidence](tool-text-original-defaults-2026-09-10.md).

`spTextRenderable` (`0x19A745D7`) наследует `spRenderable`. Строго декодированы
20 PC-объектов из двух копий `Menus/menu.smo`; все 10 пар побайтно равны. PS2
executable подтверждает тот же serializer, хотя в доступном PS2 SMO-корпусе
объектов нет.

После inherited material/fog/alpha-sort/priority в выбранном menu наблюдается
следующий порядок. **Reader не требует этого порядка**, принимает повторные
и неизвестные поля; таблица описывает поля, а не обязательный профиль файла.

| Порядок | Field | Значение |
|---:|---:|---|
| 1 | 4 | общая relationship на `spFont`, включая NULL; в menu она inline |
| 2 | 0 | `UInt16 byteCount` + raw byte string |
| 3 | 1 | ARGB `UInt32` color |
| 4 | 2 | optional `UInt32` wrap width в пикселях |
| 5 | 3 | optional `UInt32` alignment |

Наблюдаемые значения: text `"0"`, color `0xFFD59AE5`, wrap/alignment опущены.
Все nested font/atlas chains полностью проверены. Два варианта отражают только
то, владеет ли вложенный font atlas или ссылается на общий atlas.

Factory defaults подтверждены: text/font NULL, colorFFFFFFFF, wrap/alignment0.
Отсутствующие поля допустимы для metadata inspection; layout вызывается игрой
после полей0/2/3/4 и может требовать состояния FontManager. Приложение сохраняет
raw string bytes; Latin-1 в display является обратимой проекцией байтов, не
заявлением о кодировке игры. Полный layout и безопасная запись optional fields
остаются отдельной задачей; исторический план находится в
[`smo-runtime-validation-plan.md`](smo-runtime-validation-plan.md).
