# AIAction: случайные параметры Minotaur и Mosquito

54 исполнения PC/PS2 прошли с первого раза. PC подтвердил полные методы
при найденной цели и реальные selector boundaries при её отсутствии.
PS2 отдельно подтвердил доступные prefixes; деление/общая ACC-арифметика
за указанными ниже границами не выдаются за выполненные операции.

## Подтверждённое поведение PC

| Класс | Подготовка и случайные значения |
| --- | --- |
| MinotaurAttack | command1D=1,target3A8,state3AC=0,word3B0=0,byte3BC=0,command pointer3C0. При цели один RNG result r;float3B4=float32(250000+r×240000/2³²) |
| MinotaurDefend | target3A8,state3AC=0;один RNG result r;deadline3B0=uint32(now+3000+r%1501);command pointer3B4 |
| MosquitoAttack | command1D=1,target3A8,state3AC=4;deadline3B0=uint32(now+1000);первый RNG r1 задаёт deadline3B4=uint32(now+500+r1%501);float3B8 меняет знак;второй RNG r2 задаёт move254=float32(100+r2×150/2³²);word3BC=47AFC800(90000f),byte3C4=0 |

При отсутствующей цели вызывается owner selector key1,parameter0.
MinotaurAttack делает это **до RNG**; его случайное значение за этой
границей не вычислялось. MinotaurDefend и Mosquito сначала выполняют
указанные RNG/записи, затем доходят до selector. Возврат selector не подменён.

Для Mosquito PC четыре пары outputs:
581869302/3890346734,3890346734/3586334585,
3586334585/545404204,4264392720/4112460519.
Высокий unsigned bit присутствует; PC не интерпретирует его как отрицательный
в итоговой формуле. Статические single-precision коэффициенты240000/2³²
и150/2³² подтверждены оригинальными words386A6000 и33160000.

## PS2: отдельно установленная граница

PS2 entries: MinotaurAttack25F280,MinotaurDefend25FCE0,Mosquito2605A0.
Perception исполняется полностью с оригинальным stack frame; own offsets
из таблицы в этой области больше PC на4. Вход содержит существующий timer
49FC80 и original RNG state,а не подменённые результаты этих методов.
