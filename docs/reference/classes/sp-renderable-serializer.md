# spRenderableSerializer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spRenderableSerializer](../../../Sparkplug/Code/Sparkplug/spRenderableSerializer.h).

Имя класса и подробные diagnostics присутствуют в обоих executable, но строка
оригинального source path не найдена. Поэтому
`Code/Sparkplug/spRenderableSerializer.*` — явно inferred расположение.

## Идентичность и ABI

`spRenderableSerializer` имеет Class ID `0x4D694D82`, напрямую наследует и
регистрируется от `spSerializer` (`0x42429877`), а target slot возвращает
`spRenderable::ClassID` (`0x4FDA4542`).

PS2 factory выделяет `0x14` и constructor `0x00197C70` вызывает напрямую
`spSerializer` constructor. PC destructor `0x0047F620` возвращается к
`spSerializer` destructor; собственного storage нет.

## Поля и порядок записи

| ID | Поле | Native write condition |
| ---: | --- | --- |
| 0 | material relationship | material не null |
| 1 | fog relationship | fog не null |
| 2 | alpha-sort enable | всегда |
| 3 | alpha-sort priority | всегда |

Writer сохраняет строгий порядок `0, 1, 2, 3`, пропуская только отсутствующие
relationships. Значит, default-объект всё равно записывает поля 2 и 3 со
значениями constructor state (`true`, `0`); это не default suppression.

## Portable срез

Stream framing и relationship ID table теперь общие реализованные механизмы.
Полные rollback/error/lossless/native lifetime и исходные status names остаются
открытыми; наличие adapter не означает их100%.

## Открытые вопросы

1. Original header/TU path и имена secondary interface.
2. Прямой PC allocation size вместо observed extent.
3. Точные stream/status types и block framing.
4. Relationship ownership/fixup/rollback contract.
5. Original enum spelling для scalar fields 2 и 3.
