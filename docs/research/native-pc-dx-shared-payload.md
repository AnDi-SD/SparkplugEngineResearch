# PC shared DX payload: writer, cursor и failure boundary

Checkpoint CP104, цикл до 07:00 МСК 8 сентября 2026.
Оригинальные инструкции `4C1ED0`, `4C1FA0`, `4C29C0`, factories и destructors
исполняются в ограниченном PC guest. Memory-stream slots и COM — заданные
inputs. Опыт не обращается к настоящему D3D device и не доказывает GPU rendering.

В успешном случае writer выдаёт ровно 110 байт: размеры 6/96, три INDEX16 и
24 float. Reader создаёт оба wrapper-а, копирует 102 байта через Lock/Unlock,
но оставляет cursor на 8. С C++ побайтно совпадают wire и оба буфера; отдельный
contiguous reader повторяет cursor. Writer исполняет 54 инструкции, reader с
настоящим Init — 3542. Native allocations объекта/serializer: 28/20 байт.

| Проверка | Результат |
|---|---|
| Полный writer + reader + Init | Точные байты, cursor8, исходные COM параметры и порядок |
| Отказ каждого из четырёх Write | false, один diagnostic, prefix0/4/8/14, продолжения нет |
| Null target, первый/второй Read, Tell failure | false; cursor0/0/4/8; Init не вызван |
| Init seam возвращает false | Внешний reader возвращает true; Init здесь заменён явно |
| Настоящий Init при CreateIndexBuffer failure | Stop `004C2AB6`, null read, 1757 инструкций |

Итого 10 завершённых protocol cases, 115 curated assertions, все 22 их native
allocations освобождены. Отдельный stopped case не считается успешным reader:
три allocation остались в прекращённом guest, который не возобновлялся.
Это уточняет прежнюю формулировку «reader всегда возвращает успех»: возврат
предполагает, что сам Init нормально завершился.

Профиль `file`: 1 млн инструкций / 8 секунд на вызов, 30 секунд на процесс,
arena128 КиБ и один allocation32 КиБ. Максимальное резервирование arena —
52 448 байт. Вход `spDXCombinedVB` задаёт четыре доказанных поля; его optimizer
и construction этим опытом не восстанавливаются. Заголовок целого FFPS и
общий reference dispatch в данной проверке не исполняются.

## Изменение C++

Оба reader-а проверяют 64-bit сумму размеров, физический размер потока с
учётом logical origin и предел payload32 МиБ перед выделением памяти.
Последовательный helper потребляет payload полностью. Новый
`ReadContiguousPayloadForAnalysis` оставляет cursor после размеров и принимает
только zero-origin contiguous stream. Ошибки возвращаются безопасно, прежний
target сохраняется; native null dereference не копируется на host.

Связь shared payload с полным runtime graph остаётся открытой. Native
`ReadReference` после нового payload не делает безусловный Seek к концу;
source generic reader проверяет полное потребление inline size. Нельзя
автоматически подключить shared reader и объявить whole save/load доказанным.

## Воспроизведение

```powershell
python research/probe_pc_dx_shared_payload.py valid
python research/probe_pc_dx_shared_payload.py create-failure
```

Все одиннадцать режимов перечислены в `MODES` скрипта.
Для source capture нужен `SparkplugDXSharedMeshTests.exe`
из обычной CMake сборки. CTest62/62 прошёл за57,58 с после перелинковки.
Профиль automatic workbench содержит только десять завершающихся случаев;
stopped Create failure сохранён как отдельная диагностика.

[Manifest CP104](../../research/native-cycle-checkpoint-2026-09-08-cp104.json)
содержит fingerprints и результаты каждого случая. Ни class score, ни workflow
gate за эти уточнения автоматически не повышены.
