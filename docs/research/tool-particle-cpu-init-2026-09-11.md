# Общая CPU-инициализация ParticleSystem — 11 сентября 2026

Цикл до 19:00 МСК, блок 5. Закрыт прежний отказ PC loader на зацикленных
ParticleSystem: настоящий `Media/Menus/bg.smo` загружается целиком, начальный
пул из 539 частиц доступен через C ABI и C#. Это CPU Init, без покадровой
симуляции, сортировки/рисования частиц и старого DX device lifecycle.

## Исходники и подключение

`spParticleSystemRuntime.cpp` восстанавливает CPU-часть PC48C340, Reset48BE50,
LoopInit48D1C0 и producer48C400 в объёме начальной эмиссии. Физические записи
по 32 байта и кольцо по 12 байт представлены общими C++-векторами и индексами.
Renderer owner/caps остаётся за границей современного backend. Сериализатор
вызывает единый Init для обоих режимов. Старый `PrepareNonLoopingForAnalysis`
сохранён как инспекция количества; менять уже созданный CPU-пул через него нельзя.

Повторно использованы region sampler и единственный global PRNG
`spFunctionEval::SharedRandomForAnalysis` (PC413270/4132B0). Нормализация
направления reader/producer вынесена в общий helper. Мост уже сериализует
singleton своим mutex. Повторная загрузка расходует следующие случайные числа;
автоматического reseed или генератора для каждого ParticleSystem нет.

Новый `spAxisAngleMath.h` представляет PC4620C0. Сохранены промежуточные float
stores и порядок сложений. Host double `sin/cos` заменяет x87 instructions;
универсальная идентичность для всех float inputs не заявляется. Все наблюдаемые
биты описанных ниже случаев совпали. Граница угла ±360000° исключает огромные
аргументы с другим x87 range-reduction контрактом.

`spv_graph_particle_pool` и `SmoParticleSystemData.CpuPool` возвращают начальную
копию записей и связей. Неинициализированные оригинальным allocator поля host
обнуляет и помечает `Written=false`: это наша политика памяти. Lifetime оригинал
инициализирует нулём даже у свободной записи; он сравнивается всегда.

## Проверка по игре и приложению

Использованы четыре прежних original capture: настоящий PC2 `bg.smo` и
направленные количества 127/128/129. [Досье](native-pc-particle-loop-init-pc2-2026-09-10.md)
сохраняет их границу: whole-loader вход до 4B980D после естественного LoopInit RET,
до caps/остатка файла. Завершение загрузки и teardown игры там не утверждаются.

Шесть новых fresh objects проверены одним guest через actual factory,
reader49BCE0, Init, producer и teardown. RenderNode world POD — явно заданный
вход стенда. Варианты: нулевое направление, local coordinates, почти параллельный
basis, ровно 90°, capacity256 и non-loop. Пакет занял **1,273 s**, 79600 байт
arena, все 70 tracked allocations освобождены. Профиль 1M инструкций/8 s на вызов,
child30 s, arena256 KiB. Whole game не заявляется.

Итого **10 случаев / 17818 инициализированных original words**: записи, кольцо,
counters, границы и все 624 слова PRNG с индексом. Число assertions включает
разбор fixture и не является числом независимых доказательств. `particle-init.dat`
содержит captured inputs/states с маской неизвестных слов, не целые игровые файлы.

Сохранены особенности игры:

- `float(1.8) × 300` даёт trunc capacity539.
- Последний batch использует остаток: 128 → 0 активных; 256 → 128 активных.
- Направление сначала преобразуется world orientation даже в local mode.
  В WorldSpace скорость затем преобразуется повторно.
- Producer вращает по строкам axis-angle; общий 420350 использует столбцы.
- Порядок случайных выборок и промежуточные округления сохранены.

Настоящий `bg.smo`: 11 objects/3 Nodes, whole host graph и scene+lighting прошли
за 0,199 s на предварительном DLL. На окончательном DLL C ABI сравнил все
17248 байт записей и связи, проверил short/null/wrong-ID выходы и canaries
(1094 checks, 0,143 s). C# сравнил все 539 записей (1083 checks, 0,283 s).
Прежний managed пакет 10 fixtures/67 checks прошёл. Финальные native suites
ParticleRuntime/ParticleSerialization/FunctionEval — 3/3, 3,34 s. Сборка
FormatTests: 0 warnings/errors. GPU-проверка этим блоком не проводилась.

## Исправления и границы

Первое сравнение выявило перепутанную ветку сравнения срока жизни при вставке
в кольцо: условие восстановлено по 48CC94..48CCB2. Второе выявило отсутствие
dirty-флага при подготовке world input в новом тесте; production Node не менялся.
После исправлений прошли все десять случаев. Начальные failure logs сохранены.

Поддержан fresh CPU Init с capacity **2..1024**; original unsigned clamp65536
пока шире host-контракта. Zero/one-slot Reset образует выходящие за список
указатели; это явный отказ host. Region sampler сохраняет прежние пределы:
128 за batch, finite/±65536 параметры и 4096 попыток rejection. Loop требует
живого RenderNode; его актуальные world-поля поставляет общий Node reader.
Повторный Init, frame emission/update/death/reuse, GPU output, клонирование
ParticleSystem и PS2 Init остаются отдельными задачами. Если sampler откажет
после начала Init, уже записанные данные и расход PRNG не откатываются;
успешное состояние в этом случае не объявляется.

Manifest: `research/tools-core-particle-init-2026-09-11.json`.
Raw: `local-data/results/tools-core-cycle-20260911-1900/particle/`.
Snapshot связывает выбранные исходники и фактические DLL, не полный compiler
dependency closure. Прирост процентов EXE не начислялся; PC не доказывает PS2.
