# CP112: original RGB endpoint optimizer

[CP113](native-pc-texture-compressed-mips.md) использует этот optimizer в
проверенных block encoders и общем compressed mip reader.

8 сентября 2026. Перенесён PC `64B5C0`: выбор двух концов цветовой палитры
из 16 взвешенных RGB pixels. Четыре batches по 104 случая совпали по всем
2496 endpoint float words; это 392 уникальных пары input/steps и 24 повторения
общих контрольных случаев. Все 3 377 950 original инструкций выполнены без seams.
Batch занимал 7,14–7,37 с при четырёх одновременных процессах; arena 288 байт
на guest, прежние micro 100k/2 с на вызов и 30 с на child.

Код находится в [spTextureBlockOptimizer.h](../../Sparkplug/Analysis/PC/spTextureBlockOptimizer.h),
сравнение — в [compare_pc_texture_block_optimizer.py](../../research/compare_pc_texture_block_optimizer.py).
Это следующий слой после [DXT decoder CP111](native-pc-texture-compressed-blocks.md);
готовый C++ compressed mip pipeline пока не объявляется.

## Наблюдаемое поведение

Вход — 16 float RGBA records, используются RGB. `64BB40` предварительно
квантует цвет и применяет веса `3E981530`, `3F800000`, `3DCE6734`.
Optimizer начинает с component minima/maxima и проверяет четыре направления,
выбирая ориентацию G/B. Первый score накапливается в x87, остальные три
сохраняются в float после каждого pixel; равный score не заменяет победителя.

При малом расстоянии концов возвращается начальный результат. Остальные
случаи выполняют до восьми итераций для палитры из трёх или четырёх цветов:
строят palette, проецируют pixels на направление, накапливают gradients и
curvature, корректируют endpoints. Итерации выполняются с x87 RC=truncate,
выход восстанавливает прежний control word. Original output записывает только
три float компонента каждого конца; четвёртое слово caller storage не меняется.

Host API ограничивает input конечными RGB в диапазонах этих весов и steps3/4;
invalid input не заменяет output. Это явные пределы сравненного слоя, не
приписанные native функции guards. Дополнительный trace включается только
явным caller pointer и не используется обычным optimizer call.

## Найденное различие double и x87

Первый batch дал 23/24 точных совпадения. В оставшемся случае на один bit
отличался B одного endpoint. Read-only hook в `64BA2D` локализовал отличие
между одинаковыми gradients и следующей записью endpoint:

```text
endpoint = 0
gradient bits = BCCE6730
curvature = 0.75
original updated bits = 3D099A1F
binary64 expression followed by float truncate = 3D099A20
```

Original reciprocal `-1/curvature` сохраняет 64-bit significand остаток.
Округление промежуточного произведения в binary64 теряло его прежде, чем
float store мог выбрать нижнее значение. В
[spFloat80TowardZero.h](../../Sparkplug/Analysis/PC/spFloat80TowardZero.h)
добавлена ограниченная точная арифметика: reciprocal положительного float,
умножение на float, сложение с endpoint и final float store. Внутренние
целые limbs воспроизводят truncate на каждом extended шаге. Это не полная
x87 VM: NaN/Inf, exceptions и прочие операции в этот helper не входят.

После исправления совпали все 24 исходных случая и четыре расширенных batch.
У проблемного блока совпали также четыре промежуточных state rows по17 слов.
Первый failed comparison и первый trace сохранены отдельно в ignored local-data;
исходный stopped/mismatched опыт не выдаётся за успешный после изменения кода.

```powershell
python research/compare_pc_texture_block_optimizer.py check 48 0
python research/native_workbench.py run pc-texture-block-optimizer --workers 4 --deadline-utc 2026-09-08T04:00:00Z
```

Следующая задача — `64BB40` color block encoding и DXT5 alpha optimizer
`64B299`, затем сравнение packed bytes и подключение compressed missing mips.
Scores не менялись. [Manifest CP112](../../research/native-cycle-checkpoint-2026-09-08-cp112.json).
