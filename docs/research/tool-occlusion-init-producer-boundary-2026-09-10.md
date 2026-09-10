# OcclusionVolume: граница original Init и producer

Открытие whole resource graph `Levels/Challenges/race_02.smo` сейчас
останавливается при проверке FAT: class`43D24430`, resource ID8. Это отсутствие
регистрации полного runtime-контракта, а не отказ уже подключённого Init.
Общий [spOcclusionVolume](../../Sparkplug/Code/Sparkplug/spOcclusionVolume.h)
содержит шесть проверенных topology helpers, включая
[исходящие связи4705A0](tool-occlusion-connectivity-shared-core-2026-09-09.md).
Serializer и полный Init ещё не реализованы; регистрация partial class не
устраняет этот пробел.

## Один реальный вход

Выбранный indexed файл имеет3329842 bytes и7513 objects, SHA256
`AD39C13B896718755CC98445D943FA191640636F940930086CB802629E268436`.
Resource ID8 соответствует physical index7, `oclusion_wall02`,138 bytes,
DBfile5308. Проверены только поля этого объекта:

| Payload | File offset | Bytes | Header / values |
|---|---:|---:|---|
| IndexBuffer | 190026 | 24 | `[2,2,0]`; indices`[2,1,0,1,2,3]` |
| VertexBuffer | 190055 | 60 | `[0,4,0]`; четыре разных position records |

Все четыре позиции побайтно различны. Поэтому actual460C40 после сортировки
не находит duplicates и обходит remap/compactor4609A0. Порядок CRT ties не
блокирует именно этот вход. Функция4604F0 — только ctor helper с установкой
vtable6E76FC, не дополнительный алгоритм подготовки геометрии.

## Новые original-PC наблюдения

Три fresh dispatch-only пробы останавливаются **до первой инструкции тела**:

| Entry / dispatch slot | Resolved body | Observed instructions |
|---|---|---:|
| 470E30 / 13B2B08 | 13D0460 | 1647 |
| 4609A0 / 13B17FC | 450F50 | 1634 |
| 470B20 / 13B1BD0 | 13B4F40 | 1612 |

В этих трёх пробах seams0; память объекта — объявленное host storage, не
созданный игровой runtime. Resolved-body instructions executed0. Raw prefixes
сохранены отдельно от дизассемблирования проверенного entry block.

Читаемый driver13D0460 резервирует faces`+F8` по IB.triangleCount через46F9A0
(return13D04A3), затем `3*triangleCount` edge slots`+D8` через46EC00
(return13D04C7). Последовательность13D04C9/13D04CE вызывает470B20;
13D04DC/13D04DE проверяет AL и переходит к false exit13D0592.

Отдельная новая проба загрузила неизменённые поля выбранного объекта actual
CPU readers45FB80/460300: оба вернули AL1, cursors24/60. Actual460240 создал
world VB copy. Привязка этих трёх owned buffers к actual factory object была
явным host входом для driver, **не выполнением Init**. Вызов470E30 прошёл оба
reserve и достиг470B20, затем исчерпал100000 instructions при IP88C226,
за0.4166048s. Faces/edges остались пустыми, planar/initialized —0.
Гость после cap отброшен без продолжения и новых вызовов; освобождение всех
игровых allocations этого отказавшего вызова не заявляется.

Изолированное раскрытие470B20 установило: первая инструкция13B4F40 —
`call A0D3E0`. Это VM boundary. Следующие bytes не интерпретируются как x86
алгоритм; успешного first-face результата пока нет. Старый capped full470FE0
не повторялся. Размер4 vertices/2 triangles сам по себе не доказывает конечную
стоимость VM и не обосновывает повышение instruction cap.

## Representatives одинаковых вершин

Root отдельно исполнил actual compactor4609A0→450F50 на двух объявленных
already-remapped inputs: representative0 либо4 для одинаковых вершин одного
five-vertex буфера. Каждый вызов завершился за2089 instructions; общий
измеренный interval двух случаев0.9297189s, все tracked allocations освобождены.

Выходные IB различаются: `[0,1,2,0,2,3]` и `[3,0,1,3,1,2]`. Порядок четырёх
unique vertices в VB тоже различается. Координаты индексированных треугольников
равны. Следовательно, общая byte invariance после compaction **опровергнута**;
геометрическое равенство не доказывает равенство наблюдаемого raw state.
Original CRT qsort в этих случаях не исполнялся, его реальный выбор
representative не установлен. Это не повод исследовать CRT для выбранного
race_02 без duplicates.

## Независимый PS2 static и следующий шаг

Existing1CBD40 создаёт три directed edges на triangle в исходном порядке.
Новый static extract1CAD50..1CAF90 уточняет face producer: plane через110610;
поиск первой existing32-byte face, у которой все четыре plane scalars равны
componentwise с epsilon0.001; совпадение возвращает прежнюю face и сохраняет
её position pointers. Иначе append plane и трёх входных position pointers.
Existing1CC544..1CC640 добавляет planar reverse edges в обратном порядке,
запрашивает faces с обратным порядком position pointers и строит outgoing
для первой добавленной reverse edge. Это PS2 evidence, не PC Init credit.

Следующий конкретный producer — PC470B20/13B4F40: нужен подтверждённый
first-face/three-edge результат либо читаемый semantic body с учётом partial
mutations и AL. Общая VM dependencyA0D3E0 исследуется отдельно. После этого
потребуются композиция уже готовых helpers, planar reverse side и исходные
Init effects из [PC runtime dossier](native-pc-occlusion-runtime.md): buffers,
bounds/sphere, dirty/initialized и reader44F400 с освобождением временных IB/VB.
Пропуск Init или собственных полей не является whole-loader решением.

Все новые гости: micro100k/2s/call,30s/child, один worker,64KiB arena.
Peak host memory не измерялся; общий предел1GiB не повышался. Production code,
сборки и GPU не менялись/не запускались. Buffer metadata bridge — отдельный
проверяемый срез, не успешный Occlusion runtime.

Локальные captures/scripts:
`local-data/results/tools-core-cycle-20260910-0730/occlusion-init-next/`.
Fingerprints и статусы:
[manifest](../../research/tools-core-occlusion-init-producer-boundary-2026-09-10.json).
