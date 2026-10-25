# spPS2PipelineManager / spPS2MicrocodeManager

Два разных PS2-класса: `spPS2PipelineManager` с ID `0x2E7FAA26` и
`spPS2MicrocodeManager` с ID `0x0CEC4B69`. Карточка описывает установленные
границы выбора pipeline и текущего microcode. Общие C++-исходники содержат
ограниченные реализации этих операций; полноценная реконструкция классов и
оригинальные имена методов пока не заявлены. Регистрационная база указана отдельно в
[реестре классов](../classes.json); из неё не выводится физическое наследование.

## PipelineManager: выбор factory callback

| Адрес PS2 ELF | Установленная роль |
| --- | --- |
| `0x002061D0` | Factory PipelineManager |
| `0x00205F50` | Регистрация key→callback |
| `0x00205EC0` | Поиск ключа и вызов сохранённого callback |
| `0x001FC54C` | Caller регистрации пути key0 |
| `0x001FC574` | Caller регистрации пути key1 |

Для двух связанных путей используются factories pipeline `0x002076A0` и
`0x00208680`. Callback PipelineManager и сам VU bytecode — разные объекты.
Число записей менеджера не равно числу загружаемых программ.

В [spPS2PipelineManager](../../../Sparkplug/Code/SparkplugPS2/spPS2PipelineManager.h)
реализованы пустой registry при создании, factory/RTTI, регистрация,
поиск и вызов callback. Ключ сравнивается как signed32. Повторная регистрация
перезаписывает callback; один lookup создаёт новый pipeline через callback,
а не возвращает кешированный instance. Host-контекст callback — явный
переносимый интерфейс, не оригинальный PS2 calling convention. Владение
полученным pipeline остаётся у вызывающего.

При отсутствии ключа оригинальная функция доходит до `JALR s0`, хотя эта
ветка не присваивает `s0`. Поэтому `CreatePipelineForAnalysis` возвращает
`completed=false` для miss и NULL callback. Это защитная аналитическая граница,
а не утверждение, что оригинал безопасно возвращает NULL. NULL, возвращённый
действительно вызванным callback, отличается от такого непроведённого вызова.

Первый путь выбирает один из sparse slots3/6/7 таблицы `0x004B82F0`;
второй связан с отдельным контейнером. Точные диапазоны, getter, DMA CALL
и MSCAL consumers описаны в [карточке VU-образов](../shaders/ps2-vu.md).
Формирование geometry packets относится к
[spPS2MeshData](sp-ps2-mesh-data.md), а высокоуровневый submit — к
[renderer](sp-renderer.md#slot-9-передача-mesh-в-backend).

Смежный `SelectPS2Key0PipelineForAnalysis` воспроизводит вычисления native
`0x002074D0`; это не метод оригинального manager. На вход подаются захваченные
поля descriptor `+0x18`, `+0x20`, `+0x3C` и таблица с явно заданной host-длиной.
Выбирается `table[field18]`. При нулевом элементе оригинал входит в diagnostics
и продолжает вычисления, если diagnostics возвращает управление. Этот факт
сохраняется отдельным флагом; неизвестная диагностика не заменена успешным stub.

При ненулевом wrapped divisor `d = field20 + 6` вычисляются
`q = ((((1024 - (field3C != 0 ? 256 : 0)) - 260) >> 1) - 20) / d`
и `field0C = q * field20 + 2 * (3 * (q + 3) + 1)`.
Целочисленные сложения, умножение и запись результата сохраняют low32.
Host-проверка длины таблицы не является восстановленной native проверкой;
нулевой divisor помечается `capacityDefined=false` без host division-by-zero.
Полный диапазон selector и исходный размер sparse table всё ещё неизвестны.

## MicrocodeManager: текущее состояние загрузки

Factory `0x00205E10` принадлежит `spPS2MicrocodeManager`, а не таблице
factory callbacks. Его singleton связан с `gp-0x4570`, поле `+0x18`
содержит текущий upload pointer. Pipeline upload использует это состояние
отдельно от выбора pipeline по ключу. Соседство native functions не означает
принадлежности одному классу.

[spPS2MicrocodeManager](../../../Sparkplug/Code/SparkplugPS2/spPS2MicrocodeManager.h)
имеет factory/RTTI, начальный cache0 и name-only clone с новым cache0.
Native allocation sizes равны `0x24` для PipelineManager и `0x1C` для
MicrocodeManager; registry расположен по `+0x14`, current packet — по `+0x18`.
Это offsets оригинальных объектов, не `sizeof` переносимых C++-классов.
Конструкторы обновляют singleton; уничтожение очищает его без восстановления
предыдущего instance. В частности, уничтожение старого объекта очищает singleton
даже при живом более новом объекте.

`AppendPipelineCommandsForAnalysis` выделяет общий участок pipeline methods
`0x002073F0`/`0x00208490`. При смене полного32-bit packet address предыдущий
tail tag заменяется `CALL [0x50000000, address & 0x7FFFFFFF, 0, 0]`,
в текущий write cursor записывается `END [0x70000000, 0, 0, 0]`, cursor
сдвигается на16 и cache получает полный исходный address. Маска CALL и
сравнение cache различаются: адреса с разным bit31 вызывают повторный upload.

Затем, включая cache hit, tail получает CNT tag с low16
`((writeAddress - tailAddress) >> 4) - 1`, нулевыми words1/2 и
`MSCAL0 = 0x14000000` в word3. В write cursor записывается новый END;
tail становится старым cursor, cursor увеличивается на16. QWC сохраняет
native усечение через16-bit store. Существующие payload между tail и cursor
не изменяются.

Переносимый buffer заимствует words и задаёт base/write/tail как PS2 addresses.
Проверки host span, выравнивания и отсутствия address wrap выполняются до записи;
их отказ сохраняет buffer/cache. Оригинальные fault/alias paths этим не моделируются.
Helper создаёт слова команд, но не отправляет DMA, не выбирает VIF channel/VU unit
и не исполняет microcode. Lazy creation manager в key1 и сохранённая глобальная
ссылка manager в key0 остаются частью внешнего native lifecycle.

Проверки ограниченных операций находятся в
[spPS2PipelineManagersTests](../../../Sparkplug/Tests/spPS2PipelineManagersTests.cpp):
registry replacement/miss/callback, clone/defaults/singleton, sparse slots,
capacity, точные command words, cache hit/miss, QWC и ошибочные host spans.

## Открытые части

Не установлены все исходные объявления методов и полей, безопасная native
семантика lookup miss, полный allocator/diagnostics/lifetime, все indirect callers, допустимые selector values,
контракт каждой VU-программы и поведение при runtime-изменении micro-memory.
Из статических связей не выводятся корректность GPU-исполнения или закрытие
всей PS2 graphics pipeline.
