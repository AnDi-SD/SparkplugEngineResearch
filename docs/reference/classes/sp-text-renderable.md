# spTextRenderable

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spTextRenderable](../../../Sparkplug/Code/Sparkplug/spTextRenderable.h).

## Исторический wire-срез10 сентября

После inherited material/fog/alpha-sort/priority в выбранном menu наблюдается
следующий порядок. **Reader не требует этого порядка**, принимает повторные
и неизвестные поля; таблица описывает поля, а не обязательный профиль файла.

| Порядок | Field | Значение |
| ---: | ---: | --- |
| 1 | 4 | общая relationship на `spFont`, включая NULL; в menu она inline |
| 2 | 0 | `UInt16 byteCount` + raw byte string |
| 3 | 1 | ARGB `UInt32` color |
| 4 | 2 | optional `UInt32` wrap width в пикселях |
| 5 | 3 | optional `UInt32` alignment |

Factory defaults подтверждены: text/font NULL, colorFFFFFFFF, wrap/alignment0.
Отсутствующие поля допустимы для metadata inspection; layout вызывается игрой
после полей0/2/3/4 и может требовать состояния FontManager. Приложение сохраняет
raw string bytes; Latin-1 в display является обратимой проекцией байтов, не
заявлением о кодировке игры. Полный layout и безопасная запись optional fields
остаются отдельной задачей; исторический план находится в
`smo-runtime-validation-plan.md`.
