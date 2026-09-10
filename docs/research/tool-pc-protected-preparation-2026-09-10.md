# PC: ограниченная подготовка защищённых producer-функций

Завершён адресный срез служебной подготовки двух нужных consumers: общего
renderer constructor и Occlusion shape producer. Получено естественно
расшифрованный диапазон последнего и успешный original shape result. Полный VM,
renderer constructor и default material этим результатом не закрыты.

## Ограниченные пробы и уточнение границы

Все гости создавались заново, один worker с пределом30s на дочерний процесс.
Использовались существующие профили micro100k/2s, file1M/8s и отдельно
разрешённый character4M/16s. Их общие лимиты не менялись. Original instructions
выполнялись с прежними объявленными fixture seams; оригинальные функции,
защита и вычисление преобразованных bytes не подменялись.

| Проба | Наблюдение |
|---|---|
| Constructor micro100k | Четыре protected entries, semaphore wait0, остановка внутри VM. |
| Constructor file1M | Остановка `88B0AE`; dispatch-only tail оказался историческим и не описывал текущий loop. |
| Адресный sentinel snapshot | Dispatch table `1790EE6`, первый нулевой DWORD на позиции255. Loop завершился точно на инструкции350,648. |
| Адресные comparison snapshots | Текущий record lookup: counter0…1774, stride52, field+48 XOR key-slot26. Один nonmatch cycle —1,873 инструкции. |
| Constructor character4M | Actual record611 совпал с return `13B85C5` на инструкции1,504,804. Следующая остановка: byte-loop, index933/1190. |

Последний constructor capture не содержит renderer writes: `C9C0=CCCCCCCC`,
material constructor `4A9460` не посещён, выделен только исходный объект62312B.
Нового внешнего вызова, ошибки среды или завершения конструктора не наблюдалось.
Старые capped captures сохранены; конечность отдельных loops не превращает их
в успешную полную инициализацию.

## Это XOR-дешифрование, не CRC

Полностью проверены17 VM operations `B20019…B20069` и их original native paths.
Тело сохраняет старшие24 бита DWORD по base-slot22 + index-slot23, XOR-преобразует
младший байт ключом, записывает объединённый DWORD в `890A76` и вращает key-slot25
на8 в `88D687`. Единственное изменение index — прибавление1; base и length-slot24
не меняются. Последняя запись затрагивает ещё три байта, сохраняя их значения.

Обычная итерация стоит2,668 native instructions, последняя —2,676. Actual
constructor snapshot даёт точный выход этого loop на4,683,530; дальнейшее
завершение конструктора этим расчётом не доказано. Дополнительный профиль6M
не создавался: выбран более короткий Occlusion consumer в существующем4M.

Длина подтверждена цепочкой `B1FFD1` (record pointer +8), `B1FFD9/88DD17`
(DWORD в slot24), `B1FFDD/88A7E3` (XOR с key-slot26). Для constructor длина1190;
для Occlusion record304: `D839650B XOR D8396168 = 1123`.

## Естественный Occlusion result

Выбран один объект `oclusion_wall02`, ID8 / physical index7, из pristine
`Media/Levels/Challenges/race_02.smo`. Original `470E30` вернул AL1 после
3,958,778 инструкций за9.883s; результат содержит2 faces и8 edges.
Это shape producer: поле initialized осталось0, full Init не выполнялся.

Original call `13B4F40→A0D3E0` имеет return `13B4F45`; decoded range начинается в
`13B4F48`. Естественный выход byte-loop наблюдался в `88A34B` до исполнения,
cursor `B20073`, state223, index==length1123. Он произошёл на3,932,573:
на2,810 инструкций позже прогноза3,929,763 из-за fixture-specific preparation.
Диапазон1123B сохранён после этого выхода, SHA256
`E2B9EE0751BD98FA4E63720B471AD9598860D171DAB896B3B0262CB1B56E73B7`.
Он не подставлялся вместо оригинального исполнения. Предшествующий неизменённый
FS-prefix `64` находится в `13B4F47`: сохранённые1123B не являются
самодостаточным instruction body; для CFG нужен этот отдельный prefix.

Следующая граница — разбор и подключение именно этого producer и отдельная
проверка full Init. Renderer/default material и другие protected constructors
сохраняют собственные нерешённые зависимости. Полный VM вне данного среза.

## Evidence

[Tracked manifest](../../research/tools-core-pc-protected-preparation-2026-09-10.json)
содержит SHA256 входов, scripts и неизменённых captures. Локальные материалы:

- [Constructor progression и static proof](../../local-data/results/tools-core-cycle-20260910-0730/material-preview/fallback-material/vm-progression.md).
- [Полный byte-loop proof](../../local-data/results/tools-core-cycle-20260910-0730/material-preview/fallback-material/constructor-byte-loop-static.json).
- [Actual Occlusion capture](../../local-data/results/tools-core-cycle-20260910-0730/occlusion-init-next/race02-shape-character-run1.json).
- [Естественно расшифрованный диапазон](../../local-data/results/tools-core-cycle-20260910-0730/occlusion-init-next/race02-shape-character-run1-decoded-producer.bin).
