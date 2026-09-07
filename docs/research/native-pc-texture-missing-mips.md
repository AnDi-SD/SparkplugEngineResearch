# PC: недостающие mip-уровни raw TextureData

CP108, 8 сентября 2026. Восстановлен путь генерации недостающих уровней для
нативной несжатой текстуры: `4ABBA0 → 4AB030 → 61039A → 60FDB4 → 61C44F`.
Original исполняет сам фильтр и byte/float codec; C++ reader теперь дополняет
неполную raw цепочку до1×1. Сжатые DXT missing levels остаются открыты.

Исходный материал — неизменённый объект TextureData ID10 `load_default` из
`Menus/loading.smo`: весь файл2442 байта, SHA-256
`0E8EB7A89E952CD0CF096AE4F5E3F1FE4D56BEC3696DB427567F0BB2BF04427E`.
Объект в offset1053 имеет1088 байт, reader получает1080 байт после заголовка.
Embedded source содержит один raw уровень16×16. Original создаёт пять
поверхностей и заполняет недостающие8×8,4×4,2×2,1×1. Базовые runtime поля
получают `[5,1,0,1,16,16]`; format44 остаётся unwritten, size48 остаётся0.
Файл пока используется как точный срез объекта; whole loading.smo — следующий
отдельный этап, а не результат этого checkpoint.

## Восстановленная фильтрация

`4AB030` передаёт filter4 и вызывает копирование предыдущего уровня в следующий.
`619219` строит таблицы распределения вкладов исходных пикселей. При уменьшении
вдвое эквивалентные четыре веса — `[1,7,7,1]/16`; координаты переходят через
границу к противоположному краю. Native coefficient tables сняты до освобождения.
Для размера2 совпавшие адресаты объединены в вес1/2, для размера1 — вес1.

Одних целочисленных весов недостаточно для точного результата. Codec `625743`
умножает byte channels на binary32 constant `3B808081` и записывает float.
Фильтр накапливает вклады в порядке source row/column и округляет каждую запись
накопителя до float. Codec `61F153` временно ставит x87 RC=truncate, умножает
на255, сохраняет float, прибавляет1/2, опять сохраняет float и преобразует в integer.
`spTextureMipFilter.h` воспроизводит эти стадии без изменения общего FP mode.

Например, четыре blue bytes `[134,138,125,129]` дают131. Простое округление
целочисленного среднего131,5 дало бы132. Этот случай закреплён C++ regression.
Проверочные изображения включают градиент, одиночный яркий пиксель, края,
шахматный рисунок, случайные RGBA bytes и неизменённый corpus slice.
Нормализация цвета не включает premultiplied alpha или gamma conversion:
такое поведение данным native branch не показано.

## Стенд и ограничения

Заранее выбран file profile:1 млн инструкций/8 с, процесс30 с, arena128 КиБ,
один native allocation32 КиБ. COM surfaces: стороны1/2/4/8/16, до5 уровней,
до2048 байт на поверхность, row padding4 байта. Пустые/частичные LockRect
не подставляются: разрешён только полный уровень и наблюдаемые flags.
Original balances surface AddRef/Release и Lock/Unlock; padding остаётся intact.

Ранние fresh runs последовательно остановились на неописанных external inputs:
GetModuleHandleA, дополнительный LockRect flag, MSVCR71 `_ftol`, затем `floor`.
Остановленные guests не возобновлялись. Стенд объявляет отсутствие двух debug
DLL и registry key Direct3D; resolver61632D и registry helper6462BB исполняются
оригинально. DLL не загружаются, registry/OS calls наружу не передаются.
CRT `_ftol` точно декодирует integer mantissa x87, не округляя её сначала в
double; guards отдельно проверяют значение непосредственно ниже1 и signed64.
`floor` ограничен конечными малыми входами. Internal filter seams отсутствуют.

Corpus native reader:129571 инструкция, около0,83 с,62480 arena bytes,
49 tracked allocations освобождены. Это CPU/COM evidence, не live GPU rendering.
Портируемый filter ограничен power-of-two raw RGBA; malformed input и invalid
pitch отклоняются. Общие произвольные resizes, compressed generation, другие
форматы/flags, дополнительные color transforms и все failures не закрыты.

```powershell
python research/compare_pc_texture_missing_mips.py corpus
python research/compare_pc_texture_missing_mips.py random 16x8
python research/compare_pc_texture_missing_mips.py random 1x16
```

[CP108 manifest](../../research/native-cycle-checkpoint-2026-09-08-cp108.json)
содержит10 свежих сравнений:9136 mip bytes, из них2336 сгенерированы. Прежние21
texture cases, CTest62/62 и Python92/92 также прошли. Class scores
этим checkpoint автоматически не повышаются.
