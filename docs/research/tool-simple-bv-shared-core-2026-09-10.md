# SphereBV и BoxBV: общий код нужных инструментам операций

Цикл до 07:00 МСК 10 сентября. Восстановлены частичные реальные классы
`spSphereBV` и `spBoxBV`, их scalar/section readers, запись известных полей,
clone и используемая CollisionInfo transform virtual. C#-инспекторы обращаются
к этим readers через тонкий ABI; прежние C# чтение и вычисления удалены.

PC источник: pristine `WinxClub.exe`, SHA-256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Оригинальные header paths и названия большинства методов неизвестны;
`ForAnalysis` обозначает аналитический интерфейс к подтверждённой операции.
Современный layout классов не воспроизводит двоичный ABI игры.

## Оригинальные адреса и объекты

| Операция | SphereBV | BoxBV |
|---|---|---|
| Class ID, direct RTTI base | `390946D2`, `21CC76AF` | `7B4C0876`, `21CC76AF` |
| Registration | `6D3C40`, record `760700` | `6D4640`, record `761830` |
| Target factory | `472450 →4721D0` | `488310` |
| Allocation, vtable | `60`, `6E8F20` | `C4`, `6EC308` |
| Deleting destructor | `472250 →472020` | `487C70 →487A70` |
| Clone, inherited copy | `4724B0`, `413120` | `488370`, `413120` |
| Serializer factory | `43A190` | `439320` |
| Reader / writer | `43A2E0` / `43A4D0` | `439470` / `4396B0` |
| CollisionInfo transform slot 1C | `4723B0` | тот же `4723B0` |

Настоящие factory, readers, writers, clone и destructors исполнялись в свежих
guest. Целевые объекты и их RTTI не заменялись фиктивными объектами. Allocator
fixture предварительно заполняет каждое native allocation байтами `CC`:
наблюдаемые нули являются результатом конструктора, а не чистой arena.
Byte stream, CRT allocator/free и diagnostic output остаются явно заданными
платформенными границами стенда.

Старые PC/PS2 serializer dossiers сохраняют независимые статические свидетельства:
[Sphere](native-class-sp-sphere-bv-serializer.md),
[Box](native-class-sp-box-bv-serializer.md). Новое PS2 исполнение не требовалось:
нужные PC методы оказались доступны. Результат PC не повышает PS2 execution coverage.

## Семантика

Sphere constructor задаёт position и radius нулями; base sphere также нулевая.
Box constructor задаёт position=0, full size=(1,1,1), но **base radius остаётся 0**.
Его нельзя автоматически пересчитывать из constructor size.

Field 0 обоих readers читает position и копирует её одновременно в собственный
член `+28` и base center `+18`. Field 1 Sphere побитно копирует radius в `+34`
и base radius `+24`. Field 1 Box сохраняет full size по `+34` и вычисляет radius.
Half-extents здесь временные значения вычисления, а не подтверждённые постоянные
члены класса. Возвращаемый `DerivedSizeState` — проекция результатов для анализа.

Оригинал принимает отрицательные, NaN и бесконечные значения. Source position,
radius/size сохраняют raw float bits; base radius Box является результатом
арифметики. Нормализация, positivity и finite guards в эти readers не добавлены.
Unknown fields пропускаются; последнее повторное известное поле задаёт состояние.
При отсутствующем value reader возвращает 0; ранее прочитанная position
сохраняется, прежний radius/size остаётся прежним. Scalar reader сначала читает
во временное значение и только после успешного чтения меняет объект.

Section reader использует общий `spDataBlockSerializer` и аналитический
`SectionCursor`. Exact payload extent, конечный terminator, отсутствие trailing
bytes и ограничение размера относятся к host envelope. Это более строгая
проверка повреждённых контейнеров, а не утверждение о точной обработке всех
malformed inputs оригиналом. Реальный PC file stream может успешно вернуть
короткое ненулевое чтение; его полная ошибка/частичное потребление здесь не
имитируются.

Writer использует общую подтверждённую схему: position опускается в пределах
epsilon `0.001`, radius/size записывается всегда. Проверены default, отрицательные
и реальные конечные значения; новое execution evidence для nonfinite writer
в этом блоке не заявляется.

Clone вызывает inherited named copy `413120`; он сохраняет constructor geometry
нового объекта, а не копирует radius/size/position исходного. Это подтверждено
на ненулевой position и отрицательных параметрах обоих классов.

`4723B0` прибавляет повёрнутую local position к переданному CollisionInfo position.
Rotation, scale и authored BV state не меняются; scale не участвует в смещении.
Четвёртый native аргумент CollisionInfo в этом общем leaf не используется.
В реконструкции оба override вызывают единственный protected helper
`spBoundingVolume::ApplySimplePositionForAnalysis`; прочие BV его не используют.

Порядок умножений/сложений transform — z,y,x. X/Y остаются широкими до сложения
с входной position, Z сначала сохраняется во временный float. Эти границы
подтверждены `4723B7..472413` и сохранены в helper. Проверенный rotated-input
capture задаёт local=(1,-2,3), input=(7,8,9), rotation90° и scale=(2,3,4),
получает position=(9,9,12). Это не универсальное доказательство bit identity
double-реализации и всех x87 rounding cases.

## Доказанная ошибка прежней реконструкции Box radius

Прежний `spBoxBVSerializer::DecodeSizeForAnalysis` округлял half-components,
products и сумму как float. Для size=(0.1f,0.1f,1.1f) это давало radius bits
`3F0DF578`. Оригинальный reader `439470` даёт **`3F0DF579`**:
`box-rounding.json` сохраняет настоящий target/serializer, input, результат
и успешное освобождение всех allocations.

Участок `4394EA..439535` держит half-components/products в x87, складывает
z²+y²+x², берёт корень и только затем сохраняет radius как float. Общий helper
исправлен на широкие промежуточные значения с этим порядком; half-extents для
инспектора проецируются отдельно. Это исправление подтверждённой ошибки
реконструкции, а не подгонка класса под прежний C# результат. Для двух реальных
Box inputs старое и правильное вычисления случайно дают одинаковые bits.

`spSimpleBVSerializationTests.cpp` содержит отдельную проверку original
`3F0DF579`, исходные поля трёх реальных ресурсов, raw bits, повторные поля,
unknown fields, constructor/clone/transform и ошибки границы. В нём предусмотрен
bounded `--capture sphere|box HEX [mode]`: JSON `defaults`, `decoded`,
`readResult`, `readPosition`, `written` и, при соответствующем mode,
`clone`/`transform`. State содержит те же `sphereBits`, `positionBits`,
`valueBits`, что original capture; оригинальный numeric vtable не подделывается.

## Проверки и артефакты

Оригинал: **18 успешных micro-cases**, 100k instructions /2 s на вызов,
30 s внешний процесс, arena 64 KiB. Максимальный наблюдаемый вызов —
10 086 instructions, максимальная arena reservation — 704 bytes.
Все native-owned allocations каждого успешного случая освобождены.

Реальные scalar inputs: `SFX/vase.smo`, `SFX/blooming_flower.smo`,
`Characters/QuietusCarnivorous/Qc.smo`. Остальные случаи синтетические и не
выдаются за новые разновидности корпуса. Полный корпус не сканировался.

Локальные результаты и hashes:
`local-data/results/tools-core-cycle-20260910-0700/simple-bv/research-summary.json`.
Здесь же лежат `static-evidence.json`, `static-evidence.log` и точные версии
probe, соответствующие hashes captures. Рабочий probe:
`.codex-tmp/probe_pc_simple_bv_core.py`. Эти локальные артефакты не попадут
в обычный Git clone; интеграционный manifest закрепляет hashes локальных probes,
captures и отслеживаемых исходников. Capture CLI нового C++ доступен в репозитории.

Сохранены две ранние ошибки оформления стенда: первый sphere-empty считал
неверные per-call instruction deltas; первая failed-value попытка не смогла
JSON-encode bytes diagnostic. Их свежие исправленные варианты имеют отдельные
имена; остановленный guest не возобновлялся, limits не увеличивались.

Интеграционный этап выполнен: текущий C++ совпал со всеми **18 original cases**
по defaults, decoded state, reader result/position и, где оригинал это снимал,
writer bytes, clone и transform. `SimpleBVSerialization` — 130 checks;
`OBBScalar` — 86, `CollisionCore` — 54, `SpatialSerialization` — 97,
`FullLoader` — 213. Пять native suites прошли после подключения Sphere/Box factories
к общему ResourceGraph. Первая сборка выявила только отсутствующие host C++
friend declarations CloneManager; они добавлены без изменения игровой семантики.

Общий для Sphere/Box/OBB ABI `spv_bv_scalar_read` имеет 32-байтовый host DTO.
Он создаёт настоящий класс, вызывает общий reader и копирует getters;
для half extents используется общий подтверждённый helper. 35 scalar rows из
original captures и проверки границ дали 196 ABI/source checks. Через публичные
C# decoders и существующий inspector прошли **166 managed checks**; общий
FormatTests на Bloom projectile — 647 assertions. Положительный результат
scalar inspection не означает успешную загрузку всего исходного файла.

`vase.smo` загружается как actual graph из 77 объектов, `Qc.smo` — из 50.
`blooming_flower.smo` целиком по-прежнему не загружается: раньше Box ID36
встречается source-less TextureData ID6 `fx_glow`. Его единственная секция
содержит поля 6/0, без отдельного terminator source-wrapper; это известная
граница [CP115–116](native-pc-texture-cross-upload.md). Не добавлен фиктивный
wrapper и не изменены BV readers ради этого файла. Его Box scalar независимо
совпал с оригиналом и читается инспектором. Диагностика с offsets/hashes:
`simple-bv/integration/blooming-flower-diagnosis.json` в каталоге цикла.

Текущий native DLL SHA-256:
`D5FDE86B13D86B9CBCCE11F6EEE74022BE180DDDB590D13C4463639DC7CE00B5`.
Проверки и исходные состояния: `simple-bv/integration/abi-1.json`,
`managed-1.json`, `source-*-1.json`, `native-2.log` в каталоге цикла.

## Граница

Это используемые tools slices реальных классов, не весь collision runtime.
Нет contact/intersection queries, corners, broadphase/scene registration,
полного game startup или PS2 whole-file execution. Sphere/Box подключены к общему
загрузчику, три C# scalar-decoder стали тонкими адаптерами. Это завершённый срез
ядра, а не завершение Viewer, collision runtime или всего переноса tools.
