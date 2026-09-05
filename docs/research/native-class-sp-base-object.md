# Система объектов SparkBase: `spBaseObject` и `spNamedObject`

Статус: углублённый собираемый срез реконструкции. Исполняемые файлы и игровые
ресурсы не изменялись. Точные 32-битные layouts отделены от переносимой C++
реализации, а недоказанные имена методов сохраняются как `vfunc_<offset>`.

## Происхождение

В PC executable буквально присутствует исходный путь:

`Z:\Sparkplug\Code\SparkBase\spBaseObject.cpp`

Регистрации обеих платформ подтверждают находящиеся в этом слое классы
`spBaseObject`, `spCloneManager`, `spNamedObject`, `spRTTIManager` и
`spPropertySystem`. Имя исходного заголовка и наличие C++ namespace в оригинале
пока не установлены.

Контрольные бинарники:

| Платформа | Файл | SHA-256 |
|---|---|---|
| PC | `local-data/pc-pristine/WinxClub.exe` | `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` |
| PS2 | `local-data/Winx Club the game PS2/SLES_532.19` | `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE` |

## Нативная регистрация и RTTI

Общий constructor регистрационной записи находится по PC RVA `0x00012FF0` и
PS2 VA `0x001115B0`. Реализации двух платформ подтверждают одинаковый 32-битный
контракт:

| Смещение | Размер | Значение |
|---:|---:|---|
| `0x00` | 4 | class ID |
| `0x04` | 4 | base class ID, `0` у корня |
| `0x08` | `0x40` | встроенная нуль-терминированная строка имени класса |
| `0x48` | 4 | указатель на регистрационную запись базового класса |
| `0x4C` | 4 | factory callback; `0` для несоздаваемого типа |
| `0x50` | `0x0C` | встроенная группа свойств: owner registration, count, first record |
| `0x5C` | 4 | пока неизвестное слово |

Размер записи — `0x60`. PC-код непосредственно копирует имя в `record+0x08`,
а обе версии сохраняют factory в `+0x4C` и добавляют запись в singleton
`spRTTIManager`.

Трактовка блока `+0x50` как узла RTTI-реестра была ошибочной. Constructor
`0x001115B0` передаёт адрес этого блока в `0x00111000`, который записывает
указатель на содержащую registration в `+0x00` группы и обнуляет `+0x04/+0x08`.
Дальнейшие consumers дают точный контракт:

| Смещение внутри группы | Значение |
|---:|---|
| `+0x00` | owner registration |
| `+0x04` | число свойств именно этого класса |
| `+0x08` | первый элемент непрерывного массива записей по `0x58` байт |

`0x00110FE0(group, property)` устанавливает первый элемент и увеличивает count;
в PS2-коде найдено 283 прямых вызова этого добавления. `0x00110E50` возвращает
элемент по индексу как `first + index * 0x58`. Поиск `0x00110E70(group, name)`
сначала рекурсивно проходит группу базовой registration через `owner+0x48`,
затем сравнивает имя с `record+0x04` в текущей группе. Проверка
`wxVectorWrapper` по `registration+0x54` тем самым проверяет именно локальный
count и не даёт повторно установить свойства класса.

Последний аргумент constructor не сохраняется в записи. После вставки он
вызывается один раз как post-registration callback. Его назначение теперь
подтверждено как регистрация свойств: callback `wxVectorWrapper` создаёт записи
`X Value`, `Y Value` и `Z Value` в singleton `spPropertySystem`. Ровно 125
одинаковых имён классов имеют такой callback на PC и PS2. У `spBaseObject`
callback просто возвращает `true`, у `spNamedObject` он отсутствует.

Регистрации двух пилотных классов:

| Поле | `spBaseObject` PC / PS2 | `spNamedObject` PC / PS2 |
|---|---:|---:|
| Class ID | `0x415352A1` / `0x415352A1` | `0x44DE07FD` / `0x44DE07FD` |
| Base ID | `0` / `0` | `0x415352A1` / `0x415352A1` |
| Registration object | `0x00755310` / `0x0049FF60` | `0x007555F8` / `0x004A02C0` |
| Factory | `0` / `0` | `0x00413190` / `0x00106070` |
| Property callback | `0x004F3DF0` / `0x00100380` | `0` / `0` |

Менеджер по class ID умеет найти registration, вернуть её и вызвать factory из
`+0x4C`. Это тот же механизм, через который resource loader создаёт объекты, а
не отдельная система специально для SMO.

## `spBaseObject`: общий 32-битный layout и lifetime

Constructor `sub_00102BF0` инициализирует:

```text
this + 0x00 = vtable 0x0048C5E0
this + 0x04 = 0
this + 0x08 = uint16(0)
this + 0x0C = 0
sizeof(this) = 0x10
```

PC constructor `0x0040E910` независимо записывает те же четыре значения:
`vptr`, ноль в `+0x04`, **одно 16-битное** значение в `+0x08` и ноль в
`+0x0C`. Ни PC, ни PS2 constructor не записывает байты `+0x0A/+0x0B`.
Поэтому это выравнивающая прокладка после `uint16`, а не отдельное поле. Полный
общий layout двух проверенных сборок:

| Смещение | Размер | Подтверждённая роль |
|---:|---:|---|
| `+0x00` | 4 | vptr |
| `+0x04` | 4 | optional список обратных ссылок; original member name неизвестно |
| `+0x08` | 2 | intrusive strong-reference count |
| `+0x0A` | 2 | alignment padding; constructor не инициализирует |
| `+0x0C` | 4 | сохраняемое/копируемое состояние неизвестной роли |

Слово `+0x0C` нельзя считать просто резервным нулём. PS2 copy paths
`0x00172190` (`spMaterialMovieTexture`), `0x0018CB20` и `0x0018C430`
(`spQuad`) копируют его вместе с `+0x04` и halfword `+0x08`; PC paths
`0x004D7F70` и `0x004D8010` делают то же самое. При этом полный поиск доступов
в translation unit `spBaseObject.cpp` на обеих платформах не нашёл ни одного
поведенческого чтения `+0x0C`: там есть только инициализация, а глобальные
кандидаты после проверки оказались либо указанными копиями base storage, либо
чужими структурами. Поэтому факт копирования доказан, но source name и смысл
поля остаются неизвестными. Наблюдаемое shallow-копирование списка и счётчика
фиксируется как native behavior и не переносится автоматически в безопасный
portable ownership API.

Предыдущая трактовка `+0x04` как контейнера неизвестных свойств оказалась
неверной. `sub_00100860(target, owner)` лениво выделяет по этому адресу
двусвязный список размером `0x0C`, а затем добавляет `owner` в список обратных
ссылок `target`. `sub_00100820(target, owner)` удаляет ту же пару. Это
подтверждается callers замены object-reference: старый target удаляет owner из
списка и теряет ссылку, новый target увеличивает счётчик и добавляет owner.

Layout списка и его узла:

| Объект | Смещение | Значение |
|---|---:|---|
| list header | `+0x00` | число узлов |
| list header | `+0x04/+0x08` | next/previous встроенного sentinel |
| node | `+0x00/+0x04` | next/previous |
| node | `+0x08` | указатель на referring owner |

И header, и node имеют размер `0x0C`. `sub_00103950` инициализирует пустой
sentinel, `sub_00103810` вставляет узел, `sub_00103760` удаляет его, а
`sub_001008E0` уничтожает все узлы и header. Payload не удаляется как owned
object. Исходные имена типа списка и члена класса пока не найдены; термин
«список обратных ссылок» остаётся аналитическим.

Дополнительные проходы `0x001003C0`, `0x00100520` и `0x00100610` перебирают
referring owners из этого списка и передают им уведомления/контекст изменения.
`0x00100520` полностью строит запись размером `0x20`, а `0x00100610` пересылает
уже существующую запись без изменений:

| Смещение | Значение в `0x00100520` | Значение в `0x001003C0` |
|---:|---|---|
| `+0x00` | входной event code | входной event code |
| `+0x04/+0x08/+0x0C` | нули | нули |
| `+0x10` | source/target object | source/target object |
| `+0x14` | ноль | текущий referring owner |
| `+0x18/+0x1C` | два входных context-аргумента | два входных context-аргумента |

У корневого `spBaseObject` соответствующий virtual slot `+0x0C` пуст, но его
сигнатура включает указатель на эту запись: на PC пустой handler завершает
вызов как `ret 4`, а PS2 callers передают record в `a1`. Производные referring
objects переопределяют слот; найдены в том числе overrides `0x00154900` и
`0x001A5AC0`, которые пересылают record через `0x00100610`. Таким образом,
список обслуживает не только lifetime relations, но и распространение
обновлений по обратным связям. Имена типа, полей и протокола остаются
аналитическими, но его бинарный контракт теперь известен.

Поле `+0x08` подтверждено как 16-битный intrusive reference count. Callers
увеличивают его при установке object-reference и уменьшают при замене/удалении;
при достижении нуля вызывается destructor target. Исходное имя поля и точные
имена операций retain/release пока неизвестны. В PS2 executable найдено 176
прямых вызовов добавления обратной ссылки `0x00100860` и 114 вызовов удаления
`0x00100820`, то есть это общий object-reference protocol, а не частный helper
одного класса.

## Исправленная граница vtable

PS2 vtable начинается по `0x0048C5E0`. Два начальных нуля являются служебной
частью ABI; вызовы в коде используют следующие смещения:

| Смещение | PS2 VA | Подтверждённое поведение |
|---:|---:|---|
| `+0x08` | `0x00102B50` | deleting-destructor; освобождает список обратных ссылок |
| `+0x0C` | `0x00100810` | пустой обработчик; исходная роль неизвестна |
| `+0x10` | `0x00100370` | возвращает `nullptr`; clone-construction slot корня |
| `+0x14` | `0x00100320` | базовая часть копирования при clone, возвращает `true` |
| `+0x18` | `0x00102830` | возвращает registration `0x0049FF60` |
| `+0x1C` | `0x00100010` | сравнивает class ID только текущего registration |
| `+0x20` | `0x00100050` | проходит цепочку `baseRegistration` до совпадения ID |
| `+0x24` | `0` | терминатор/неиспользуемое слово |

Ранее функция `0x00104E90` ошибочно считалась слотом `+0x30` этой таблицы.
Сырой дамп и constructor соседнего объекта показывают, что следующая vtable
начинается уже по `0x0048C608`; `0x00104E90` находится в её `+0x08`.

PC даёт независимую полную таблицу по `0x006DAEB8`. В ней нет двух начальных
служебных слов PS2 ABI, поэтому одинаковые смысловые slots сдвинуты на `-0x08`:

| PC slot | PC VA | Соответствие PS2 |
|---:|---:|---:|
| `+0x00` | `0x00411CC0` | deleting destructor `+0x08` |
| `+0x04` | `0x005B7A00` | пустой notification handler `+0x0C` |
| `+0x08` | `0x004A1BF0` | clone-construction, возвращает null `+0x10` |
| `+0x0C` | `0x0040ECE0` | clone-copy `+0x14` |
| `+0x10` | `0x0040E930` | registration getter `+0x18` |
| `+0x14` | `0x00408350` | exact-type check `+0x1C` |
| `+0x18` | `0x00408370` | base-chain check `+0x20` |

PC non-deleting destructor `0x004102B0` ставит базовую vtable, разрушает и
освобождает optional list по `+0x04`, затем обнуляет указатель. Это совпадает с
PS2 lifetime-путём и дополнительно подтверждает назначение поля.

## `spCloneManager` и контракт клонирования

`spCloneManager` хранит соответствие `source object -> clone` и счётчик глубины.
Перед виртуальным копированием он проверяет, не создавался ли clone ранее. Это
не косметическая копия одного объекта: map нужен для сохранения общих ссылок и
циклов объектного графа.

Наблюдаемый порядок для создаваемого класса:

1. выделить объект нужного размера;
2. вызвать constructor базового класса и установить производную vtable;
3. зарегистрировать пару source/clone в `spCloneManager`;
4. вызвать source vtable slot `+0x14` для копирования базовых и производных полей;
5. при неуспехе удалить незавершённый clone;
6. после выхода из корневого clone очистить временную map.

Первый переносимый срез уже реализует lifetime временной map, регистрацию
пары source/clone и порядок копирования простого leaf-object. Повторное
разрешение ссылок и циклов пока не перенесено: для этого нужен ещё один
небольшой производный класс с реальными object references. Поэтому полная
семантика native `spCloneManager` пока считается незакрытой.

Теперь восстановлен и его PS2 layout. Регистрация имеет ID `0xC4419F78`, base
ID `0x415352A1`, registration object `0x004A0200` и factory `0x001053A0`.
Factory выделяет `0x18` байт:

| Смещение | Размер | Значение |
|---:|---:|---|
| `0x00` | `0x10` | base `spBaseObject` |
| `0x10` | 4 | vptr полиморфного singleton-support base |
| `0x14` | 4 | текущая глубина clone |

Основная vtable находится по `0x0048C620`. Второй base имеет отдельную vtable и
destructor thunk с коррекцией `this - 0x10`. Его назначение подтверждено
constructor/destructor-парами: временная vtable ставится на secondary subobject,
constructor публикует адрес полного объекта в отдельный глобальный instance,
а destructor его сбрасывает. В evidence-layout он поэтому отмечен аналитическим
именем `SingletonSupportSubobjectLayout`. Исходное имя, вероятно имя шаблонной
специализации, не найдено ни в строках, ни в доступном RTTI и остаётся открытым.

Map не является полем manager: это отдельный статический контейнер размером
`0x10` по `0x004A01F0`, непосредственно перед registration object. Entry
`0x00104F10` сначала ищет source в этой map, затем лениво создаёт singleton,
увеличивает глубину, вызывает virtual clone и очищает map только после выхода
из корневого вызова. `0x001050C0` регистрирует пару source/clone.

PC подтверждает тот же class/base ID и размер `0x18`: factory `0x00412540`
выделяет `0x18`, primary vptr устанавливается в `0x006DB54C`, secondary — в
`0x006DB548`. Это независимая проверка размера, но не доказательство исходного
имени второго base.

## `spRTTIManager`

Регистрация `spRTTIManager` имеет одинаковые ID на обеих платформах: class ID
`0x5EA0637A`, base ID `0x415352A1`. На PS2 registration object находится по
`0x004A2550`, factory — `0x00111AF0`, primary vtable — `0x0048C910`.

PS2 factory выделяет `0x24` байта. После base `spBaseObject` следует тот же
singleton-support subobject по `+0x10`, а с `+0x14` начинается собственное дерево
registrations размером `0x10`. Восстановлены четыре операции:

| PS2 VA | Поведение |
|---:|---|
| `0x001116B0` | найти registration по class ID и вызвать factory `+0x4C` |
| `0x00111720` | вернуть registration либо `nullptr` |
| `0x00111780` | проверить наличие class ID |
| `0x001117E0` | вставить registration, ключ — поле `+0x00` записи |

Destructor `0x00111820` разрушает дерево и сбрасывает singleton. Constructor
также обеспечивает существование `spPropertySystem`, что фиксирует зависимость
между подсистемами.

PC registration object находится по `0x0075A368`, factory `0x00414C60`
выделяет `0x20`, а не `0x24` байта. Наиболее вероятная причина — меньший
platform-specific tree object после `+0x14`, однако это вывод по размерам, не
прочитанный layout: constructor защищён/перенаправлен SecuROM. Поэтому PS2
layout `0x24` зафиксирован как exact evidence, а единый псевдоуниверсальный
layout для двух платформ не создаётся.

## `spPropertySystem`

Этот соседний класс удалось закрыть до уровня layout. Его class ID —
`0x584F73E4`, base ID — `0x415352A1`. PS2 registration object находится по
`0x004A24F0`, factory `0x00111560` выделяет `0x20` байт, primary vtable —
`0x0048C8E0`. PC factory `0x004177F0` независимо выделяет те же `0x20` байт.

| Смещение | Размер | Подтверждённое значение |
|---:|---:|---|
| `0x00` | `0x10` | base `spBaseObject` |
| `0x10` | 4 | vptr полиморфного singleton-support base |
| `0x14` | 4 | capacity массива property records |
| `0x18` | 4 | текущее число records |
| `0x1C` | 4 | указатель на массив records |

Инициализатор `0x00111020` получает `0x400`, вычисляет `0x58 * capacity`,
создаёт массив и сохраняет `capacity=1024`, `count=0`. Поэтому `0x400` теперь
подтверждено как число резервируемых записей, а не флаг. Property callback
`wxVectorWrapper` вычисляет `entries + count * 0x58`, копирует туда очередную
запись и увеличивает `count`; это связывает layout с реальным producer.

Часть схемы записи теперь также доказана:

| Смещение | Подтверждённое значение |
|---:|---|
| `+0x00` | vptr type-specific стратегии сравнения; virtual slot `+0x08` сравнивает значение свойства у двух объектов через getter |
| `+0x04` | имя свойства; это поле сравнивает поиск `0x00110E70` |
| `+0x08` | числовой property type для parser dispatch |
| `+0x0C/+0x10` | пока неизвестные параметры |
| `+0x14` | флаги accessor-а; bit 0 добавляет `propertyName` к аргументам getter/setter |
| `+0x18` | 12-байтный descriptor getter-а |
| `+0x24` | 12-байтный descriptor setter-а |
| `+0x30..+0x3B` | type-specific union: у type `6` в `+0x30` лежит таблица имён enum; ветвь type `7` может трактовать весь участок как callable descriptor |
| `+0x3C..+0x47` | descriptor-like участок; точная общая роль пока неизвестна |
| `+0x48` | необязательный прямой callback, вызываемый в property stream path; исходное имя и полный контракт неизвестны |
| `+0x4C` | пока неизвестное поле |
| `+0x50..+0x57` | type-specific payload: у float-варианта два float, у bool-варианта используются отдельные байты |

Общий constructor `0x00110DB0` устанавливает vptr, пустое имя, type `11` и
нулевые defaults базовой части размером `0x50`. Type-specific constructors
меняют vptr стратегии сравнения и инициализируют payload `+0x50..+0x57`, так
что полный типизированный record занимает `0x58`; исходные имена базового и
производных C++-типов пока неизвестны. Массив создаётся wrapper-ом `0x00111140`.
`wxVectorWrapper` helper `0x002A4CE0` задаёт имена `X/Y/Z Value`, type `3` и
два трёхкомпонентных набора float metadata.

Callable ABI на PS2 теперь восстановлен точно. Getter и setter представлены
одинаковым тройным descriptor-ом:

| Слово | Наблюдаемая роль |
|---:|---|
| `+0x00` | signed поправка `this` |
| `+0x04` | signed byte offset virtual slot; отрицательное значение означает прямой вызов |
| `+0x08` | адрес прямой функции либо offset указателя на vtable внутри скорректированного объекта |

Helpers `0x003FE4C0` и `0x003FE500` отличаются местом object argument: `a0` в
обычном вызове и `a1`, когда `a0` занят hidden aggregate-return pointer. После
поправки `this` helper при неотрицательном втором слове читает vptr по offset из
третьего слова и virtual target по offset из второго; иначе прыгает прямо по
адресу из третьего слова. В статических property metadata многократно встречается
прямая форма `{0, -1, function}`. Raw scan нашёл 112 вызовов этих helpers; все
распознанные property-вызовы передают descriptor ровно из `record+0x18` или
`record+0x24`. Проверка descriptor-а на пустоту находится по `0x003FE540`.

Bit 0 в `record+0x14` не влияет на выбор прямой/виртуальной формы descriptor-а. При
установленном бите getter получает `(object, propertyName)`, а setter —
`(object, propertyName, value)`; без него имя из аргументов исключается.

Consumer `0x00100960` получает registration объекта через vtable `+0x18`, ищет
property по группе `registration+0x50` и переключается по `record+0x08`:

| Type | Наблюдаемое чтение текста |
|---:|---|
| `0` | `true` / `false` |
| `1`, `2` | целое число; точные исходные типы различий пока неизвестны |
| `3` | float |
| `4` | string; найдены две разные typed-вариации с одним dispatch value |
| `5` | восемь шестнадцатеричных цифр; вероятно packed value, точное имя неизвестно |
| `6` | целочисленное значение с enum metadata: `+0x30` у доказанного примера указывает на null-terminated список `Directional/Point/Spot` |
| `7` | no-conversion в text parser; binary path создаёт объект через RTTI и использует дополнительные callbacks, что указывает на relationship/collection-семантику, но точный тип ещё не доказан |
| `8` | три float |
| `9` | диагностическая ветвь `Unknown property type...` |
| `10` | четыре float |

Значение `11` находится уже за пределами таблицы и служит исходным
неинициализированным/неподдерживаемым состоянием. Callable ABI и положения
getter/setter закрыты, но общая семантика участков `+0x30/+0x3C/+0x48/+0x4C`,
исходные enum/type names и имена `capacity/count/entries` всё ещё не восстановлены.
Эти обозначения остаются аналитическими. Singleton-support base присутствует
физически, но его original type name пока неизвестен.

### Создание объекта и чтение properties

Граница между двумя форматами создания объекта подтверждается внешним caller
`0x00156D40`: он заглядывает в первый байт stream и при символе `<` вызывает
`0x00102840`, иначе — `0x00102A00`.

| PS2 VA | Доказанная роль |
|---:|---|
| `0x00102840` | XML `CreateFromStream`: берёт class ID из первой header-записи property queue, создаёт объект через `spRTTIManager`, затем применяет очередь через `0x00100D10` |
| `0x00102A00` | binary `CreateFromStream`: читает header размером `0x18`, создаёт объект по class ID и передаёт его в `0x00101E80` |
| `0x00100D10` | применяет XML property queue к уже созданному объекту; текстовые значения доходят до parser `0x00100960` |
| `0x00101E80` | читает binary property header и значения в существующий объект, включая вложенные object/collection branches |
| `0x001012F0` | копирует/извлекает property stream между двумя stream-интерфейсами и проверяет совпадение конечных позиций |

Строки самого executable подтверждают термин `CreateFromStreamXML`, наличие
property queue/header и ошибки неизвестного RTTI. Это не отдельный SMO loader:
создание всегда заканчивается общим RTTI factory и общим property dispatch.
Адреса выше занесены как analytical aliases; исходные C++-сигнатуры целиком
не восстановлены, а virtual slots `+0x30/+0x34/+0x38/+0x3C` принадлежат
переданному stream-интерфейсу, не vtable `spBaseObject`.

## `spNamedObject`

Factory `0x00106070` выделяет `0x14` байт, вызывает `spBaseObject` constructor,
ставит vtable `0x0048C690` и обнуляет первое собственное поле `+0x10`.

Поле `+0x10` — указатель на shared string entry. В entry байт по `+0x08`
используется как refcount; строковые данные начинаются с `+0x09`. При копировании
`sub_00105DC0` освобождает старое имя назначения, увеличивает refcount источника
и передаёт тот же entry. При насыщенном `0xFF` refcount строка создаётся заново.
Функция `sub_00105E80` заменяет имя через общий string manager. Это согласуется
с многочисленными буквальными вызовами `GetName()` в диагностических строках
других engine-модулей.

| Смещение vtable | PS2 VA | Поведение |
|---:|---:|---|
| `+0x08` | `0x00105EE0` | deleting-destructor имени и базы |
| `+0x0C` | `0x00100810` | унаследованный пустой обработчик |
| `+0x10` | `0x00105FA0` | выделяет `0x14`, регистрирует clone и копирует его |
| `+0x14` | `0x00105DC0` | копирует shared name, затем подтверждает успех |
| `+0x18` | `0x00105DB0` | возвращает registration `0x004A02C0` |
| `+0x1C` | `0x00100010` | унаследованная exact-type проверка |
| `+0x20` | `0x00100050` | унаследованная base-chain проверка |
| `+0x24` | `0` | терминатор/неиспользуемое слово |

## Реализованный код

На текущем evidence-корпусе сам `spBaseObject` закрыт структурно, но ещё не
source-identical:

| Область | Статус |
|---|---|
| class/base ID, registration, source module | закрыто на PC и PS2 |
| размер, offsets, constructor/destructor | закрыто; `+0x0C` остаётся без семантического имени |
| вся vtable | границы, targets и поведение закрыты на PS2; таблица и короткие handlers независимо подтверждены на PC |
| strong-reference lifetime | механизм `uint16` retain/release/delete-at-zero закрыт, original names неизвестны |
| reverse-reference lifetime | list/node layouts, add/remove/destroy и массовые callers закрыты |
| notification protocol | record `0x20`, direct construction, forwarding и overrides закрыты; имена неизвестны |
| clone/type checks | базовые slots и manager interaction закрыты |
| XML/binary object creation | общие entry points и путь до RTTI/property dispatch закрыты; весь serializer как отдельная subsystem ещё шире этого класса |

Таким образом, оставшиеся вопросы не меняют уже доказанный размер или порядок
virtual slots. Они мешают утверждать, что восстановлена исходная декларация
буква в букву: прежде всего это `+0x0C`, имена notification/reference API и
защищённое тело одного PC slot.

- [`spBaseObject.cpp`](../../Sparkplug/Code/SparkBase/spBaseObject.cpp) — exact
  translation-unit path, переносимое поведение object/RTTI/clone/name;
- [`spBaseObject.h`](../../Sparkplug/Code/SparkBase/spBaseObject.h) — inferred
  header path; namespace и методы `vfunc_*` явно аналитические;
- [`SparkBaseAbi.h`](../../Sparkplug/Analysis/PS2/SparkBaseAbi.h) — отдельные
  byte-exact layouts базы, списка обратных ссылок, managers, property system и
  registration record с адресами evidence;
- [`PC/SparkBaseAbi.h`](../../Sparkplug/Analysis/PC/SparkBaseAbi.h) — отдельный
  PC layout и полная PC vtable, не смешанные со служебными словами PS2 ABI;
- [`spBaseObjectTests.cpp`](../../Sparkplug/Tests/spBaseObjectTests.cpp) — class
  IDs, inheritance, factory, exact/base type checks, clone и layouts;
- [`CMakeLists.txt`](../../Sparkplug/CMakeLists.txt) — изолированная сборка
  реконструкции без игровых файлов.

Переносимые классы намеренно не объявлены ABI-совместимыми с Win32/PS2: вместо
32-битных сырых адресов они используют безопасные host ownership types. Точная
ABI-модель живёт отдельно и не смешивается с кодом, который мы можем запускать и
тестировать на современной машине.

`spRTTIManager`, `spCloneManager` и `spPropertySystem` в исполняемых файлах
наследуются от `spBaseObject` и полиморфного singleton-support base. Структурная
роль последнего доказана, исходное имя — нет. Их byte-exact PS2 layouts уже
отделены в evidence-заголовок. Одноимённые переносимые классы пока остаются
behavior-facades и намеренно не выдают host-иерархию за оригинальную.

## Что ещё не закрыто

1. Имена исходного header, namespace и настоящие имена vtable-методов.
2. Роль `spBaseObject +0x0C`, исходные имена reverse-reference member и счётчика
   `+0x08`; `+0x0A/+0x0B` уже закрыты как alignment padding.
3. Исходный тип списка обратных ссылок, имя 32-байтного notification record,
   его полей и протокола, а также исходное имя доказанного singleton-support base.
4. Семантика type-specific полей `+0x30/+0x3C/+0x48/+0x4C`, исходные имена
   property types/member-function descriptor и PC tree-layout `spRTTIManager`
   после `+0x14`; PS2 callable ABI и offsets getter/setter уже доказаны.
5. Защищённое SecuROM тело PC clone-copy slot `0x0040ECE0`; полная таблица
   `spBaseObject` и короткие PC callbacks уже подтверждены через vtable и PS2-аналог.
6. Clone класса с повторяющимися ссылками или циклом, который проверит map не
   только структурно, но и поведением.
