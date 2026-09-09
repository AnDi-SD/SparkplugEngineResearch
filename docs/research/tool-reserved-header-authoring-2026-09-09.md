# Общие заголовки полей при изменении размеров

Седьмой блок цикла, сокращённого пользователем до23:00 МСК 9 сентября.
Три собственных switch UInt8/16/32 в Importer и пять мест создания UInt32
headers заменяются вызовами существующего общего `spDataBlockSerializer`.
Сам recovered writer и native ABI не меняются. Основание — прежнее
[original-PC сравнение](tool-field-header-shared-writer-2026-09-09.md):
14 допустимых headers, две особенности оригинала и три явных отказа.

## Граница общего writer и редактора

`SmoDataBlockWriter.BuildReservedHeader` поддерживает новую явно указанную
reservation либо существующую, наблюдённую общим reader. `PatchReservedHeader`
меняет заголовок в уже перемещённой копии данных. Обычный `BuildHeader` сохраняет
прежний выбор более широкой формы при необходимости; для in-place операции
такое расширение запрещено, иначе оно повредило бы соседние байты.

Проверка вместимости остаётся в существующем native ABI. Новая C# обвязка
проверяет полученный size code, а для наблюдённого header — RawHeader и длину.
Собственной таблицы UInt8/16/32 capacity нет. Старый header сначала сравнивается
с байтами destination; неподходящие/stale metadata отклоняются до изменения
буфера. Старый полный payload здесь не перечитывается: после shrink он уже
может не помещаться, хотя сам header ещё корректно сохранён.

Patch использует два stackalloc6 буфера, не выделяет byte arrays на каждый
ancestor header. Это наша проверка безопасного редактирования вокруг общего
writer, не восстановленный метод игры. Математика FAT relocation, resource ID
и inline-size prefixes этим блоком не переносится и не объявляется original.

## Потребители

- `SmoSkinnedBranchSplitBuilder`, `SmoVisualForestInjector` и
  `SmoCollisionBranchAppender`: удалены три ручных функции переписи size word.
- Collision selection пробует общий reserved writer вместо собственной
  `CanStoreSize`; неподходящий template по-прежнему пропускается.
- `SmoLevelModelGraphReplacer.InjectInlineObject/InjectReference` и
  `SmoVisualForestInjector.PromoteReferenceToInline/ReplaceInlineLeaf/DemoteInlineLeafToReference`:
  новый header получает общий UInt32 writer. Смещения вычисляются от фактической
  длины header, а не от захардкоженного13B reference envelope.

Настоящие zero-size поля, ID31 и forced-extended low ID остаются ранее
отложенными lossless случаями. Их нельзя молча канонизировать. Forced-low-ID
проверяется до вызова native fallback: расширенный UInt8 header и компактный
UInt16 могут иметь одинаковую длину, поэтому одной проверки размера недостаточно.
Проверенный fixed/empty/default `BuildHeader` не ужесточается новым API.

## Проверка

Native binary и recovered source не менялись; новые original guests и повторные
native suites не запускались. Тело header ABI и оба shared serializer файла
совпадают с commit36f79ce;10 зависимостей original capture перепроверены по SHA.
Пять managed consumer builds прошли без предупреждений и ошибок, с одним worker.
Header regression118 проверяет reservation/overflow, перемещённый и уже укороченный
буфер, stale/forged metadata, сохранность destination при отказе и прежний fallback.

Collision append на Alfea02 прошёл; полные7106577B совпали с предыдущим результатом
SHA256 `CAE6F468D2742C3E8B9F880660F07A3132DE2445587D0740C27E89E6CC7B42D1`.
Три Model replacements/50checks, Icy13 и FormatTests647 также прошли. Все три
Model outputs и весь Icy output побайтно прежние. Изменение header не изменило
игровое содержимое, resource IDs или результат relocation в этих сценариях.
Локальные артефакты: `authoring-headers/` и `reserved-header-audit/` внутри
`local-data/results/tools-core-cycle-20260910-0700/`.
