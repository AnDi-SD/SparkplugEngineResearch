# PC ParticleSystem: генератор случайных чисел и области эмиссии

## Случайные числа и геометрия

PRNG — MT19937 с624 UInt32, recurrence multiplier1812433253, twist offset397,
mask9908B0DF, tempering masks9D2C5680/EFC60000. Native initial index625
автоматически вызывает Seed5489; явный seed заканчивается index624.
После первого Next оригинал выдал3499211612, выполнив17274 инструкции и
настоящий seed helper. Source использует явный объект PRNG, без скрытой
зависимости от process-global state.

| Tag | Native sampler | Подтверждённое поведение |
| ---: | --- | --- |
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

Source:
[sampling и MT](../../../Sparkplug/Analysis/PC/spParticleSampling.h),
[ParticleSystem method](../../../Sparkplug/Code/Sparkplug/spParticleSystem.cpp),
[compiled regression](../../../Sparkplug/Tests/spParticleSerializationTests.cpp).
Host явно ограничивает batch128, finite scalar magnitude65536 и4096 rejection
attempts на точку. Double intermediates воспроизводят всю matrix; все возможные
x87 rounding inputs не доказаны. Полная emission velocity/direction, lifetime
update, looping initialization, render/draw и clone остаются открыты.
Class scores пересчитываются отдельно, без автоматического PC→PS2 зачёта.
