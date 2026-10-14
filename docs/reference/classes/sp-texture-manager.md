# spTextureManager

`spTextureManager` имеет Class ID `0x2F434D04` и зарегистрированную базу
`spBaseObject` (`0x415352A1`) на PC и PS2. [Общие исходники](../../../Sparkplug/Code/Sparkplug/spTextureManager.h)
восстанавливают известную PC-семантику lifetime и резервирования. Исходные
имена методов, header и translation unit не установлены.

В PC-таблице после семи операций `spBaseObject` находятся два pure-call слота
`+0x1C` и `+0x20`. Базовый manager не имеет registered factory. Его
[PC-наследник](sp-dx-texture-manager.md) конкретен и реализует эти слоты
немедленными возвратами без действий.

## PC layout

База и `spDXTextureManager` имеют общий extent `0x2C`. Leaf-класс не добавляет
полей. [Независимый PC32 layout](../../../Sparkplug/Analysis/PC/spTextureManagerAbi.h)
не зависит от переносимого C++ ABI.

| Offset | Размер | Значение |
| ---: | ---: | --- |
| `+0x00` | `0x10` | `spBaseObject` |
| `+0x10` | `4` | vptr singleton-support subobject |
| `+0x14` | `1` | raw reserve flag, default `0` |
| `+0x15` | `3` | constructor не записывает padding |
| `+0x18` | `4` | запрошенное количество записей, default `0` |
| `+0x1C` | `4` | незаписанное allocator storage старого MSVC vector |
| `+0x20` | `4` | begin, default `0` |
| `+0x24` | `4` | end, default `0` |
| `+0x28` | `4` | capacity end, default `0` |

Каждая запись контейнера занимает `0x0C`. Reserve переносит её как три
32-битных слова. Смысл слов, их исходные типы и производитель записей пока
не установлены. Поэтому реализация не приписывает им texture ID, sampler
или ownership ресурса.

## Настройка reserve

Операция сначала записывает raw byte flag и запрошенный count. При нулевом
flag она возвращается, сохраняя существующие записи и capacity; count в этой
ветви не ограничивается. Любой ненулевой byte вызывает reserve.

Если требуемый count не превышает capacity, allocation и записи сохраняются.
При росте создаётся allocation ровно для запрошенного числа `0x0C`-байтных
записей, активные записи побайтно переносятся и прежняя allocation освобождается.
Число активных записей не меняется. Уменьшение count не уменьшает контейнер.
Начальный PC caller задаёт `flag=1, count=0x400`.

32-битный контейнер проверяет предел `0x15555555` до выделения памяти.
Превышение при ненулевом flag выходит в native length-error path. Сам flag
и count к этому моменту уже записаны. Portable `ConfigureReserveForAnalysis`
сохраняет этот порядок и возвращает false при length/allocation failure;
это явный host-result, исходный успешный return type не восстановлен.
Native exception unwinding полностью не реконструирован.

## Singleton и освобождение

Constructor публикует указатель на весь объект, даже если другой экземпляр
уже существовал. Destructor освобождает vector storage и безусловно очищает
global singleton. Если уничтожить старый экземпляр после создания нового,
global всё равно станет нулём. Portable код сохраняет это поведение.

Destructor не вызывает операций для отдельных `0x0C`-байтных записей и не
освобождает потенциальные pointees из их слов. Внешний код отвечает за их
неустановленное предметное владение. `SetRecordStorageForAnalysis` — явно
обозначенный host-input для уже заполненных opaque records, не восстановленный
метод регистрации текстуры.

Открыты исходные имена, значение записей и их producer/consumer graph.
PS2 extent и container ABI отличаются; PC layout и PC no-op операции
PS2-реализации не приписываются.
