# PC: массивы плоскостей видимости и общий блокер копирования

## Что подтверждено

| PC entry | Контракт | Что намеренно не делает |
| --- | --- | --- |
| `46B720 → 412D60` | resize массива20-byte planes; размер и fill-value20 передаются по значению, `ret18` | не пересчитывает внешний `activeCount+10` |
| `46B390` | общий insert, вызываемый resize в позиции end; capacity growth `max(required,capacity+floor(capacity/2))` | не равен reserve для внешнего plane-stack |
| `45DC40` | copy-конструирование диапазона:4 слова equation и1 byte enabled, шаг20 | не копирует3 padding bytes |
| `46ADC0 → 13DF2F0` | всем live planes присваивается младший byte аргумента; activeCount = size при ненулевом byte, иначе0 | не удаляет первую плоскость и не меняет размер/буфер |
| `45EA00 → 45E420` | уничтожение live elements, освобождение owned allocation, zero begin/end/capacity | не обнуляет allocator word00 и outer activeCount10 |
| `45E530 → 13DFCF0` | copy-constructor vector: отдельно выделяет ровно logical size исходника | не копирует source capacity, allocator00, outer activeCount10; не является assignment45E870 |
| `46C350 → 13DE550` | append plane-set в outer stack, включая spare capacity и relocation | не доказывает reserve-versus-size46C0F0 |

Термины resize/insert/set-enabled-all аналитические. Исходные имена типа
плоскости, контейнера и этих методов неизвестны; не создан новый «оригинальный»
класс по придуманному имени. Native plane-set20 = vector16 + activeCount4;
plane20 = equation4×float + enabled byte + padding3.
`allocatorWord` в analytical source — рабочая подпись слова00, а не
восстановленное имя поля. Его сохранность доказана; полная роль allocator/state
этого слова пока не установлена.

### Повторное использование и рост

Пустой resize0 не выделяет память. Shrink и clear оставляют прежнюю capacity;
возврат к большему logical size в пределах capacity заполняет только новый
суффикс. Resize до того же размера не перезаписывает существующие плоскости.

Float equation копируется побитно, без нормализации: сохраняются NaN payload
и signed zero. Enabled не нормализуется к1: raw7/255 остаётся7/255. В all-enable
аргумент256 означает low byte0,257 —1. Это observed ABI edge cases, а не
рекомендация передавать некорректные значения исходному bool API.

### Copy-constructor и стек наборов

Portable источник включает только fresh vector copy, не полный outer stack,
не assignment и не неизвестный manager constructor. Копирование в уже
сконструированный/self destination отклоняется host guard, а не объявляется
доказанным native overwrite/error behavior.

### Исходный путь

Allocation call46B4B3 передаёт строку по6DAF38:
`z:\sparkplug\code\sparkbase\spSTL_allocator.h (vector allocator)` и line421
(`0x1A5`). Это доказанный **allocator header**, не найденный header плоскостей
или translation unit Visibility. Подмена одного другим не допускается.

## Что всё ещё блокирует полный путь

Оба callers45E870 (`46C88C` child descent и `46CC77` camera-near portal branch)
после вызова **отдельно** копируют outer activeCount. Поэтому «просто скопировать
20-byte plane-set» не является доказанной реализацией45E870.

`45E870 → slot13B1DE8 → 13B5E20` и
`46C0F0 → slot13B20F0 → 13B92C0` входят вA0D3E0. СтатическиA0D3E0 обращается
к общему protected dispatcher888940; последующий шифрованный материал не
интерпретируется как обычный метод движка. Соседний45E880 имеет другое
состояние/vtable/размер и не принят за искомую реализацию только по соседству.

Для портала установлен дополнительный смысл46CC90: после копирования
унаследованного plane-set код **повторно включает все его плоскости**, включая
отключённые ранее, прежде чем рекурсивно проверять destination-zone roots.
Статическая caller связь и независимо исполненный all-enable подтверждены;
сам blocked near-plane whole-call не объявлен пройденным.

[spVisibilityPlaneStorage.h](../../../Sparkplug/Analysis/PC/spVisibilityPlaneStorage.h)
находится в analytical namespace, не в выдуманном original source tree.
Он моделирует logical size/capacity, value-copy, enabled и отдельный activeCount.
`std::vector::size()` внутреннего storage представляет native capacity;
реальная host allocation capacity не выдаётся за ABI. Native-indeterminate
padding в новой host памяти инициализируется0; host guard4096 и стандартные
исключения — явно безопасные отличия, не поведение native allocator failure.
