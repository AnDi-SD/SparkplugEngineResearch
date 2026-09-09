# Общий reader анимированных текстур

Четвёртый блок цикла до19:00 МСК9 сентября. Перенесены чтение контроллера
и выбор ключа. Циклическое воспроизведение в UI и разрешение материалов
этим блоком не объявляются завершёнными.

## Реализация

`spAnimTexControllerSerializer` использует один цикл fields/count/times/references
для полной загрузки и явной metadata-инспекции. Полный reader сохраняет
канонических владельцев Texture; инспекция возвращает offsets/ID, не создавая
подставных ресурсов. Временные массивы, повторы и NULL сохраняются как записаны.
`spTextureTrack::SelectKeyIndexForAnalysis` выделен из существующего Evaluate;
Evaluate вызывает тот же метод. PC478C60 выбирает первую границу строго больше
времени; последняя граница включительна. C# больше не разбирает этот payload
и не ограничивает его255 кадрами или строго возрастающим положительным временем.

ABI2 хранит standalone исходный TextureTrack с временными ключами и пустыми
resource slots, предназначенный только для выбора индекса. Временный настоящий
контроллер после чтения уничтожается: частичный контроллер не остаётся
зарегистрированным в AnimationManager. C# SafeHandle владеет view; ID связываются
с общим каталогом документа. Это инспекция, не загруженный граф материалов.

Сохраняются host limits4096 ключей,16 МиБ payload и bounded references.
Неупорядоченные/нечисловые времена доступны для инспекции, но существующие
runtime guards отклоняют их оценку. Empty track даёт индекс-1. Нулевой count
вызывает ReadData(0), как оригинал: FileStream fixture его отклоняет, memory
inspection допускает. Это различие потоков, не изменение чтения ради теста;
см. `native-class-sp-memory-stream.md` и original PC465820.

## Проверки

Пять свежих original-PC случаев boundary/duplicate/null/negative/empty:
полные reader/ownership/runtime/writer captures совпали с source. ABI совпал
по временам/ID и33 оригинальным выборам кадра при уже вычисленной позиции
трека. Ранее запечатанный original Material→AnimController→DXTexture capture
повторно использован для inline frame и повторных ссылок, без новой эмуляции.
Полный исходный controller loop проверен source suite; ABI часы не реализует.

C++ MaterialController488, MaterialSerialization554, FullLoader213 прошли.
Сборки Viewer/Importer/LVLcreator CoreTests:0 warnings/errors. По базе выбраны
четыре небольших PC файла с разным составом контроллеров и один level regression:
Darcy_shadow_bats951, FloraX1933, bloom_crystal1753, bloomx1871, Alfea0236330
assertions. Полного corpus scan и PS2 исполнения не было.

Выборка обнаружила два ошибочных старых ожидания BloomX. Wing binding должен
совпадать по настоящим Model→Material→Texture links и состояниям, но тест требовал
ReferenceEquals с другим C# binding. Декодер/резолвер из предыдущего коммита
3a6b818 с текущими общими зависимостями даёт те же данные и отдельный экземпляр;
это сравнение исходников двух классов, не запуск полной старой сборки.
Другой тест ожидал varying alpha у sparkles0001, хотя исходный PC native BGRA
содержит alpha255 во всех пикселях. Preview побайтно равен сохранённому mip.
Исправлены проверки связей и сохранения BGRA; игровые алгоритмы не подгонялись.
Оба первых failure logs и диагностика сохранены рядом с итоговыми отчётами.

## Осталось

Texture binding ещё выбирает часть ресурсов по физическому расположению и именам;
UI использует равномерную FrameDuration. Следующий перенос должен использовать
реальные Material/AnimController references и исходное накопление/оборачивание
времени. Нельзя объявлять текущий индексатор заменой этих часов или придумывать
8fps/default material при отсутствующей игровой связи.

Evidence: `research/tools-core-animated-texture-reader-block-2026-09-09.json`.
