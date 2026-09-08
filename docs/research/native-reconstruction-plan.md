# План реконструкции исходных классов Sparkplug

Текущая цель — [необходимые части классов для инструментов](tool-driven-research-scope.md).
Полное [завершение PC SMO/SAN](pc-smo-san-completion-contract.md) сохраняется
как долгосрочный контракт, а не условие выпуска приложений.
Последний ограниченный цикл завершён 8 сентября в 07:00 МСК:
[отчёт CP99–CP123](native-cycle-report-2026-09-08-0700.md),
[журнал](../../journal/2026/2026-09-08-pc-smo-san-0700.md).
PC workflow-v2: **45,62%**, 191 оценённый класс из 279; PS2: 31,16% из 245.
Семь широких engine gates: 0 passed, 6 partial, 1 open. Ниже сохранены
исторические срезы, а не новая очередь от первого класса.

Постоянные правила закреплены в [манифесте исследования](research-manifesto.md):
непрерывная оптимизация, общий бюджет RAM около 1 ГиБ, регулярные commits.
Текущий порядок задают конкретные операции софта и их недостающие контракты;
активные срезы и deferred backlog — в [native-work-items.json](../../research/native-work-items.json).
[Первый опыт ускорения после цикла](native-research-strategy-2026-09-07.md)
снял ограничение стенда для двух lit first-generation сценариев при heap
128 КиБ. Полные loader/save/ownership/visibility и PC display относятся к
широкой долгосрочной цели. Для инструмента проверяются используемые части
и фактический результат команды. Число классов и средняя оценка не задают
готовность приложений.

## Историческая последовательность реконструкции

Текущий прогресс: первый срез собран и протестирован. Подтверждены registration
record `0x60`, роли factory/property callbacks, clone slots `+0x10/+0x14`,
`spNamedObject` размером `0x14`, reverse-reference list базы, её 16-битный
reference count, общий PC/PS2 layout `spBaseObject = 0x10`, точный
notification record `0x20`, PS2 layouts двух managers и общий PC/PS2 размер
`spPropertySystem = 0x20`. В registration `+0x50` доказана встроенная
owner/count/first property group, а у записи `0x58` — name/type и parser
dispatch. Следующие два среза закрыли пустую границу `spCrossPlatform = 0x14` и
абстрактный поток `spStream = 0x1C` с девятью pure-virtual операциями.
PC leaf `spPCFileStream = 0x20` восстановлен до прямого Win32 backend.
PS2 leaf `spPS2FileStream = 0x114` закрыт как exact layout и проверяемый
state-machine slice; его shared-manager path и PC pre-open resource-resolver
оставлены отдельными доказательными зависимостями.
Переносимая реализация и отдельные ABI-layouts находятся в `Sparkplug/`;
неизвестные поля и исходные имена явно сохраняются в очереди, а не заполняются
догадками.

Общий `spEngineCore` также прошёл полный цикл текущей глубины: найден точный PC
source path, подтверждены direct base `spBaseObject`, allocations `0x158/0x150`,
разные PC/PS2 container layouts, singleton, factory/clone, 18 class-local slots
и bootstrap/teardown contract. Он вынесен в отдельный target
`SparkplugEngine`, чтобы не смешивать исходный модуль со `SparkBase`.

Следующий последовательный срез закрывает `spIndexBuffer`: общий layout
`0x28`, topology-count transforms, variable-width payload, stream grammar и
различие между blank RTTI clone и отдельным глубоким копированием подтверждены
на обеих платформах. Исходные имена enumerators/API пока не подменяются
догадками.

Для следующего `spMesh` закрыта обязательная базовая зависимость `spResource`:
это concrete storage-free слой `spNamedObject` размером `0x14`, который копирует
имя и при уничтожении снимает себя с глобального resource manager.

`spMesh` также доведён до проверяемого среза: общий exact `0x50`, две vtable,
null factory/clone, bounds state и индексный min/max pass. Неизвестный embedded
support и vertex decoding оставлены явными зависимостями следующих классов.

Следующая renderer-ветка раскрыла обязательную базу `spRenderable` и первый
concrete leaf `spModel`. У базы доказан platform split `0x58` PC / `0x50` PS2:
два одинаковых callback-контейнера имеют разные compiler ABI. Поэтому и
`spModel` расходится как observed `0x60` PC / exact `0x58` PS2. Закрыты RTTI,
material/fog state, alpha-sort/priority, base-`spMesh` relationship,
projection group и clone/copy. Native PS2 constructor исправил старую
корпусную гипотезу: отсутствующий serialized projection field нельзя считать
эффективным нулём, потому что runtime default равен `3`. Поздний разбор
`spModelSerializer` также исправил прежнее заужение этой связи до
`spMeshData`: leaf допустим, но serialized target — базовый `spMesh`.

По следующему `spMeshData` выполнена ограниченная разведка: direct base
`spMesh`, размер `0x58`, две vtable, owning `spIndexBuffer +0x50` и
`spVertexBuffer +0x54`, их deep-copy/release и повторный bounds pass совпали на
PC/PS2. Обязательный `spVertexBuffer` теперь закрыт полным циклом: общий exact
`0x5C`, 22 component offsets, bit-to-width layout, ownership, stream grammar,
blank RTTI clone и отдельный deep copy совпали на обеих платформах. Это снимает
блокировку code stage `spMeshData` без введения `void*` или мнимого geometry API.

Platform mesh branch также разделена без сглаживания ABI. Общая concrete база
`spPlatformSpecificMeshData` не добавляет storage. Для `spDXMeshData` доказаны
owning-buffer prefix, PS2 exact `0x44` и deep conversion из `spMeshData`.
Для `spPS2MeshData` закрыты PS2 exact `0x100`, PC observed extent `0x100`,
constructor tables, packet ownership, component normalization и serializer
connection. Portable slice воспроизводит только безопасное планирование:
аппаратная DMA/VIF/GIF-цепочка остаётся evidence до появления настоящего PS2
backend contract.

Следом закрыт общий `spTexture`: registration ставит его прямо под
`spNamedObject`, тогда как реальная C++ lifetime-цепочка проходит через
`spResource`. PS2 exact layout равен `0x38`, PC имеет полный observed extent
`0x38`; подтверждены secondary `spITexture`, null clone, общий Init-state и
округление размеров до степеней двойки. Portable slice останавливается перед
четырьмя аппаратными pure slots. Следующий data-узел — `spTextureData`.

Перед ним закрыта обязательная embedded-зависимость `spTextureBuffer`: общий
exact `0x30`, concrete factory, два owned pointers, blank clone, пятиаргументный
`Init` и pixel-size map `4/4/1/2/2` совпали на PC/PS2. Portable реализация
добавляет только overflow guard и не выдумывает тип auxiliary object `+0x24`.
Следующим остаётся сам `spTextureData`.

`spTextureData` также доведён до проверяемого общего среза: concrete base
`spTexture`, exact `0x4A0/0x498`, embedded buffer `+0x38`, два разных container
ABI, name-only clone и virtual CPU-buffer copy доказаны. Огромный platform
block `0x410` и serializer-record semantics не заполнены догадками. Следующий
класс выбирается по registration-порядку после этой texture-вертикали.

Статус: рабочий порядок действий после первичного разделения Sparkplug/Winx и
тестовой разведки `spBaseObject`. Это компактный план исследования, а не задача
немедленно декомпилировать весь движок.

## Основной принцип

Работа идёт снизу вверх: сначала общий object/registration contract, затем
bootstrap, после него resource/scene/render paths. Форматы SMO и база ресурсов
используются как доказательные входы, но не диктуют названия C++ API.

Исходные пути воспроизводятся там, где они найдены буквально. Для уже
подтверждённого
`Z:\Sparkplug\Code\SparkBase\spBaseObject.cpp` целевой относительный путь будет
`Sparkplug/Code/SparkBase/spBaseObject.cpp`. Если имя или место заголовка не найдено,
допускается рабочий inferred path с явной пометкой; это не выдаётся за точную
копию исходного дерева.

Для каждого класса заводится одна карточка со следующими разделами:

1. original module/source path и граница `engine/game/platform/third-party`;
2. class ID, base ID и registration objects для PC/PS2;
3. constructor/destructor candidates, размер и layout по платформам;
4. vtable slots с адресами `sub_*`, пока настоящее имя не доказано;
5. callers/callees, owner, lifetime и связь с ресурсами;
6. подтверждённые факты, рабочие гипотезы и открытые вопросы отдельно;
7. минимальные layout/ID/behavior tests для добавленной реконструкции.

Общий список незакрытых offsets, типов и platform-различий ведётся отдельно в
[`native-open-questions.md`](native-open-questions.md). При разборе нового
класса его xrefs обязательно сверяются с этим списком: неизвестное ищется по
ходу следующих вертикалей, а не забывается после смены активной задачи.

## Этап 1. Закрыть пилот `spBaseObject` — частично готово

- [x] Исправить трактовку `field_04`: это optional двусвязный список обратных
  ссылок на владельцев, лениво создаваемый и удаляемый базой.
- [x] Подтвердить `field_08` как 16-битный intrusive reference count по callers
  замены и уничтожения object-reference.
- [x] Разобрать slots `+0x14/+0x18/+0x1C/+0x20`: base clone-copy,
  registration getter, exact-type и base-chain checks.
- [x] Исправить границу vtable: `0x00104E90` относится к
  следующей таблице, а не к `spBaseObject`.
- [ ] Найти исходные имена типа списка, members `+0x04/+0x08` и операций ссылок.
- [x] Закрыть `+0x0A/+0x0B` как alignment padding по одинаковой halfword-записи
  PC/PS2 constructors и отсутствию самостоятельных accesses.
- [x] Сверить полную PC vtable и сигнатуру notification slot с PS2.
- [x] Восстановить PS2 notification record размером `0x20` и два пути его
  создания/пересылки по reverse-reference list.
- [x] Разделить XML/binary `CreateFromStream`, проследить оба пути до RTTI
  factory и общего property dispatch существующего объекта.
- [ ] Определить роль слова `+0x0C` и исходные имена notification slot/record.

Результат этапа: точный PS2 layout `0x10`, минимально понятный lifetime и
vtable contract без обязательного восстановления оригинальных имён методов.
Если имён нет, они остаются `sub_*`.

## Этап 2. Восстановить class registration и RTTI — ядро готово

Этот этап нужен до массового разбора классов: один раз поняв registration ABI,
мы перестанем заново угадывать назначение одинаковых callbacks.

- [x] Восстановить общий PC/PS2 layout registration object `0x60`.
- [x] Разделить stored factory callback и одноразовый property-registration callback.
- [x] Проследить class lookup, base-chain walk и object creation через factory.
- [x] Подтвердить property callback на `wxVectorWrapper` и 125 совпадающих
  классах PC/PS2.
- [x] Восстановить PS2 layout `spRTTIManager`: base `0x10`, второй
  полиморфный base `+0x10`, registration tree `+0x14`, общий размер `0x24`.
- [x] Подтвердить структурную роль второго base как singleton-support subobject
  у `spCloneManager`, `spRTTIManager` и `spPropertySystem`.
- [ ] Найти исходное имя singleton-support base и закрыть platform-specific PC tree-layout;
  allocation size PC `0x20` уже подтверждён независимо.
- [x] Восстановить layout `spPropertySystem = 0x20`: capacity/count/array по
  `+0x14/+0x18/+0x1C`, capacity `0x400`, property record `0x58`.
- [x] Исправить registration `+0x50`: это property group owner/count/first;
  восстановить append, индексный и наследуемый поиск по имени.
- [x] Восстановить начало property record: comparison vptr/name/type, type dispatch
  `0..10` и 12-байтный PS2 member-function ABI.
- [x] Подтвердить getter/setter по `+0x18/+0x24` и роль bit 0 `+0x14` как
  передачи имени свойства, а не переключателя прямого/виртуального ABI.
- [ ] Восстановить семантику type-specific участков `+0x30/+0x3C/+0x48/+0x4C`,
  исходные enum/type/descriptor names и PC-вариант callable ABI.
- [ ] Проверить контракт ещё на abstract/non-creatable и `wx...` ветвях.

Результат этапа: общий decoder registration records и таблица устойчивых ролей
callbacks, пригодная для всех 733 PC и 681 PS2 registrations.

## Этап 3. Проверить наследование на `spNamedObject` — пилот готов

`spNamedObject` — небольшой контрольный производный класс. Уже видно, что PS2
выделяет `0x14` байт, вызывает constructor candidate `spBaseObject`, заменяет
vtable и добавляет поле по `+0x10`.

- [x] Подтвердить `+0x10` как shared string entry: refcount `+0x08`, text `+0x09`.
- [x] Разделить inherited/overridden slots и constructor/destructor chain.
- [x] Восстановить factory, clone construction и shared-name copy.
- [x] Добавить layout test `base 0x10 -> derived 0x14`.
- [ ] Найти прямое source-path evidence для отдельного header; текущий inferred
  `spBaseObject.h` не выдаётся за оригинальный.

Формат карточки прошёл проверку на базе и первом производном классе и
принят для следующих пилотов.

### Следующий малый класс: `spCrossPlatform` — срез готов

- [x] Подтверждены одинаковые PC/PS2 class ID `0x20A72504` и base
  `spNamedObject`.
- [x] На PS2 доказано отсутствие новых полей: размер остаётся `0x14`, а данные
  производных классов начинаются с `+0x14`.
- [x] Восстановлены обе vtables, constructor/destructor chain, null-clone и
  отсутствие factory/property callback.
- [x] Добавлены переносимая реконструкция, ABI evidence, тест и отдельная
  карточка `native-class-sp-cross-platform.md`.
- [ ] Original header/translation unit и source-level abstractness остаются
  неизвестными; отдельный `spCrossPlatform.cpp` не создаётся без evidence.

### Следующий содержательный класс: `spStream` — глубокий срез готов

- [x] Подтверждены одинаковые PC/PS2 class ID `0x6CC80D8A` и base
  `spCrossPlatform`.
- [x] Доказан общий layout `0x1C`: logical origin `+0x14` и owned diagnostic
  stream name `+0x18`.
- [x] Сопоставлены все девять pure-virtual stream-операций и non-pure
  `GetBuffer()` по `spMemoryStream`, `spFileStream` и platform file streams.
- [x] Не сглажена ABI-разница двух write-overloads: stream-to-stream slot идёт
  по PC `+0x34`, но PS2 `+0x40`, а raw `WriteData` — по PC `+0x38`, но PS2
  `+0x3C`.
- [x] Восстановлены enum-значения seek `1/2/4`, typed `Read/Write`, C-string
  wire format и whole-stream helper.
- [x] Добавлены abstract portable contract, PC/PS2 ABI evidence, test-only
  concrete stream и карточка `native-class-sp-stream.md`.
- [ ] Original header/translation unit, имена двух полей, mode enum и имя
  stream-to-stream virtual slot остаются в канонической очереди.

## Этап 4. Ввести native catalog в существующую базу

Таблицу SMO `classes` расширять до runtime-каталога нельзя: serialized class и
native C++ type — связанные, но разные сущности. После двух пилотных классов
нужно добавить отдельный слой:

- executable и его SHA-256/platform;
- original module и source path;
- native type, class ID и boundary;
- inheritance edge;
- registration object/callback;
- native function с VA/RVA и временным `sub_*`;
- связь type/function/field и provenance каждого утверждения;
- отдельно original name, analytical role и confidence/status.

Первый импорт должен воспроизводимо загрузить уже полученные PC/PS2 registration
graphs. Поля/vtable `spBaseObject` и `spNamedObject` станут проверкой того, что
schema хранит не только имена классов.

## Этап 5. Превратить каркас в минимальный `SparkBase` — первый срез готов

Созданы exact translation-unit path, inferred header, отдельные ABI evidence,
CMake-сборка и автотест. Текущее дерево:

```text
Sparkplug/
  Analysis/PS2/SparkBaseAbi.h
  Code/
    SparkBase/
      spBaseObject.h
      spBaseObject.cpp
      spStream.h
      spStream.cpp
  Tests/spBaseObjectTests.cpp
  CMakeLists.txt
```

Заголовки, build project и вспомогательные файлы можно добавить под рабочими
именами, если без них нельзя собрать тест, но они получают пометку `inferred` до
обнаружения исходного пути. Первая реализация самого класса должна проверять
только подтверждённые свойства: class ID, base relationship, layout выбранного
ABI и известное поведение slots. Современные удобные wrappers располагаются
отдельно и не маскируются под исходный Sparkplug API.

## Этап 6. Восстановить границу запуска engine/game

Следующая вертикаль после базового object system:

```text
spApp -> spPCApp / spPS2App -> wxPCApp / wxPS2App
spEngineCore -> wxEngineCore
```

Оба platform edge, concrete `wxPCApp/wxPS2App`, lifecycle callbacks и точные
factory allocations уже подтверждены. Общий `spEngineCore` закрыт до exact
platform layouts, всех 18 slots и собираемого semantic slice. `wxEngineCore`
также закрыт: он не добавляет storage на PS2 и заменяет ровно один frame slot
на обеих платформах. Теперь классы manager globals внутри
initialize/update/shutdown связываются с обоими уровнями core.
Именно этот этап даст практическое разделение библиотеки движка и приложения
Winx, а не только классификацию имён.

## Этап 7. Ресурсная вертикаль

Здесь учитываются рекомендации из executable/runtime документов:

1. `spStream`/platform file stream и raw-file/PCK resolver;
2. `spSerializerManager`, registration dispatch и header/FAT boundary;
3. FAT index/file-index, точный цикл entries и relationship fixup;
4. root `spNode`, scene/partition ownership и lifetime;
5. mesh/material/texture runtime objects и GPU resources;
6. draw provenance до D3D call, затем эквивалентный PS2 VIF/GIF/GS path.

Первый end-to-end контроль остаётся `Alfea02.smo`; затем маршрут повторяется для
character SMO, SAN, STX и одного объекта, созданного из SPT/SPL.

## Короткая очередь ближайших работ

Чтобы не утонуть в полном анализе, ближайшие заходы ограничиваются пятью
результатами:

1. определить `spBaseObject +0x0A/+0x0C` и найти исходные имена уже понятных
   полей `+0x04/+0x08`;
2. найти второй base `spRTTIManager`/`spCloneManager`, закрыть PC tree-layout и
   проверить clone-graph на классе с повторными ссылками;
3. ввести schema native catalog и автоматический импорт registrations;
4. продолжать stream-ветку по доказанным зависимостям: `spMemoryStream`, общий
   `spFileStream`, `spPCFileStream` и PS2 leaf-срез `spPS2FileStream` закрыты;
   общий `spAsyncFileStreamManager`, stateless PC leaf, большой PS2 leaf и
   настоящий общий resolver `spPCKManager` закрыты; PS2-only преобразователь
   физического пути установлен как `spPS2Helper` (прежний PC-кандидат оказался
   `spPCErrorManager`); `spPS2Helper`, `spPS2IOPModuleManager` и общий `spApp` уже
   закрыты полным циклом;
5. после базовых managers перейти к bootstrap связке
   `spApp -> spPCApp/spPS2App -> wxPCApp/wxPS2App`; вся цепочка закрыта до
   безопасной собираемой реконструкции; `spEngineCore` закрыт до отдельного
   собираемого target и exact platform ABI; `wxEngineCore` также закрыт до
   единственного derived override. Общие `spError` и `spErrorManager` закрыты
   до exact platform layouts и проверяемого fixed-stack/dispatch contract.
   Platform leaf `spPCErrorManager` и `spPS2ErrorManager` тоже закрыты:
   первый даёт Win32 MessageBox/console map, второй возвращает пустой retail
   callback. Дальше очередь идёт по небольшим manager-классам, на которые
   непосредственно ссылается bootstrap.

Первым следующим common manager закрыт `spSubscriptionManager`: exact `0x20`,
двухуровневая группировка подписчиков и notification dispatch подтверждены на
обеих платформах. Следующий кандидат выбирается по соседней bootstrap-регистрации
и только если его полный evidence/code/test цикл помещается в оставшееся окно.

Следом закрыто семейство `spFontManager`: общий registry/default-font lifetime,
PC layout `0x3C`, PS2 layout `0x38` и оба platform leaf. PC leaf добавляет
отдельный backend-buffer stage после common initialization; PS2 leaf не имеет
ни storage, ни override этого этапа. Конкретные `spFont` и renderer resources
остаются следующей доказательной зависимостью, а не заменяются заглушечными
native-классами.

После font family закрыта input boundary: common `spInputManager`,
`spDXInputManager` и `spPS2InputManager`. Portable слой сохраняет lifecycle и
capacity `4/2`, а несовместимые DirectInput/PS2 device tables остаются раздельным
evidence. PS2 leaf имеет exact `0x48`; PC `0x50` пока строго называется
observed extent из полных field accesses, поскольку allocation защищён.

Следующим завершён `spDebugManager`: concrete direct-`spBaseObject` manager с
exact `0x38` на обеих платформах, singleton, 12 debug flags, циклическим
20-entry cursor и platform-разными renderer resources. Одноимённый по смыслу
`wxDebugManager` доказан как независимый game class и в этот срез не включён.

Затем закрыт переносимый ownership-срез `spEntityManager`: общий direct-base
ID, singleton, append/remove/clear/notification dispatch и blank clone
совпадают. PS2 allocation равен exact `0x20`; PC пока имеет только доказанный
prefix до `+0x1F`, так как factory защищён `.rld`. Два event-processing метода
с кодами `0x1C/0x1E` оставлены evidence-only до восстановления протокола.

Следующим отделён `spTemplateManager` от соседнего template pipeline. Manager
является компактным ref-counted registry: find/add/clear и blank clone доказаны;
PS2 exact allocation `0x24`, PC — observed prefix `0x24`. `spTemplateObject`
и `spTemplateSerializer.cpp` остаются следующей, уже существенно более крупной
зависимостью.

Перед `spTemplateObject` отдельно закрыт `spTemplateInstance`: это concrete
direct-`spNamedObject` с exact `0x28` на обеих платформах, пустым runtime-list и
создаваемым `spNode` с именем `Instance Root`. Blank-runtime clone переносит
только inherited name; relationship algorithms отложены до точного `spNode`.

Затем закрыт переносимый срез `spTemplateObject`: exact source path и `0x1B0`,
serializer-owned state/path, four-way dispatch, state-dependent cleanup и
selective clone доказаны на PC/PS2. Полный parser намеренно оставлен соседнему
реальному классу `spTemplateSerializer`, а не переименован в методы объекта.

Следом зафиксирован verified каркас `spTemplateSerializer`: exact source path,
direct-base ID, `0x1F4`, target/input ownership boundary, XML descriptor layout
и blank clone совпадают. Сложные child/property read-write routines остаются
evidence-only до полного восстановления транзакционных правил parser-а.

Следующая PS2 registration приводит к `spGameLevel`. Закрыты exact `0x2C`,
platform list ABI, ownership списка `spTemplateInstance` и name-only blank
runtime clone. Три trailing resources и serializer transaction оставлены
явными неизвестными.

Следом закрыт проверенный lifecycle/data-flow срез `spGameLevelSerializer`:
exact source path/ID/`0x184`, binary magic/header boundary, `0x168` instance
record и цепочка template resolution → instance transform → level insertion.
Полный read/write API и rollback пока не симулируются.

После renderer buffer/mesh/texture ветки восстановлена общая scene-основа
`spNode`: concrete direct-`spNamedObject`, exact `0xB4` PC / `0xC0` PS2,
local position/scale/3×3 orientation, flags, parent/child ownership, root walk,
recursive mask `0x200` и deep hierarchy clone. Runtime constructor default
`0x00070A00` отделён от форматного fallback старых SMO без animated field.
`spTemplateInstance` теперь использует настоящий `spNode`; world cache,
scene-manager link и collision vector остаются следующими доказательными
зависимостями.

Следующая связка `spLight -> spLightData` также закрыта. Abstract `spLight`
добавляет type/color/attenuation/intensity/range/angles/shadow/enabled state,
embedded scene-light support и dirty-transform update. PC имеет полный
наблюдаемый extent `0xF0`, PS2 — exact `0x100`. Concrete `spLightData` не
добавляет storage, но даёт factory/clone и связывает все девять serializer
fields с runtime offsets. Подтверждённая PS2-аномалия inherited copy — intensity
не переносится в clone — сохранена явно. Очередь продолжается отдельным
`spLightDataSerializer`, не смешивая форматную логику с data-классом.

Первый dependency этого перехода закрыт: `spSerializer` имеет точный ID
`0x42429877`, прямой registered base `spBaseObject`, null factory/clone и второй
vptr по `+0x10`. PS2 concrete factories доказывают размер `0x14`; PC пока имеет
полный observed extent. Центральные `SBOO` load/save entry points описаны, но
не подменены придуманным manager API. Дополнительная проверка constructor chain
показала, что `spLightDataSerializer` C++-уровнем проходит через
`spNodeSerializer`, хотя engine RTTI обоих serializer records указывает прямо
на `spSerializer`.

Промежуточный `spNodeSerializer` теперь восстановлен: общий ID `0x4545848A`,
concrete factory/blank clone, storage-free `0x14`, target `spNode`, девять field
IDs, relationship-finalizer и native write order/default suppression подтверждены
PC/PS2. Portable слой строит только известный план записи; stream codec,
quaternion и collision/fixup остаются evidence-only. Следующий класс —
`spLightDataSerializer`, причём его C++ inheritance и registered RTTI graph
должны остаться раздельно описанными.

`spLightDataSerializer` также закрыт в переносимой границе: ID `0x33EC2F8E`,
C++ base `spNodeSerializer`, registered base `spSerializer`, storage-free
`0x14`, concrete factory/clone, target `spLightData` и два последовательных
field plans. Подтверждены все light defaults, включая suppressed `false` у
field 8 при runtime constructor default `true`. Следующая небольшая ветка —
`spLightSerializer`, уже зарегистрированный от `spNodeSerializer`; после него
следует выбирать ближайший serializer с закрытым target data class.

`spLightSerializer` закрыт следующим: общий ID `0x06165309`, совпадающий C++ и
engine-RTTI base `spNodeSerializer`, exact PS2/observed PC `0x14`, concrete
factory/blank clone, target `spLight` и отдельные node/light write plans.
Одинаковая с `spLightDataSerializer` таблица полей сохранена в самостоятельном
классе. Следующим выбирать ближайший serializer, target которого уже
восстановлен, и не переносить stream codec до доказательства общего framing.

По этому правилу следующим закрыт `spRenderableSerializer`: общий ID
`0x4D694D82`, direct C++/registered base `spSerializer`, exact PS2/observed PC
`0x14`, concrete factory/blank clone и target `spRenderable`. Writer условно
пишет material/fog relationships, но всегда пишет alpha-sort enable/priority;
resource-index pass проходит обе связи. Следующая естественная ступень этой
ветки — `spModelSerializer`, если его наследование и собственные поля одинаково
подтвердятся на обеих платформах.

`spModelSerializer` подтверждён на обеих платформах: ID `0xDB55C34A`, direct
C++/RTTI base `spRenderableSerializer`, exact PS2/observed PC `0x14`, target
`spModel`, inherited write/index/read и два собственных поля. Mesh relationship
проверяется на `spMesh`, поэтому прежнее заужение portable `spModel` до
`spMeshData` исправлено. Projection group записывается всегда. Следующий
кандидат — serializer одной из уже закрытых resource-data веток с минимальным
собственным framing.

Следующим закрыт `spMeshDataSerializer`: exact PC source path, общий ID
`0x66380037`, direct base `spSerializer`, exact PS2/observed PC `0x14`, target
`spMeshData` и единственный cross-platform field. Его payload последовательно
содержит собственные index/vertex buffers, не relationships; native mode gate
пока сохранён численно (`0/2`), не превращён в недоказанный enum. Следующая
ступень — platform-specific mesh-data serializer, использующий уже доказанный
direct helper.

Platform-specific ступень `spPS2MeshDataSerializer` теперь тоже закрыта:
exact source path, direct `spMeshDataSerializer` base, exact PS2/observed PC
`0x14`, target `spPS2MeshData`, field order `CrossPlatform? ->
PlatformSpecific -> BoundingBox` и reader dispatch подтверждены. Разные
native-load masks PC `0x02` / PS2 `0x08` не слиты. Нативный packet codec
оставлен evidence-only; следующий кандидат — парный `spDXMeshDataSerializer`.

Парный `spDXMeshDataSerializer` также закрыт: exact source path, direct
`spMeshDataSerializer` base, exact PS2/observed PC `0x14`, target
`spDXMeshData`, поля `CrossPlatform? -> PlatformSpecific`, reader dispatch и
разные masks `0x02/0x08` подтверждены. Восстановлена проверяемая арифметика
FVF/vertex/index header; Direct3D buffer codec остаётся evidence-only.
Следующий класс выбирается из соседней texture-serializer ветки с уже
восстановленным target data class.

Первый класс соседней ветки, `spTextureDataSerializer`, закрыт в переносимой
границе: exact source path, общий ID/direct `spSerializer` base, exact
PS2/observed PC `0x14`, target `spTextureData`, поля source `2/3/4`, локальные
поля `6/0` и mode gate `0/2` подтверждены на обеих платформах. Raw payload
считает `width * height * pixelSize` и не включает depth; этот старый контракт
закреплён безопасным planner. Resolver/reference loading остаются
evidence-only. Следующая ступень — platform-specific texture serializer.

`spPS2TextureDataSerializer` закрыт следующим: exact PC source path, direct
`spTextureDataSerializer` base, exact PS2/observed PC `0x14`, target ID,
source-wrapper, порядок `PlatformType -> CrossPlatform? -> PlatformSpecific`,
значения type `8/9` и разные reader masks `0x02/0x08` подтверждены. Также
зафиксирован каркас native palette/mip payload; имена descriptor fields и
swizzle/packing остаются evidence-only. Следующий парный кандидат —
`spDXTextureDataSerializer`.

`spDXTextureDataSerializer` также закрыт в переносимой границе: exact PC source path,
direct `spTextureDataSerializer` base, exact PS2/observed PC `0x14`, target ID,
source-wrapper, порядок `PlatformType -> CrossPlatform? -> PlatformSpecific`, type
`6/7` и отдельные reader masks подтверждены обеими сборками. Native payload сохраняет
field IDs `0/1`, первый mip и повторяемые mip-записи `width/rowStride/height/raw bytes`;
безопасный planner проверяет переполнение, но Direct3D texture creation остаётся
evidence-only. Следующий кандидат выбирается среди target-классов этой пары, начиная
с `spDXTextureData`, если его layout/lifetime подтверждается без догадок.

Проверка `spDXTextureData` не нашла самостоятельной RTTI-регистрации или строки класса
ни в PC, ни в PS2: в отличие от `spDXMeshData`, пока подтверждён только target ID
serializer-а. Создание пустого portable target-класса отложено. Следующая реальная
зарегистрированная зависимость — `spMaterialSerializer`, base для трёх material-data
serializer-ов.

`spMaterialSerializer` закрыт в безопасной стандартной границе: exact PC source path,
общий Class ID/direct `spSerializer` base, PS2 exact/PC observed `0x3C`, embedded
data-block state по `+0x14`, factory/clone и standard-layer write/index/read contracts.
Порядок сверён с 105 588 объектами корпуса. Нестандартные layer classes planner явно
отклоняет; их поля остаются evidence-only. Следующий класс — concrete
`spMaterialDataSerializer`, после него парные DX/PS2 leaf при наличии времени.

`spMaterialDataSerializer` закрыт как тонкий concrete leaf: обе сборки подтверждают
Class ID/direct `spMaterialSerializer` base, target `spMaterialData`, PS2 exact/PC
observed размер `0x3C`, отсутствие дополнительных members, blank clone и null-target
guard reader-а. Собственный material codec не добавлялся: read/write используют общую
грамматику base. Следующие классы по зависимости — `spDXMaterialDataSerializer` и
`spPS2MaterialDataSerializer`; их нужно проверять раздельно, несмотря на общий base.

Платформенная пара также закрыта в узкой границе. Обе регистрации существуют в обеих
сборках, обе напрямую наследуют `spMaterialSerializer`, имеют PS2 exact/PC observed
`0x3C`, меняют только RTTI/lifetime и наследуют общий write/index/read contract. У них
нет target-hook `spMaterialDataSerializer`, поэтому platform payload или target в коде
не моделировались. Следующая зависимость по serializer-ряду — `spCameraSerializer`.

`spCameraSerializer` закрыт в безопасной wire-boundary: direct `spNodeSerializer` base,
target `spCamera`, PS2 exact/PC observed `0x14`, inherited relationship finalizer и два
field ID. Поле `Camera` всегда содержит near/far/view-angle/pixel-aspect floats; `Camera2D`
пишется только для true. Runtime viewport initialization и полный target остаются
evidence-only. Следующий leaf — `spCameraDataSerializer`.

`spCameraDataSerializer` закрыт как storage-free thin wrapper над camera serializer.
Особенно важно: PC и PS2 независимо возвращают target `spCamera` (`0x18DF3845`), не
отдельный `spCameraData` (`0x24BB4C41`). Read/write и relationship finalization полностью
делегируются base; это поведение закреплено тестом против «логичной», но неверной
подмены ID. Следующий сериализатор по текущему порядку — `spFogSerializer`.

`spFogSerializer` закрыт в полной известной wire-boundary: direct `spSerializer`
base, target `spFog`, PS2 exact/PC observed `0x14` и единственный обязательный
field `esfFog` из пяти 32-битных значений (type, ARGB, start, end, density).
Unknown fields остаются на общем skip-пути; raw fog type намеренно не превращён в
неподтверждённый полный enum. Следующий serializer по порядку регистрации —
`spMatColorControllerSerializer`.

`spMatColorControllerSerializer` закрыт на structural/wire boundary. Обе сборки
подтверждают direct `spSerializer` base, target `spMaterialColorController`,
размер `0x14` и одинаковую последовательность evaluator-ов: четыре color-helper
по offsets `+0x68/+0xB8/+0x108/+0x158`, затем functional alpha по `+0x1A8`.
Отсутствие target-объектов в PS2-корпусе не трактуется как отсутствие класса:
полные MIPS reader/writer и factory присутствуют. Следующий класс —
`spLightControllerSerializer`.

`spLightControllerSerializer` закрыт на structural/wire boundary. Обе сборки
подтверждают direct `spSerializer` base, target `spLightController`, размер `0x14`,
девять одинаково упорядоченных полей и обязательную relationship-секцию на
`spLight`. Условия подавления scalar-полей совпадают, но default ARGB различается:
PC `0xFF000000`, PS2 `0x00000000`; это сохранено как раздельное evidence, а не
замаскировано общим значением. Следующий controller/serializer-кандидат сначала
проверяется по регистрации и коду обеих платформ.

Следующим закрыт `spAnimTexControllerSerializer`: direct `spSerializer`, target
`spAnimTexController`, exact PS2/observed PC `0x14` и один обязательный внешний
field. Внутренняя грамматика подтверждена executable и корпусом: count, полный
массив float-времён, затем столько же variable-size relationships с базовым gate
`spTexture`. Старое описание `relationship<spTextureData>` уточнено: это частый
concrete inline payload, а не проверяемый serializer-ом base ID. Следующий кандидат
— `spUVControllerSerializer` после независимой PC/PS2 сверки.

`spUVControllerSerializer` закрыт на structural/wire boundary. Обе сборки
подтверждают direct `spSerializer`, target `spUVController`, размер `0x14`, один
обязательный внешний field и временный helper над embedded transform по `+0x4C`.
Вложенный порядок одинаков: семь functional evaluator-ов (translation XYZ,
scale XYZ, rotation), затем UV pivot и rotation axis. Relationship pass
отсутствует. Следующая локальная зависимость — `spTransFunctionEvalSerializer`;
математика evaluator-ов остаётся evidence-only до отдельного разбора.

`spTransFunctionEvalSerializer` затем отделён от UV-wrapper-а как самостоятельный
direct `spSerializer`: общий ID `0x2AE96657`, target `spTransFunctionEval`
`0x491432F0`, exact PS2/observed PC `0x14`. Writer создаёт field 0 wire-типа 5 и
обходит те же семь evaluator-ов и два вектора по offsets embedded target-а; pass
relationship-фиксации сразу успешен. Следующий кандидат — вызываемый им serializer
одного functional evaluator-а после независимого установления identity.

Вызванный leaf подтверждён под native-именем `spFunctionEvalSerializer`:
общий ID `0x1D2A151D`, direct `spSerializer`, target `0x9450E590`, exact
PS2/observed PC `0x14`. Поля `0..5` — FunctionType, Frequency, Amplitude,
XOffset, YOffset, Pitch; defaults `0/1/1/0/0/0`, а reader хранит reciprocal
Frequency отдельно. Семантические имена raw function types не выдумываются.
Следующим проверяется родственный color-function leaf.

Родственный leaf подтверждён как `spColorFuncEvalSerializer`: ID `0x2CC46B90`,
target `spColorFuncEval` `0x0BC70FE7`, direct `spSerializer`, exact PS2/observed
PC `0x14`. Поля `0..7` содержат два ARGB, type и пять float-параметров; default
color различается (`0xFF000000` PC, `0x00000000` PS2), остальные defaults и
reciprocal Frequency совпадают. Тем самым локальные evaluator-зависимости
material-color и UV controllers структурно замкнуты.

Следующим минимальным BV-классом закрыт `spSphereBVSerializer`: общий Class ID
`0x7294634F`, direct `spSerializer`, exact PS2/observed PC `0x14`. Position field
пишется только за epsilon `0.001`, radius — всегда; reader обновляет пары offsets
`+0x28/+0x18` и `+0x34/+0x24`. Target `spSphereBV` `0x390946D2` подтверждён
корпусом, но отдельного target-ID virtual hook в native vtable нет. Следующий
кандидат — `spBoxBVSerializer`.

`spBoxBVSerializer` закрыт следующим: Class ID `0x48E43495`, direct serializer,
exact PS2/observed PC `0x14`; position использует тот же epsilon `0.001`, full size
пишется всегда. Reader зеркалит position и вычисляет half-extents плюс bounding-
sphere radius. Target `spBoxBV` подтверждён корпусом без отдельного target hook.
Следующий BV-кандидат — `spOBBBVSerializer`.

`spOBBBVSerializer` закрыт следом: Class ID `0x68EA2ED1`, direct serializer,
exact PS2/observed PC `0x14`; поля position/full-size/rotation подтверждены на
обеих платформах. Position и identity matrix подавляются за epsilon `0.001`,
size обязателен, matrix записывается как quaternion и читается обратно в matrix.
Target `spOBBBV` `0x4DA04889` подтверждён корпусом без отдельного target hook.
Прежнее продолжение к `spConvexBVSerializer` снято с позиции автоматического
следующего шага: выбор по малому размеру оставлял центральные зависимости
resource pipeline неизвестными.

## Новый порядок: логический фронт зависимостей

С 2026-09-04 класс выбирается не по ожидаемой сложности и не по близости имени
в registration table. Очередь строится от уже подтверждённого end-to-end пути.
С текущего цикла при равной связности приоритет получает модельно-графическая
вертикаль: resource/FAT, mesh, material, texture, scene/render и platform draw.
Это приоритет выбора, а не разрешение пропускать неизвестный callee или
обязательную ownership-зависимость текущего узла:

1. выписать незакрытые callee/type/ownership границы текущего узла;
2. выбрать узел, закрытие которого снимает наибольшее число этих границ;
3. пройти его PC/PS2 identity, ABI, callers/callees, lifecycle и error paths;
4. не переключаться из-за сложности, пока не найден естественный boundary либо
   проверяемый внешний blocker;
5. только после code/evidence/test/doc цикла пересчитать фронт.

Поэтому сложный класс может идти перед лёгким, если он является join-точкой
нескольких уже разобранных веток. Первый тест этого принципа —
`spSerializerManager`: он связывает streams/PCK, serializers, FAT и root
`spNode`. Разбор обнаружил прямой platform-hook edge и уточнил ближайшую
очередь:

1. закрыть storage-free `spSerializerHook` и платформенную пару
   `spDXSerializerHook` / `spPS2SerializerHook`, потому что generic load-path
   вызывает этот interface непосредственно;
2. затем перейти к принадлежащему manager-у FAT helper-у: он остаётся более
   крупной join-точкой object IDs, names, offsets, serializers и resource
   manager;
3. из FAT идти в `spResourceManager` и relationship fixup, а не обратно в
   независимые leaf serializers.

Hook оказался маленьким не потому, что очередь снова сортируется по простоте,
а потому, что это ближайшее прямое ребро текущего call graph. Отдельный
code/evidence/test/doc цикл теперь завершён: общий abstract interface и PS2
leaf перенесены, exact PS2 body закреплён, а PC DX body честно остановлен на
SecuROM thunk. Следующий цикл закрыл основной FAT index/lookup/cursor/clear и
object-index срез. Неизвестные PC body, original names и fixup по-прежнему
остаются в очереди, а не объявляются несуществующими.

## Новая граница оценки и SMO-first приоритет

С 2026-09-05 прогресс executable-реконструкции больше не выражается одним
процентом. Registration graph задаёт стабильные знаменатели, но само знание
имени, class ID и direct base не считается знанием логики:

- объединение PC/PS2 содержит 784 уникальных зарегистрированных типа;
- 373 типа принадлежат движку `sp...`, 411 — игровому слою `wx...`;
- прямой asset-scope содержит 36 типов, реально встреченных в SMO, и
  `spAnimation`, реально встреченный в SAN: всего 37 типов.

Для оценки логики каждого scope используются пять составляющих: data/wire
contract — 25%, layout/lifetime/ownership — 20%, runtime callers/callees — 30%,
platform/backend differences — 15%, error/rollback paths — 10%. Неизвестная
составляющая получает ноль; частично доказанная — только подтверждённую долю.
Поэтому процент является консервативным coverage score, а не долей строк
декомпиляции. Его нужно публиковать вместе со знаменателем и диапазоном
погрешности.

Базовый снимок перед первым DB-backed циклом:

| Scope | Центральная оценка | Рабочий диапазон | Проверяемая опора |
|---|---:|---:|---|
| Весь executable, engine + game | 13% | 11–15% | взвешено по 373 engine и 411 game types из общего каталога 784 |
| Логика Sparkplug | 24% | 21–27% | 373 `sp...`; 162 затронуты native-карточками, 128 имеют reconstructed declarations |
| Логика Winx | 2.5% | 1.5–3.5% | 411 `wx...`; глубоко затронуты только bootstrap/app, asset и отдельные Bloom/debug paths |
| Нативная логика прямых SMO/SAN-классов | 43% | 38–48% | 37 типов; 16 SMO-типов уже затронуты native-карточками, восемь имеют собственный portable runtime-класс |

Отдельно сохраняется форматная метрика: наблюдаемый wire layout имеет полный
strict read-only decoder для 36/36 SMO-классов, а PC SAN `spAnimation` существенно
разобран; с поправкой на незакрытый PS2 SAN это около 99% наблюдаемой структуры.
Эти 99% нельзя подставлять вместо 43% нативной логики: writer, lifetime,
runtime binding, platform materialization и error paths ещё не равны parser-у.

Очередь теперь выбирается по `SMO/SAN occurrence × importer/exporter impact ×
dependency centrality`, а прежнее общее предпочтение любой графики используется
только как tie-breaker. Текущий приоритет:

1. `spSkin` и связка `spAnimation`/controller/track/node binding;
2. runtime `spUVController`, `spAnimTexController` и
   `spMaterialColorController` вместе с реально вызываемыми evaluator-ами;
3. остатки `spModel`/`spMeshData`/`spMaterialData`/`spTextureData`, включая
   material-cache consumer, native writer/materialization и platform payload;
4. `spStaticRenderObject`, потому что это 20 469 PC-объектов уровня и критичный
   путь Level Creator;
5. `spParticleSystem`, `spSkyBox`, `spLensFlare`, затем text/font SMO-ветвь;
6. прямые level-SMO spatial/collision классы: partition, octree, BSP,
   zone/portal, occlusion, navigation и BV.

Это не запрещает выходить за 37 типов. Serializer, manager, base class или
platform leaf, без которого текущий SMO/SAN-тип нельзя честно закрыть,
исследуется как обязательная зависимость. Но независимый класс вне этого
closure больше не обгоняет SMO/SAN-узел только потому, что он проще или относится
к графике вообще.

### PC-first цикл 2026-09-05

Прогресс и evidence теперь хранятся рядом с corpus index в schema v5, а
очередной процент читается из `latest_native_coverage`. EXE сканируется только
явной bootstrap-командой `native_knowledge.py sync`; обычные incremental
manifest и `report` работают по базе. Это отделяет уже подтверждённое знание от
дорогой повторной разведки и сохраняет provenance каждого изменения оценки.

За первый цикл закрыты два прямых узла текущего asset closure:

- `spSkin` поднят с 30% до 75% после PC layout/lifetime/copy, renderer palette
  и полного `spSkinSerializer` read/write/index; проверка 21/21;
- `spAnimation` поднят с 55% до 68% после PC track/tag ownership и lifetime;
  его serializer выделен отдельно на 65%, текущая проверка 27/27. Выявлено, что
  текущий SAN decoder покрывает наблюдаемые fields `0..4`, тогда как native
  reader/writer имеет также `5..12` и control field `64`.

PS2 для обоих узлов оставлен `deferred`: PC-код не создал блокера, который
требовал бы платформенного сравнения. Следующая логическая граница не меняется
случайно — это `spController`/`spSubController`, evaluator tick и его выход в
`spNode` transform, затем проверка заполнения уже восстановленной skin palette.

Исторический снимок первого PC-first цикла после четырёх incremental manifests
(последующий часовой цикл описан в конце документа):

| Scope | Оценка | Диапазон | Числитель / знаменатель |
|---|---:|---:|---:|
| Весь executable | 13,10% | 11,09–15,06% | 102,705 / 784 |
| Логика Sparkplug | 24,78% | 21,78–27,78% | 92,430 / 373 |
| Логика Winx | 2,50% | 1,50–3,50% | 10,275 / 411 |
| Прямые SMO/SAN-классы | 44,54% | 39,57–49,57% | 16,480 / 37 |

В оставшееся время dependency scout добавил `spController` и
`spSubController` по 25% каждый. Они не входят в прямой знаменатель SMO/SAN,
поэтому меняют общую и engine-оценки, но не искусственно повышают 44,54%.

## Критерий готовности одного класса

Класс можно переносить из research-only в реконструкцию, когда:

- доказаны original class name и module либо явно записано, что module unknown;
- class/base IDs проверены хотя бы на одной сборке;
- layout и размер имеют воспроизводимый источник;
- функции без оригинального имени сохраняют `sub_*` или отдельный analytical
  alias;
- platform differences не смешаны в один псевдоуниверсальный ABI;
- есть маленький автоматический тест подтверждённых свойств.

Это критерий качества, а не требование полностью понять класс: неизвестные поля
и методы допустимы и остаются явно неизвестными.

## Оценка двухчасового пилота логического фронта

Пилот начат от уже связанных `spStream`/PCK, `spSerializer`, concrete
serializers и `spNode`, поэтому первым взят не очередной короткий leaf, а
join-узел `spSerializerManager`. За один непрерывный цикл получены:

- переносимый класс с подтверждёнными RTTI/factory/clone/singleton,
  registration ownership/order, mask dispatch и безопасным header front-end;
- byte-exact общий `0x2C` ABI, `0x18` registration node и `0x1C` file header;
- карта двух PS2 load paths до FAT materialization и уточнение границы
  manager-header против `ObjectCount`;
- PS2 FAT helper `0x64`, resource entry `0x24`, file entry `0x0C` и буквальные
  имена семи полей из diagnostics;
- исправление двух прежних ложных связей: object-header reader не ищет
  serializer manager, а post-load helper переносит FAT name в `spNamedObject`;
- новый прямой edge `spSerializerHook -> spDXSerializerHook /
  spPS2SerializerHook`, подтверждённый независимыми PC/PS2 RTTI records;
- read-only regression scanner, расширенный с 58 до 67 проверок неизменности бинарных опор, и
  CTest-проверки переносимого поведения.

Финальная проверка дополнительно исправила собственную промежуточную гипотезу:
manager `+0x18` читается как минимум тремя именованными mesh writer-ами и ещё
пятью writer-site-ами. Значения `0/2` разрешают optional/default fields; теперь
неизвестны только исходное имя политики и семантика остальных значений.

По результату стратегия эффективнее прежней для цели «уменьшать неизвестный
граф», хотя число быстро закрытых классов за единицу времени ниже. Работа над
одним join-узлом одновременно уточнила manager, serializer, FAT, hook и
resource-load документацию; ни одно боковое исследование не было выбрано лишь
из-за малого размера. Главный недостаток — central nodes требуют дольше и
часто заканчиваются доказанной границей, а не полностью компилируемым
end-to-end loader-ом.

Решение после пилота: оставить логический фронт основным порядком. Для каждого
следующего этапа отдельно считать (1) закрытые прямые edges, (2) исправленные
ложные edges, (3) оставшиеся opaque boundaries и (4) автоматизированные
binary checks. Hook cluster, основной FAT helper-срез и `spResourceManager`
cache-hit уже пройдены. Ближайшая очередь: cache-miss materialization,
рекурсивное разрешение relationship, затем payload save; при равных развилках выбирается путь, который
раньше приводит к native mesh/material/texture import/export и renderer.
Сортировка по предполагаемой сложности больше не используется.

Первый графический шаг нового приоритета закрыл PC-only
`spDXVertexBuffer`/`spDXIndexBuffer`. Они отделены от общих CPU-буферов,
получили exact `0x20/0x1C` ABI, RTTI/factory/blank clone, D3D9 create/lifetime
contract и безопасный portable storage. Все прямые callers подтверждают пять
allocation sites каждого wrapper-а; combiner создаёт их с `usage=8`, `pool=1`
и `D3DFMT_INDEX16`. Следующая join-точка не выбирается заново по простоте:
это непосредственно потребляющий их `spDXMeshCombiner`, затем `spDXMesh`.

`spDXMeshCombiner` затем закрыт без смены ветки: exact `0x2C`, один virtual
destructor, два shared dynamic buffers, lock cursors, четыре commit delta,
exact-full unlock и temporary global `0x00763148`. Portable helper добавляет
только безопасную capacity/overflow проверку. Следующий узел остаётся
`spDXMesh`, потому что именно он читает cursors/счётчики комбайнера и превращает
одну часть общей партии в runtime render mesh.

`spDXMesh` закрыт до следующей естественной границы вместе с обнаруженной
веткой `spDXSharedMeshData`. Для runtime mesh подтверждены PC-only identity,
observed prefix `0x88`, общие metadata offsets `spMesh +0x44/+0x48/+0x4C`,
standalone CPU/GPU и combined paths, shared ranges, packed-u8-to-float expansion
и полный component-to-D3D9-FVF mapping. Для shared pair подтверждены ownership,
observed `0x1C` prefix и wire serializer с порядком sizes/index/vertex. Portable
код дополнительно проверяет capacities, overflow и truncated stream. Следующая
join-точка — serializer/materializer самого `spDXMesh`; неизвестный renderer
mapping за `0x004AE0E0` не замаскирован выдуманным handle.

`spDXMeshSerializer` закрыт следующим прямым ребром. Подтверждены PC-only ID,
direct `spSerializer`, target `spDXMesh`, observed `0x14`, обе vtable и полный
wire-порядок: `u8` topology, components, FVF, relationship
`spDXSharedMeshData`, пять range/stride слов, center и radius. Portable codec
оставляет common relationship framing внешним, но даёт проверенный scalar
round-trip и materialization через shared init. Анализ writer-а открыл следующую
обязательную optimizer-границу: `0x004BEDF0` выбирает сериализуемый общий buffer,
а `0x004C07E0` вычисляет его пять range-параметров. Дополнительная RTTI-сверка
установила точный source type: это `spDXCombinedVB` (`0x4B18E622`), который
`spDXSharedMeshDataSerializer` remap-ит в load-side `spDXSharedMeshData`.
`spDXCombinedVB` перенесён отдельным узким срезом с observed `0x3C`, raw payload,
container ownership и blank clone. Следующим узлом становится владеющий им
`spDXSceneGraphOptimizer`, а не ещё один независимый serializer.

## Графический цикл до 10:00 МСК 5 сентября 2026 года

Новый приоритет не меняет логический порядок: serializer/resource/scene
dependencies разбираются, если без них нельзя честно дойти до model/render
пути. Первый переход от `spDXCombinedVB` выявил общий
`spSceneGraphOptimizer` (`0x4FE639C2`) и его обязательный runtime dependency
`spRenderNode` (`0x603625D0`).

Для `spRenderNode` подтверждены direct `spNode` base, renderable ownership,
clone/lifetime, PC observed vector prefix `0xC8`, PS2 exact allocation
`0x1E0`, два platform-specific vtable набора, bounds/update/render boundaries
и проход optimizer-а по `spModel`. Portable owner, ABI asserts и отдельный
read-only scanner добавлены; scanner фиксирует 36 независимых PC/PS2 опор.

Для общего optimizer уже доказаны PC 10-slot primary vtable, отдельный
singleton destructor subobject, start/node/model/end callback protocol и
рекурсивный scene traversal. PC-only `spDXSceneGraphOptimizer` имеет отдельные
primary/callback/singleton vtables и concrete factory; PS2 leaf отсутствует.
Следующий этап — перенести подтверждённый общий traversal, затем отделить
известную DX grouping/materialization от всё ещё opaque
`0x004BEDF0/0x004C07E0`. После этого очередь возвращается к
`spRenderNodeSerializer`, чтобы runtime scene ownership соединился с нативным
SMO read/write путём.

`spRenderNodeSerializer` закрыт следующим прямым ребром: exact PC source path,
общий ID/direct `spNodeSerializer`, target `spRenderNode`, factory/clone,
storage-free `0x14`, read/index/write и единственное повторяемое relationship
поле `0` подтверждены обеими платформами. Reader требует `spRenderable` и
запрещает null; writer сначала сохраняет `spNode`, затем каждую связь с native
data-block code `7`. Portable plan и attach seam не маскируют оставшийся общий
relationship/fixup protocol. Следующий приоритет снова смещается к DX optimizer
grouping и downstream renderer consumer.

Повторный проход DX optimizer не стал подменять защищённые helper-ы гипотезой.
Он зафиксировал concrete observed prefix `0x54` с batch list и group tree/map,
выделил пятый callback `0x004C0100`, который передаёт `spModel::baseMesh`
защищённому add-mesh entry, и доказал разделение ролей optimizer lookup,
combined-VB range lookup и `CombineData`. Portable combined-VB получил
проверяемую five-word association table; mesh writer теперь использует именно
её, как native `0x004B1B20`, и отклоняет отсутствующие/выходящие за payload
range. Следующий front смещён к `spRenderer`/platform renderer, не объявляя
SecuROM VM-body восстановленным.

`spRenderer` затем закрыт до следующей честной backend-границы. На PC доказана
иерархия `spCrossPlatform -> spRenderer -> spDXRenderer -> spPCRenderer`, на
PS2 — `spCrossPlatform -> spRenderer -> spPS2Renderer`; общие/DX registration
абстрактны, platform leaves concrete. Точные common размеры равны
`0xCA08/0xCBB0`, concrete — `0xF368/0x19D00`; одинаковый gate находится по
`+0xC050`, а platform-разные кэши содержат `12+72` и `12+96` слов. Отдельный
интерфейс по `+0x18` имеет 29 операций: PC leaf table зафиксирована целиком,
PS2 независимо подтверждает все 29 `this-0x18` thunk/body пар. Исправлена
старая упрощённая запись о D3D device: полный адрес `+0xC9E8` получается из
interface-relative `+0xC9D0` и лежит внутри exact common extent, а не начинает
DX-derived storage. Portable family, platform ABI, CTest и read-only scanner
50/50 готовы. Следующий front — именование resource/draw/reset slots от их
callers и D3D9/PS2 endpoints; неизвестные 29 имён не подменены догадками.

Следующая связная ветвь закрыла `spRenderTarget`, `spCubeRenderTarget` и
`spRenderTargetManager`. Подтверждены три несимметричные PC/PS2 RTTI-цепочки,
общий target prefix `0x28`, cube prefix `0x2C`, обычный PC surface и шесть cube
faces, exact PS2 allocations `0x30/0x2C/0x44`, а также разные допустимые
`eTBPixelFormat` для DX ordinary, DX cube и PS2 ordinary. PC renderer reset
реально обрамляет device reset вызовами manager release/reinit; manager хранит
ordinary, cube и layer targets в трёх отдельных контейнерах и деактивирует
узлы без их немедленного удаления. Portable family, ABI, CTest и read-only
scanner 43/43 готовы. Следующий front проходит через неизвестный третий
`LayerRenderTarget` список к backing texture/material pass и renderer slots
0/1; это уменьшает разрыв между сериализованной моделью и фактическим draw.

Третий список после этого раскрыт через точную иерархию `spMaterialTexture ->
spMaterialRenderTargetTexture -> spMaterialCameraViewTexture /
spMaterialCubeMapTexture`. Зафиксированы platform layouts `0x6C/0x80`, common
target extents `0x8C/0xA0`, concrete PS2 allocations `0xB0`, target/fallback
selector, recursion guard, camera/cubemap state и reset loops. Параллельно сняты
все редкие ветви `spMaterialSerializer`: шесть layer class ID и fields `7`,
`13..16`, включая неочевидную принадлежность dynamic cubemap классу
`spMirrorLayer`, а не `spCubeEnvMapLayer`. Portable family, расширенный planner,
ABI, CTest и scanner 56/56 готовы. Следующий графический front — конкретные
camera/scene/renderer calls этих двух render methods и slots 0/1 renderer-а.

Следующий front закрыл runtime `spCamera` и его выход в renderer. Подтверждены
abstract common ID, concrete `spCameraData`/`spDXCamera`/`spPS2Camera`, PC
observed `0x238` и PS2 exact `0x250/0x340`, defaults, dirty masks, viewport
ratio, две projection-формулы и шесть frustum planes. Camera application
сопоставила renderer slots `12/13/14` с projection/view/world matrices, slot
`22` с viewport, а PC bodies — с точными D3D9 `SetTransform`/`SetViewport`
endpoints. Material render paths доказали платформенную перестановку первых
двух operations: PC cube/ordinary — `0/1`, PS2 ordinary/unsupported cube —
`0/1`. Slots `3/4/5` подтверждены как begin/end/clear. Portable mapping явно
аналитический: исходные C++ имена не выдуманы. Camera scanner даёт `37/37`,
расширенный renderer scanner — `78/78`.

Следующая логическая точка выбирается не заново: camera уже вызывает scene
traversal, а renderer уже имеет mesh submission endpoint. Поэтому фронт идёт в
`spRenderNode`/`spRenderable` draw и culling callbacks, чтобы связать
`spModel::baseMesh` с установленными camera matrices и фактическим draw.

Этот разрыв закрыт на обеих платформах. PS2 `spRenderNode::0x001AA810`
проверяет mask `0x200`, выполняет sphere/frustum-plane culling и вызывает
чистый render-slot `spRenderable`; `spModel` заменяет его телами
`0x00479DC0/0x0015A640`. Оба тела выполняют pre-render, передают `baseMesh` в
renderer interface slot `9`, затем выполняют post-render. PC slot
`0x004BC670` распаковывает `spDXMesh` и достигает SecuROM bridge перед открытым
D3D wrapper; PS2 `0x001FF6A0` напрямую выполняет VIF/DMA/GS-подготовку и
buffer binding. Новый scanner фиксирует 37/37 опор, portable mapping и ABI
tests добавлены.

Одновременно исправлена ложная симметрия: classifier runtime mode `0..8`
существует в PS2 body `0x00159F80`, но соответствующий PC vtable slot указывает
на no-op `0x0048EAA0`; соседний PC `0x00479B00` является debug/dump path.
Следующая обязательная join-точка — `spRenderable` pre/post material/fog state,
далее material pass/layer dispatch к уже доказанному mesh submission. Spatial
и gameplay callees остаются в очереди, но не вытесняют этот графический front.

Join-точка material/fog закрыта без расширения догадками. Общий `spMaterial`
имеет exact PS2/observed PC extent `0x80`, 11 render states, восемь pass slots,
два flags и color-controller relationship; concrete `spMaterialData` добавляет
четыре RGBA-вектора и specular power (`0xD0` PS2, `0xC4` PC). Его native clone
намеренно оставляет default payload из-за success-only copy stub. `spFog`
закрыт полным `0x28` layout/defaults и тем же blank-clone поведением. Renderer
slot `26` доказан call sites и endpoints: PC переводит type `0..3` в
disabled/exp/exp2/linear D3D states, PS2 сохраняет fog и linear-path flag.
Исправлена ловушка PS2 ABI: caller offset `+0x70` включает два header words и
соответствует slot `26`, а не shutdown slot `28`. Portable classes, platform
ABI, CTest и read-only scanner 42/42 готовы. Следующая связная зависимость —
`spMaterialPassLayer`, затем `spMaterialTextureLayer` и `spStdLayer`.

Цепочка pass/layer закрыта следующим узлом. `spMaterialPassLayer` напрямую
наследует `spBaseObject`, имеет общий extent `0x38`, raw final-blend operation,
count и восемь owned layer relationships; native copy действительно deep-clone-ит
их через manager. `spMaterialTextureLayer` — direct-base оболочка `0x14` с одним
owned `spMaterialTexture`, а derived `spStdLayer` не добавляет storage и создаёт
обычный nested texture (`0x6C` PC / `0x80` PS2). Все PC factories являются
точными SecuROM thunk-ами, поэтому их размеры остаются observed; PS2 allocations
exact. Portable RTTI/ownership/deep clone, ABI structs, CTest и read-only scanner
45/45 готовы. Следующий графический front — consumer pass-графа: применение
material/texture states и непосредственный переход к renderer mesh submission.

Переход к PS2 mesh submission закрыт конкретным runtime-листом `spPS2Mesh`.
Подтверждены PS2-only ID/base, exact `0x58`, две vtable, owned
`spPS2MeshData*` и непрозрачный packet-emitter helper, conversion/attach/release,
намеренно blank clone и renderer draw-preparation consumer. Связка с packet
builder дополнительно назвала `spPS2MeshData +0x30/+0x34` как emitted vertex и
primitive counts. Portable ownership/count seam, ABI/tests и read-only scanner
24/24 готовы. Следующий графический front — material/backend consumer; helper
и его PS2 DMA/VIF/GIF API остаются обязательной неизвестной зависимостью, а не
заменяются придуманным интерфейсом.

Material/backend front продолжен PS2-only листом `spPS2Material`: подтверждены
ID/direct `spMaterial` base, exact `0xD0`, обе vtable, четыре RGBA-блока,
specular power, полноценный common+tail copy/clone и update, который при смене
renderer generation обновляет controller и проходит material passes. Portable
класс, PS2 ABI, CTest и scanner 31/31 готовы. Native factory не пишет `+0xC0`,
поэтому host-side zero initialization явно отмечена как safety divergence.
Join-точка `0x001700B0` доведена через layer relationship до
`spMaterialTexture::Update` и общего renderer slot `23`: подтверждены stage
selection, animation/static UV и преобразование 3×3 matrix в PS2 backend 4×4.
PC `0x004BB590` независимо связывает тот же slot с
`D3DTS_TEXTURE0 + stage`/`IDirect3DDevice9::SetTransform`.
Остальные texture/render-state operations и serializer selection остаются
соседними обязательными неизвестными ветвями.

Следующий короткий pass уточнил pre/post-render protocol без нового класса.
PS2 `spRenderable::pre` пишет material/fallback pointer и связанный float в
renderer current-material cache `+0xC164/+0xC16C`, вызывает fog slot `26` и при
material flag `+0x74` временно сохраняет/обнуляет renderer byte `+0xC19C`;
post восстанавливает его. Значит, material применяется отложенно внутри draw,
а следующая точная граница — consumer current-material cache в mesh submission.

### Завершение PC-first цикла 2026-09-05

После `spSkin` и `spAnimation` исследована их ближайшая PC runtime-зависимость
[`spTransformTrackEval`](native-class-sp-transform-track-eval.md): подтверждены
factory allocation `0x78`, binding slot `+0x10`, два blend input по `0x30` и
полное тело PRS evaluator `0x005FEBB0`; read-only проверка проходит 14/14.
Следующий связный фронт — caller/scheduler active evaluator tick, затем перенос
локального PRS в `spNode` и проверка заполнения skin palette. PS2 остаётся
отложен до реальной PC-неоднозначности.

DB-снимок предыдущего PC-first цикла: executable 13,10%, Sparkplug 24,78%, Winx 2,50%, прямые
SMO/SAN-классы 44,54%.

### Часовой PC animation runtime-цикл 2026-09-05

Следующий этап выполнил именно намеченную зависимость, без поиска случайного
простого класса: `spActor` tick/binder → `spNodeController` →
`spTransformTrackEval` → local node PRS. Добавлены portable вычислительные
срезы и isolated native checks. Исправлена граница evaluator vtable (9 slots),
установлены input key caches/time/priority, node cached-world fields и точный
порядок `inverseBind * boneWorld`. Методика, адреса и ограничения:
[PC animation runtime](native-pc-animation-runtime.md).

Следующий порядок работы:

1. Protected preamble `spNode::0x00421420` закрыт продолжением, как и
   quaternion setter/affine builder; portable tree/billboard и 179 guest checks
   готовы. Caller перед skin palette остаётся частью frame integration.
2. Representations `1..4`, preparation, shared-array hints и reader-to-sampler
   закрыты [SAN keys checkpoint](native-pc-animation-keys.md). Следующий шаг —
   animation/track constructor, ownership, tags и partial-read rollback;
   allocator padding при native next-slot reads остаётся неизвестным.
3. Довести frame caller, callbacks и input ownership/capacity `spActor`.
4. Проверить связанную цепочку на контролируемом игровом запуске с ограничениями
   времени/ресурсов; isolated replay не заменяет такую проверку.

PS2 не требуется для полученных доказательств и остаётся второй очередью.
Точные текущие четыре оценки хранятся в базе, итоговая запись —
[журнал часового цикла](../../journal/2026/2026-09-05-pc-animation-runtime-cycle.md).

Продолжение до 21:00 МСК фиксируется отдельным
[журналом](../../journal/2026/2026-09-05-pc-reconstruction-until-2100.md).
Первый checkpoint — [PC node world](native-pc-node-world.md), второй —
[PC SAN keys](native-pc-animation-keys.md). Следующий активный участок —
animation object/track/tag lifecycle был закрыт третьим
[checkpoint](native-pc-animation-lifecycle.md): три portable original classes,
206 guest checks и уточнение physical named base. Четвёртый
[checkpoint](native-pc-san-reader.md) закрыл full field reader, normal tag names,
header/payload failure distinction и переносимый original serializer: четыре SAN,
2832 native/portable comparisons. Теперь front — actor/frame и name registry,
затем full resource-loader/writer integration. Опасные ownership/resize contracts
сохранены как явные неизвестные upstream invariants.

Финал цикла: [actor playback](native-pc-actor-playback.md) — original tick и
portable scheduler совпали на 93 сценариях, empty lifecycle проверен, ближайший
caller установлен как `spAnimationManager::0x004535A0`. Работа остановлена по
просьбе пользователя после перебоя связи. Следующий цикл начинать с manager/frame,
name registry и существующих input-capacity/lifetime вопросов, не с новой случайной leaf.

Ночной цикл до 10:00 МСК 2026-09-06:
[журнал](../../journal/2026/2026-09-06-pc-reconstruction-until-1000.md).
Первый [manager checkpoint](native-class-sp-animation-manager.md) исполнил и
перенёс registry/frame/lifetime, controller copy и fresh-runtime actor clone;
real SAN reader использует shared registry в guest integration tests.
Далее идти к actor input binding/priority/capacity, tree/nonempty teardown,
затем наружу к engine frame/resource/render caller. Portable SAN binding
ownership — отдельная незавершённая часть, non-owning lookup не подменять
удерживающим BindName без освобождения. PS2 и игровой запуск не нужны текущему
изолированному доказательству и сейчас не запускаются.

Второй [input/start checkpoint](native-pc-actor-binding.md) исполнил discovery,
binder, Start/restart и nonempty teardown, перенёс evaluator insertion с156
differential cases. Далее complete descendant wrapper/Stop, portable registry
lease и actor owned input/start; затем outer frame. Прямой third blended input
не запускать: локальный Start guard до binder CALL не найден.

Третий checkpoint добавил original descendant wrapper/Stop и portable owned SAN
name leases. Четвёртый [owned runtime](native-pc-actor-owned-runtime.md) перенёс
actor tree/Start/Rebind/Stop/StopAll, подключил owned Tick к тем же evaluator/keys/
node classes: 11 CTest suites, 81 owned-actor assertions, 5154 сквозных comparisons.
PC node startup global matrix найден и инициализируется original6D38E0.
Это закрывает portable binding пункт выше, но не upstream animation lifetime/
third-input contract. Следующие связные шаги — remaining public controls,
outer frame41CD50/owner3C и затем resource/render; native event dispatcher/reentry
и full FFPS/FAT loader/writer не подменяются имеющимися seams.

Checkpoint5: [PC app/engine frame](native-pc-engine-frame.md) и
[`spTaskTimer`](native-class-sp-task-timer.md). Actual app/update/animation
chain134 checks, isolated graphics209, timer34 native/30 C++/1968 bit-exact
comparisons, CTest12/12. Outer animation caller закрыт как executable edge;
portable app/core ещё требует подключения, native timer child links и full
engine ctor остаются открыты. Далее логично идти от scene manager45A7D0
к world-update и palette/render consumers, параллельно закрывая необходимые
frame/event зависимости, не расширяя direct SMO/SAN37 искусственно.

Checkpoint6: [scene registration/owned world frame](native-pc-scene-world.md).
Native scenes и node attachment исполняются вместе с app/timers/SAN actor;
source camera ABI offsets исправлены фактическими аргументами renderer.
Далее **по этой же зависимости**: typed scene registration (RenderNode/Light/
Partition/Occlusion), четыре scene-owned manager runtime boundaries,
portable Scene/SceneManager и camera→scene culling/draw. Не пропускать эти
side effects ради упрощённого Attach и не объявлять existing plain host Node
полноценным native scene graph. PS2 остаётся вторичным фронтом.

Checkpoint7: [PC render-node runtime](native-pc-render-node-runtime.md).
Native ctor/registration55, world/bounds/cull/draw54, callback15, static21;
corrected PC1D4 и primary14/support6, portable geometry math1504 comparisons,
CTest13/13. RenderNode branch scene attachment закрыт вместе с исходными
списками; class runtime/source integration ещё не объявлен готовым. Следом
specialized45A810/45A8C0 и Light/Partition/Occlusion: они нужны той же сцене.
Не забывать unknown projection ID58DA4026, light-cache type, original clone/cache
и переходы renderer456310/scene45EC70; geometry getters и portable virtual
world-dispatch требуют подключения без потери native side effects.

Checkpoint8: [`spLightManager`/PC light runtime](native-class-sp-light-manager.md).
Original Light branch registration/reparent, world refresh и fixed cache
закрыты; portable list/selection source добавлен, Node100 helper перенесён.
Exact PC LightDataF0 и protected intensity-copy независимо подтверждены;
старые unknown allocation/copy пункты больше не актуальны. Native90/static22,
portable29/differential1800, CTest14/14. Далее specialized45A810/45A8C0
SkyBox/LensFlare/Projection, затем SceneInit/Partition/Occlusion и portable
Scene wiring. Literal partition recursion — не реконструкция concrete payload
класса; original types и property invalidation должны быть установлены отдельно.

Checkpoint9: [PC specialized scene managers](native-pc-scene-special-managers.md).
Registration81, Sky runtime39, Projection/Flare43, static25; exact ABI и CTest14.
Sky follows actual DefaultCamera parent из game blocks, сохраняет local
orientation и отдельно очищает model fog перед base draw. Три разных списка
не сводить к одному контракту. Следующие приоритеты: portable RenderNode/derived
virtual world/geometry getters и Scene wiring; одновременно обязательная
SceneInit→Partition/Occlusion граница и настоящие Projection/LensFlare consumers.
Оригинальные недостающие классы не заменяются successes от recording seams.

Checkpoint10: [Model→RenderNode source/world](native-pc-model-render-world.md).
Native Model47/clone ownership29/static23, portable34,2272 comparisons/32
сценария и14 CTest. Virtual world/getters/lazy caches подключены; source
duplicate-alias clone исправлен actual always-clone contract. Дальше по
той же цепочке: nonempty Renderable pre/post callbacks и renderer alpha/
model queues; derived Sky/Light/Camera world source и automatic Scene wiring.
Обязательные SceneInit→Partition/Occlusion не откладываются навсегда за seams.
PC cold MeshData teardown не равен полной Init/materialization; game/PS2
и проценты не расширяются за счёт повторного учёта старых source/cards.

Checkpoint11: [PC renderer protocol](native-pc-renderer-protocol.md). Native
callbacks75/queues64/static28, source23/differential768/CTest15. Alpha and
normal queue contracts теперь исполнены, typed PC passes original no-op.
Дальше scene45EC70 nonpartition path и обязательный SceneInit45D850→concrete
Partition/Occlusion; derived Sky/Light/Camera world source. Full renderer ctor
scout достиг100k/2s cap, не повышать лимит ради whole-object claim; изучать
необходимые protected fragments и runtime global75F8E8 по callers.

Checkpoint12: [PC partition/static runtime](native-pc-partition-runtime.md).
Exact six class identities/ABI, spatial38/render38/static30 и CTest15.
Закрыты concrete counterpart RenderNode callbacks, разные виды владения,
System physical-vs-RTTI inheritance, static matrix submission и shared math
startup. Далее **по тому же графу**: VisibilityManager ctor/container/portal
queries, SceneInit/render и portable spatial source; full static serializer
transaction связывает уже известные SMO fields с native render boundary.
Не повышать bounded cap ради whole ctor и не заменять неизвестные зависимости
положительными fixture results. Root raw reset требует снятых обратных связей.

Checkpoint13: [SceneInit/Visibility](native-pc-visibility-runtime.md). Actual
Init и System transfer, native39/static25, record-oriented source и1215
differential checks. Whole manager ctor46C0F0 остаётся capped, это явно
borrowed dependency, не factory-success seam. Next: SceneRender45EC70 через
Shadow/occluder dependencies, concrete Octree/portal child walk; preserve the
different first camera plane and normal/debug root selection. Не расширять
direct SMO/SAN37 denominator за счёт зависимых managers.

Checkpoint14: [whole SceneRender/Shadow/DX state](native-pc-scene-render-runtime.md).
Actual Scene67/Shadow lifetime14/static23, source state-cache960 comparisons,
6 unit tests/CTest17. Whole draw no longer just a static map: original
perspective/alternate ordinary/partition calls complete through real empty
Shadow/Lens phases, explicit COM leaves only. Далее **OcclusionVolume mesh
and camera volume planes**, then Octree/portal clipping; native shader/light
shadow nonempty path stays open, not replaced by unconditional success.

Checkpoint15: [Occlusion geometry/Scene](native-pc-occlusion-runtime.md).
Exact1B8/original TU, native84/static24, plane math1483 comparisons/CTest17.
Standalone CPU weld and cached plane consumer выполнены; full Init470FE0
достиг100k cap и не продолжался. Далее **Octree concrete child queries**,
затем portal geometry; параллельно по встреченным callers искать topology
builder и проверку authored decagon. Наличие формы в SMO больше не выдаётся
за доказанную успешную runtime-инициализацию. Новых manager dependencies
в direct37 denominator не добавлять.

Checkpoint16: [Octree runtime/source](native-pc-octree-runtime.md).
Native43/static24, original-named partial PartitionNode/Octree source28,
3104 exact fields/320cases и CTest18. Original sphere-mask shortcut сохранён,
не заменён идеальной geometry overlap. Normal traversal/isolated45E870 copy
capped100k; whole Debug21 Scene отдельно подтверждён, не считается заменой.
Далее **ZonePortal/ZonePortalNode→plane/polygon clipping**, сохраняя список
protected constructor/copy/topology gaps и неопределённые original names.

Checkpoint17: [Portal runtime/source](native-pc-zone-portal-runtime.md).
Native63/static32, partial Portal/PortalNode source19,1024 plane comparisons/
256cases/CTest19. Original whole Scene front/closed/backface/empty/partial
aperture and cyclic Zone graph now executed, imported CRT exit registration
explicit and original cleanup runs. Далее **491AA0 polygon clipping and its
scratch lifecycle**, сохраняя game Open/debug и protected near-plane45E870
gaps. Пять original getter names берём из диагностик, остальные analytical
имена не выдаём за восстановленные C++ symbols; PS2 по-прежнему deferred.

Checkpoint18: [polygon clipping/ring contracts](native-pc-polygon-clipping.md).
Native28/static22, geometry-only source11/3842 differential fields256cases,
CTest20. Logical resize491660/partial copy491A30 и mixed alias metadata move
разделены; repeated-first correction и keepCoplanar/empty contracts сохранены.
Unnamed helper не получает invented class/denominator. Далее **spBSPNode** —
второй concrete spatial query путь из SMO, позволяющий закрыть обе authored
альтернативы PartitionNode; protected normal plane-copy/ctor остаются open.

Checkpoint19: [BSP runtime/source и camera Zone](native-pc-bsp-runtime.md).
Native48/static17/source17,2046 differential fields256cases/CTest21. ExactAC,
independent plane/polygon и original query distinctions перенесены в partial
source. Whole Scene выбирает camera leaf Zone; это не выполнение normal
recursive plane-copy45E870. Далее по этой же цепочке: оставшиеся spatial
registration consumers/geometry-to-render inputs, приоритет direct SMO/SAN,
без замены неизвестных helper-ов и без нового unrelated class sampling.

Checkpoint20: [spatial consumers](native-pc-spatial-consumers.md), native94/
static15. BSP Zoned Static stays-root отличается от Octree always-mask;
Occlusion borrowed lists/без billboard exception и Static owning duplicates
исполнены. Whole Debug21 draw dedup не закрывает normal protected traversal.
Далее — Collision owner/registration и оставшиеся geometry-to-render inputs;
до deadline текущего цикла — сверка checkpoint manifests/DB/source evidence.

Цикл6 сентября до12:00: [workbench](native-research-workbench.md) переводит
выбор задач на общий blocker/dependency порядок и раздельный PC/PS2 ledger.
Начальный logical queue хранится в `research/native-work-items.json`, unknown
behavior отделён от unknown name/path. Новый
[plane-storage checkpoint](native-pc-visibility-plane-storage.md) подтвердил
resize/reuse, raw all-enabled, release, copy-constructor и outer append,
но не подменил ими assignment45E870 или manager ctor46C0F0. Дальнейшая очередь:
эти две точные границы по новым evidence, затем остающиеся concrete spatial
consumers и actual FFPS/FAT mesh submission. PS2 остаётся second tier;
перенос прежних доказательств в platform ledger не считается новым reverse.
