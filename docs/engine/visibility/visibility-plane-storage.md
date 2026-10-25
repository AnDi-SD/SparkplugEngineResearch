# PC: массивы плоскостей видимости

## Что подтверждено

| PC entry | Контракт | Что намеренно не делает |
| --- | --- | --- |
| `46B720 → 412D60` | resize массива20-byte planes; размер и fill-value20 передаются по значению, `ret18` | не пересчитывает внешний `activeCount+10` |
| `46B390` | общий insert, вызываемый resize в позиции end; capacity growth `max(required,capacity+floor(capacity/2))` | не равен reserve для внешнего plane-stack |
| `45DC40` | copy-конструирование диапазона:4 слова equation и1 byte enabled, шаг20 | не копирует3 padding bytes |
| `46ADC0 → 13DF2F0` | всем live planes присваивается младший byte аргумента; activeCount = size при ненулевом byte, иначе0 | не удаляет первую плоскость и не меняет размер/буфер |
| `45EA00 → 45E420` | уничтожение live elements, освобождение owned allocation, zero begin/end/capacity | не обнуляет allocator word00 и outer activeCount10 |
| `45E530 → 13DFCF0` | copy-constructor vector: отдельно выделяет ровно logical size исходника | не копирует source capacity, allocator00, outer activeCount10; не является assignment45E870 |
| `45E870 → 13B5E20` | vector assignment: self no-op; пустой источник освобождает destination buffer; непустой источник переиспользует достаточную capacity либо выделяет ровно source size | не копирует allocator00, outer activeCount10 и padding; не применяет resize growth1.5 |
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

`InitializeVectorCopyForAnalysis` включает fresh vector copy. Его отказ для
сконструированного/self destination — host guard, не native overwrite/error
behavior. Отдельный `AssignVectorForAnalysis` выполняет присваивание уже
сконструированному destination и допускает self no-op.

Для assignment источник с logical size0 освобождает буфер назначения даже
при собственной ненулевой capacity. Это отличается от resize0, который
сохраняет буфер. Непустой source size не больше destination capacity сохраняет
буфер, padding слотов и неиспользуемый хвост. При нехватке capacity выделяется
ровно source size. Equation words, включая NaN payload и signed zero, и raw
enabled byte копируются без арифметики. Allocator word и внешний activeCount
назначения сохраняются во всех этих ветвях.

### Исходный путь

Allocation call46B4B3 передаёт строку по6DAF38:
`z:\sparkplug\code\sparkbase\spSTL_allocator.h (vector allocator)` и line421
(`0x1A5`). Это доказанный **allocator header**, не найденный header плоскостей
или translation unit Visibility. Подмена одного другим не допускается.

## Assignment, outer count и отдельный reserve

Оба callers45E870 (`46C88C` child descent и `46CC77` camera-near portal branch)
после вызова **отдельно** копируют outer activeCount. Поэтому «просто скопировать
20-byte plane-set» не является реализацией45E870.

`45E870 → slot13B1DE8 → 13B5E20` проходит штатную защищённую подготовку
и затем исполняет настоящее присваивание. Девять отдельных исходных вызовов
закончились нормальным возвратом и уничтожением owned storage: self, два
варианта пустого источника, оба пустых, shrink, равный размер, рост внутри
capacity, рост с перевыделением и исходно пустое назначение. Текущий C++ helper
после канонической сборки совпал на всех девяти входах: return identity,
metadata, raw equation/enabled values, reuse и все bytes сохранённого буфера.
Indeterminate padding нового native allocation не сравнивается с нулевой
инициализацией host. Защищённая подготовка
не подменялась успешным результатом или заранее расшифрованным телом.

`46C0F0 → slot13B20F0 → 13B92C0` отдельно подготавливает внешний vector
plane-set records. В настоящем Visibility constructor аргумент32 означает
capacity32 при logical size0; это не создание32 live plane sets. Настоящие
factory, deleting destructor и shutdown scratch pool завершились с
освобождением всех выделений на этом входе. Полные populated reserve/relocation
и allocator-failure paths не следуют из одного конструктора.

Для портала установлен дополнительный смысл46CC90: после копирования
унаследованного plane-set код **повторно включает все его плоскости**, включая
отключённые ранее, прежде чем рекурсивно проверять destination-zone roots.
Статическая caller связь и независимо исполненный all-enable подтверждены;
восемнадцать полных portal-вызовов, включая epsilon band, совпали с текущим
C++ по словам плоскостей, enabled/count, аргументам рекурсии, порядку и меткам.
Каждый исходный вызов использовал свежий guest и штатную подготовку assignment.
Это ограниченная проверка выбранных входов, не численная гарантия для всего
домена и не завершение полного visibility subsystem.

[spVisibilityPlaneStorage.h](../../../Sparkplug/Analysis/PC/spVisibilityPlaneStorage.h)
находится в analytical namespace, не в выдуманном original source tree.
Он моделирует logical size/capacity, value-copy, enabled и отдельный activeCount.
`std::vector::size()` внутреннего storage представляет native capacity;
реальная host allocation capacity не выдаётся за ABI. Native-indeterminate
padding в новой host памяти инициализируется0; host guard4096 и стандартные
исключения — явно безопасные отличия, не поведение native allocator failure.
