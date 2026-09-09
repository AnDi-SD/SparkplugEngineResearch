# OBB: общая scalar inspection, 10 сентября 2026

Срез текущего цикла: поля position, size и authored quaternion класса
`spOBBBV`, которые нужны инспектору и общему загрузчику. Восстановленная
логика находится в `Sparkplug/`; мост только передаёт поле и возвращает
наблюдаемые значения. Collision queries и полная игровая интеграция сюда
не входят.

## Одна реализация полей

`spOBBBVSerializer::ReadScalarFieldForAnalysis` выполняет существующие
подтверждённые тела полей PC `00439BA0`. `ReadPayloadForAnalysis` вызывает
ту же функцию после проверки ограниченного поля. Position записывается
в локальный параметр и отдельный bounding-center mirror; size сохраняется
полностью, а радиус рассчитывается общим helper. Rotation читается как
четыре исходных float и передаётся существующему общему `ToMatrix` без
нормализации.

`observedQuaternion` — наблюдение исходного поля для инспектора. Это не
дополнительный quaternion member класса и не результат обратного
преобразования матрицы. Сам класс сохраняет Matrix3, как оригинал.

Scalar inspection принимает raw float bits, включая −0, NaN и infinity.
В существующем whole reader сохранена явно обозначенная host-проверка
finite для position, size и rotation. Она ограничивает безопасную загрузку
полного графа и не выдается за проверку игры. Точное округление matrix из
нечислового quaternion не заявляется: общий `ToMatrix` имеет контракт для
finite inputs, а инспектору возвращается именно исходный quaternion.

## Доказанная ошибка реконструкции радиуса

Оригинальная ветка size `00439C47..00439C92` сохраняет полные размеры в
`+58/+5C/+60`, держит половины, их квадраты и сумму `z² + y² + x²` на x87,
затем записывает единственный float радиуса в `+24`. Прежний C++ helper
округлял каждый квадрат и промежуточную сумму до float. Для размера
`(0.1f, 0.1f, 1.1f)` это давало `3F0DF578`, тогда как fresh original reader
записал `3F0DF579`.

Исправление использует более широкие промежуточные значения и конечное
округление радиуса. Оно также устраняет искусственный float overflow при
возведении в квадрат больших конечных размеров и преждевременный underflow
малых. Это доказанное исправление реконструкции, а не подгонка Viewer.
Универсальное побитовое равенство double и x87 для всех float не заявляется.

Тот же порядок независимо подтверждён для Box в `004394EA..00439535`.
Поэтому арифметика вынесена в один аналитический
`Analysis/PC/spBoundingVolumeSize.h`; helpers обоих serializer лишь переводят
существующие DTO. Half extents — производное представление инспектора:
указанные PC-ветки не записывают отдельный массив half extents.

## Original evidence и проверки

Pristine PC EXE SHA-256:
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Использованы реальные factory `004879C0`, serializer factory `00439A50`,
reader `00439BA0` и destructors; заменены только byte stream и CRT fixture.
Нет заглушки тела serializer, графического backend или запуска игры.

11 свежих случаев: defaults, position, exact size, rounding-sensitive size,
negative size, nonunit quaternion, raw position bits, raw quaternion bits,
subnormal size, large finite size, repeated position с неизвестным полем.
Каждый выполняется в новом guest: micro 100 000 инструкций / 2 секунды на
вызов, arena 64 КиБ, отдельный process timeout 30 секунд. Все native allocations
освобождены; reserved arena 544 bytes на случай. Original radius bits для
subnormal `(00000001,00000002,00000003)` равны `00000002`, для large finite
`(7149F2CA,71C9F2CA,72177617)` — `71BCE7CA`.

Локальные результаты находятся в
`local-data/results/tools-core-cycle-20260910-0700/obb-inspection/`:
`*-v2.json`, соответствующие logs и `pc-obb-reader-disassembly.log`.
Первый `size-rounding.json` сохранён отдельно: его state корректен, но
счётчик посещений снят после cleanup и пуст; повторный fresh `-v2` capture
фиксирует посещения до cleanup. Старый результат не заменялся.

`Sparkplug/Tests/spOBBScalarTests.cpp` содержит проверку original radius bits,
finite matrix, raw наблюдений, прежних whole-reader finite guards и границ
scalar API. CLI `--capture HEX_SECTION` позволяет сравнить текущий C++ с
original состояниями. Локальный helper
`.codex-tmp/compare_obb_scalar_cycle.py` выполнил **11 успешных сравнений** текущего
C++ с original captures; `source-comparison.json` закрепляет состояния и hash
проверочного EXE. Nonfinite quaternion matrix исключена из побитового утверждения,
исходный quaternion сравнивается полностью. Native `OBBScalar` прошёл 86 checks.

Вместе с Sphere/Box выполнены 35 ABI scalar rows и 166 managed checks публичных
decoders; точные радиусы для rounding/subnormal/huge inputs и исходные quaternion
bits проходят без арифметики в C#. Мост возвращает 32-байтовый host DTO,
`SmoOrientedBoxBoundingVolumeDecoder` только переводит его в существующий тип.
Общий FormatTests: 647 assertions. Подробности общего блока, native DLL hash
и границы реальных файлов — [Sphere/Box dossier](tool-simple-bv-shared-core-2026-09-10.md).

Файлы `local-data/` и `.codex-tmp/` — локальные артефакты и не появляются
при обычном клонировании репозитория. Новое UI-поведение и C ABI учитываются
в отчёте общего блока BV, без приписывания обвязки игре.
