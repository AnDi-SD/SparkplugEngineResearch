# Общий geometry helper: исходный caller и явная зависимость sort

Перенесён подтверждённый position-only UInt16 путь оригинального PC helper:
caller `460D90 → 460C40`, comparator `4607F0 → 13D1850`, compactor
`4609A0 → 450F50`. Общий код находится в
[GeometryHelper4604F0](../../Sparkplug/Analysis/PC/GeometryHelper4604F0.h).
Это частичный восстановленный объект с аналитическим именем: исходное имя
неизвестно. Его constructor `4604F0`, destructor `460500` и единственный slot
vtable `6E76FC → 460AE0` установлены непосредственно по PC. Это отдельный
owner, а не `spVertexBuffer` с vtable `6E75BC`. Original Init вызывает ctor
на stack object в `471120`, затем тот же объект передаёт в weld `47113F`.

**Полный Occlusion Init и tools hookup остаются открытыми.** Sort callback
обязателен; implementation по умолчанию отсутствует. DLL не добавлена в
приложения, stable fixture и современный `std::sort` не подставляются.
Существующие [shape guards](tool-occlusion-shape-shared-core-2026-09-10.md) и
[re-init/single-triangle границы](tool-occlusion-reinit-and-batch-2026-09-10.md)
не изменены. Центральная native проверка прошла: GeometryHelper 230/230,
OcclusionTopology 149/149, FullLoader 213/213, CTest 3/3 за 7.28 s.

## Что установлено по оригиналу

`460CA9` вызывает IAT `006D9360`, импорт именно `MSVCR71.dll!qsort`.
Ограниченная проверка имён файлов в `pc-pristine`, `Winx Club`,
`WinxClubWithDebugMenu` и `WC Tweaks` не нашла поставочную DLL. Для независимой
пробы использована установленная `C:/Windows/SysWOW64/msvcr71.dll`, версия
**7.10.7031.4**, 344064 bytes, SHA256
`DCA0E5FAF6C94B6ADFF4D90D40795D5A91BA3A3059EA408E992A0F039A494D46`.
Это доказательство поведения конкретной зависимости, а не исторической версии
из дистрибутива игры и не универсального порядка CRT ties.

Bounded CFG export `7C382650..7C3828D2` содержит 231 instruction; вызовы только
supplied comparator и `7C3825E0..7C38264C` shortsort. Для count ≤ 8 используется
max-to-high selection shortsort, который **не является стабильным**. Остальной
путь использует median-of-three partition и локальные stacks. Эти оригинальные
bytes исполнялись без замены алгоритма; portable MSVCR71 implementation в общий
код пока не добавлена.

Один child выполнил четыре случая на новых CPU IB/VB, вызывая настоящий
`460D90`, импорт с явно назначенным адресом настоящего export и оригинальный
comparator. В mapped page размером 4096 bytes разрешены только проверенные
instructions двух функций DLL. Сохранились общий instruction guard и limits:
100000 instructions / 2 s на вызов, 30 s child, один worker, arena 64 KiB.
Все четыре calls вернулись; все tracked allocations освобождены. Измеренное
время всего пакета — 1.9840345 s, использованный bump extent — 1920 bytes.
Это не измерение суммарной памяти процесса. Scene, renderer и Init в пакете
не создавались; stopped guest не продолжался.

| Вход | Instructions | IDs после qsort, до fold | Итоговый IB | VB count |
|---|---:|---|---|---:|
| unique4 | 2903 | `[2,1,3,0]` | `[0,1,2,0,2,3]` | 4 |
| duplicate5 | 3519 | `[2,1,3,4,0]` | `[3,0,1,3,1,2]` | 4 |
| duplicate9 | 5351 | `[2,7,1,6,3,8,5,0,4]` | `[3,0,1,3,1,2]` | 4 |
| all_equal12 | 6900 | `[0,1,2,3,4,5,6,7,8,9,10,11]` | `[0,0,0,0,0,0]` | 1 |

Для duplicate5 retained representative — ID 4. Старый явно объявленный stable
fixture давал ID 0 и другой raw IB/VB. Следовательно, выбор sort dependency
наблюдаем через raw geometry, даже когда позиции треугольников совпадают.
Старые результаты [CP15](native-pc-occlusion-runtime.md) остаются историческими
проверками со своей fixture boundary.

## Точный перенесённый срез

`460C90` заполняет последовательные UInt16 IDs. `460CA9` передаёт count,
element width 2 и byte comparator. `460CC0` строит inverse map **до** замены
equal IDs. `460CF0..460D19` оставляет первого представителя каждого equal run;
сравнение — 12 unsigned raw position bytes, без epsilon или нормализации
signed zero. `460D25` пропускает remap и compactor, если duplicates нет;
сам факт неиспользуемых vertices не включает compaction.

При duplicates `460D40` сначала меняет IB. Compactor отмечает referenced IDs,
назначает новые IDs в порядке исходного VB (`450FF5`), снова меняет IB и лишь
затем заменяет VB. Partial mutations не откатываются. Современное владение CPU
данными выполняет существующий `spVertexBuffer::InitializeFromDataForAnalysis`;
порядок записей следует оригинальному `4510E6 → 4600E0`.

Необычная allocation сохранена явно: `451040` читает WORD `VB+10` (байтовый
stride), `451044` умножает на retained count, `451047` дополнительно сдвигает
на 2. Для duplicate5 входной VB allocation 60 bytes соответствует пяти
position-only records; map allocation 20 bytes, replacement allocation **192**.
Output count 4 и исходный layout producer `45FEA0` дают `12 × 4 × 4 = 192`.
Это связь actual allocation/output и исходных инструкций, не отдельный observer
регистров в `451040`. Initializer `460105..460115` записывает логический
vertexSize `12 × 4 = 48`. Общий helper создаёт временное выделение 192 и
наблюдение `compactionAllocationBytes`; наружу VB отдаёт только 48 logical bytes.
Unused heap tail не выдаётся за исходно инициализированные данные.

Host guard допускает только initialized position-only VB, UInt16 IB,
не более 65535 vertices и 65535 indices, валидные references внутри VB и
непустые buffers. Zero-count original lifetime в этом пакете не доказан и
явно unsupported. Callback получает синхронный comparator с явным context
вместо исходного global `75FF9C`; должен сохранять входные buffers. Отсутствие,
отказ, invalid permutation или нарушение comparator order дают host refusal
до geometry mutation. Порядок равных ключей callback-проверка не навязывает.
Оригинальные методы возвращают void; bool/error общего API — host completion,
не придуманный игровой результат.

## Проверка и оставшаяся зависимость

[Адресные tests](../../Sparkplug/Tests/spGeometryHelperTests.cpp) используют
сохранённые четыре actual sorted permutations и конечные IB/VB records,
отдельный исторический fixture с другим representative, raw byte/signed-zero
comparator и host guards. В tests нет второй реализации qsort. Source frozen;
root выполнил одну центральную сборку и выбранные suites. Результат:
GeometryHelper **230/230**, OcclusionTopology **149/149**, FullLoader **213/213**,
CTest **3/3, 7.28 s**. Logs `shared-geometry-native-build.log` и
`shared-geometry-ctest.log`, immutable DLL
`shared-geometry-SparkplugViewerNative.dll` сохранены рядом с original probe.
SHA256 DLL: `928F59FBB347075791B3385007897809A0B9FB6A3EF4E917C6E1DC32F22D8B93`.

Следующее решение — выбрать явно versioned восстановленную sort dependency
по исследованному MSVCR71 CFG либо согласовать общую замену с понятной
tie-order границей. До решения production caller не подключён к tools, full
Occlusion Init/serializer не объявлены готовыми. Для точного race_02 object 8
keys различны, но это не разрешение скрытно пропустить общий weld.

Локальные оригиналы, DLL, captures и script находятся в
`local-data/results/tools-core-cycle-20260910-0730/occlusion-optimizer/` и не
входят в Git. Fingerprints и текущий статус — в
[manifest](../../research/tools-core-occlusion-geometry-helper-2026-09-10.json).
