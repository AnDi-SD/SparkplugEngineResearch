# PC ParticleSystem: генератор случайных чисел и области эмиссии

CP121, 8 сентября 2026. Продолжение
[параметров и whole pickup scene](native-pc-particle-parameters.md).
PC EXE SHA-256:
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

Восстановлены dispatcher `48C100`, все7 sampler functions и генератор
`413270/4132B0`. Native comparison исполняет их целиком **без seams**:
ни случайные числа, ни геометрия, ни математические функции не подменяются.
Фабрика/reader здесь не нужны: входом служит явно объявленное состояние
`regionType/+70`, `regionPointer/+74`; их связь с файлом доказана в CP120.

## Случайные числа и геометрия

PRNG — MT19937 с624 UInt32, recurrence multiplier1812433253, twist offset397,
mask9908B0DF, tempering masks9D2C5680/EFC60000. Native initial index625
автоматически вызывает Seed5489; явный seed заканчивается index624.
После первого Next оригинал выдал3499211612, выполнив17274 инструкции и
настоящий seed helper. Source использует явный объект PRNG, без скрытой
зависимости от process-global state.

| Tag | Native sampler | Подтверждённое поведение |
|---:|---|---|
| 1 point | `49DE60` | Copy position, RNG не потребляется |
| 2 box | `49DFF0` | Три centered uniforms; RNG order Y, X, Z |
| 3 sphere | `49E090` | Rejection внутри единичного шара, затем radius/translation |
| 4 plane | `49DE80` | Положительный X/Z rectangle; сохранённый normal не используется |
| 5 disk | `49E170` | Rejection в X/Z disk, Y остаётся на плоскости |
| 6 cylinder | `49DF00` | X/Z disk плюс centered height по Y |
| 7 cone | `49E220` | Сначала physical Y, затем rejection по вычисленному radius |

Uniform коэффициенты — точные powers of two: float `2F800000` =2^-32,
`30000000` =2^-31. Сохранён оригинальный порядок float spills: например,
cylinder X добавляет origin до store, Z — после промежуточного store;
box сначала генерирует Y, затем X. Cone вычисляет
`radius1 + (radius2-radius1)*physicalY` **без деления на height** и использует
maximum radius для rejection square. Это зафиксированное поведение оригинала,
не исправленная геометрическая модель.

В native writer diagnostics `6EDC14/6EDBD8` явно стоят cylinder Height/Radius;
`6EDAAC/6EDA74/6EDA3C` — cone Height/Radius1/Radius2. Старый Viewer ошибочно
считал первый scalar радиусом. Исправлены constructor arguments декодера,
Inspector/Corpus field descriptions и regression fixtures с разными height/radii.
Числовой порядок старого wire parser не менялся; исправлено значение его меток.

## Проверка и ограничения

[Comparer](../../research/compare_pc_particle_sampling.py): **252/252** —
7 regions ×4 variants ×3 seeds ×3 counts(0/1/128). Variants: обычный,
перенесённый с недвоичными scalar inputs, zero и negative. Seeds0/5489/FFFFFFFF.
Совпали **130032 position bytes и630000 bytes полного PRNG state/index**,
включая повторные twist boundaries. Native total3280591 instructions,
maximum92638 на sampler call; до1622 random draws на один batch.
Проверены sentinel bytes до output и после запрошенного extent.

Staging arena —1872 bytes, лимит64KiB; объявлен file profile1M instructions/8s,
30s child cap. Семь независимых groups допускают4 workers. Компактный стенд
не создаёт renderer/scene/COM inputs; это локальная проверка генерации позиций.

Source:
[sampling и MT](../../Sparkplug/Analysis/PC/spParticleSampling.h),
[ParticleSystem method](../../Sparkplug/Code/Sparkplug/spParticleSystem.cpp),
[compiled regression](../../Sparkplug/Tests/spParticleSerializationTests.cpp).
Host явно ограничивает batch128, finite scalar magnitude65536 и4096 rejection
attempts на точку. Double intermediates воспроизводят всю matrix; все возможные
x87 rounding inputs не доказаны. Полная emission velocity/direction, lifetime
update, looping initialization, render/draw и clone остаются открыты.
Class scores пересчитываются отдельно, без автоматического PC→PS2 зачёта.

Full build79,51 с; CTest **63/63**,49,94 с. Viewer FormatTests **644 assertions**,
включая synthetic cylinder/cone и неизменённый pickup_ptc.smo. Исправление Viewer
хранится отдельным коммитом в submodule; публикация не подразумевается тестами.
Commit Viewer: `0d35bad`. Targeted class reanalysis обновил1745 objects/406
resources/7 variants в локальной базе,77,50 с. Python102/102 за3,757 с;
workbench sampling profile7/7 (252 cases) за11,02 с вместе с controller.
