# `spPS2IOPModuleManager`: загрузка IOP-модулей

Статус: PS2-only class identity, exact layout, singleton/clone lifetime,
root setter, module-path construction, duplicate/forced-load behavior, retry
bound и owned-list cleanup восстановлены. Реальный IOP loader заменён только
явно внедряемым test seam; host-код не заявляет, что способен загрузить IRX.

Контрольный ELF —
`local-data/Winx Club the game PS2/SLES_532.19`, SHA-256
`198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE`,
`GP = 0x004A4170`.

## RTTI и границы

Registration initializer `0x00484C70` доказывает:

| Свойство | Значение |
|---|---:|
| class name | `spPS2IOPModuleManager` @ `0x0045B510` |
| class ID | `0x59264170` |
| base ID | `spBaseObject / 0x415352A1` |
| registration | `0x004B6EA0` |
| getter | `0x001E9850` |
| factory | `0x001E9E50` |
| primary vtable | `0x004910C0` |
| support vtable | `0x004910E4` |
| singleton global | `0x0049FBF8` (`GP-0x4578`) |
| exact size | `0x28` |

Factory и clone `0x001E9CD0` оба выделяют ровно `0x28`. Следующий RTTI getter
начинается по `0x001E9F30`; последний method этого класса — support destructor
thunk `0x001E9F20`.

## Layout

| Offset | Size | Роль |
|---:|---:|---|
| `+0x00` | `0x10` | `spBaseObject` |
| `+0x10` | 4 | singleton-support subobject |
| `+0x14` | 4 | owned module-root C string |
| `+0x18` | 1 | one-way flag, default `0` |
| `+0x19` | 3 | padding |
| `+0x1C` | 4 | число list nodes |
| `+0x20` | 4 | sentinel next |
| `+0x24` | 4 | sentinel previous |

List constructor `0x00103950` обнуляет count и направляет обе sentinel-ссылки
на `this+0x20`. Узел имеет размер `0x0C`: `next`, `previous`, owned module-name
pointer. Destructor `0x001E9B30` освобождает каждое имя, затем все узлы, root,
support base и `spBaseObject`.

Default root — literal `host0:c:/usr/local/sce/iop/modules/` по
`0x0045B4E0`. Direct method `0x001E9860` освобождает старую строку, выделяет
`strlen+1`, копирует новую и возвращает true. Native null input не защищён;
portable seam возвращает false.

Clone создаёт default manager и вызывает inherited пустой base-copy slot.
Root, flag и список загруженных модулей не копируются.

## `sub_001E98C0`: загрузка

Восстановленная сигнатура:

```text
bool sub_001E98C0(
    const char* moduleName,
    bool forceReload,
    const char* moduleRootOverride)
```

Алгоритм:

1. построить `<override-or-root><moduleName>.IRX`;
2. линейно сравнить `moduleName` с owned list через case-sensitive `strcmp`;
3. если имя найдено и `forceReload == false`, вернуть true без loader call;
4. иначе вызвать platform loader `0x0041B2A0(path, 0, nullptr)`;
5. при отрицательном результате вывести `Can't load Module %s`, повторить;
6. после успеха вывести `Loaded Module %s`, скопировать имя в новый list node
   и вернуть true.

Счётчик ошибки сравнивается с `0x65` после каждой неудачи. С начального нуля
это даёт ровно **102 loader calls**: значения счётчика `0..101`. После 102-й
неудачи возвращается false. Forced reload успешного имени добавляет ещё один
узел — контейнер не является множеством.

Native success path сначала просит у allocator один байт, кладёт туда
`strlen(name)+1`, а затем копирует полную строку в тот же адрес. Для коротких
реальных IRX-имён это, вероятно, полагается на минимальный размер heap block,
но по C/C++ контракту выглядит как переполнение. Реконструкция хранит безопасный
`std::string` и фиксирует это как native anomaly.

Bootstrap `0x003E4A80` загружает два набора из следующих literal names:
`SIO2MAN`, `SIO2D`, `DBCMAN`, `PADMAN`, `LIBSD`, `SDRDRV`, `CDVDSTM`,
`STREAM`, `MC2_D`. Они подтверждают отсутствие расширения в аргументе:
`.IRX` всегда добавляет сам manager.

## Флаг `+0x18`

`0x001E9B20` безусловно пишет `1` в byte `+0x18`; bootstrap вызывает его сразу
после создания manager. В пределах самого класса других чтений нет. Полный
поиск по singleton global находит только bootstrap loads/stores и class
lifetime; надёжный consumer флага в shipped ELF пока не найден. Поэтому имя
`initialized` не присваивается, несмотря на вероятную роль.

## Реконструкция и проверки

- inferred `Sparkplug/Code/SparkBasePS2/spPS2IOPModuleManager.h/.cpp`;
- exact object/list-node layouts и адреса в PS2 ABI header;
- тестовый loader проверяет два failures перед success, готовый duplicate,
  forced duplicate, override/default roots, `.IRX`, exact 102-attempt failure,
  flag и default-state clone;
- полный `SparkBaseTests` проходит.

## Открыто

1. Original source/header path и method/member names.
2. Исходный тип контейнера и причина one-byte allocation anomaly.
3. Семантическое имя/consumer флага `+0x18`.
4. Точное SDK-имя loader `0x0041B2A0` и meaning его return value кроме знака.
