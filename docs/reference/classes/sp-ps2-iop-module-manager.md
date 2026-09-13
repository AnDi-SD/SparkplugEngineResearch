# spPS2IOPModuleManager

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spPS2IOPModuleManager](../../../Sparkplug/Code/SparkBasePS2/spPS2IOPModuleManager.h).

## RTTI и границы

Registration initializer `0x00484C70` доказывает:

| Свойство | Значение |
| --- | ---: |
| class name | `spPS2IOPModuleManager` @ `0x0045B510` |
| class ID | `0x59264170` |
| base ID | `spBaseObject / 0x415352A1` |
| singleton global | `0x0049FBF8` (`GP-0x4578`) |
| exact size | `0x28` |

Factory и clone `0x001E9CD0` оба выделяют ровно `0x28`. Следующий RTTI getter
начинается по `0x001E9F30`; последний method этого класса — support destructor
thunk `0x001E9F20`.

## Layout

| Offset | Size | Роль |
| ---: | ---: | --- |
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

`0x001E9B20` безусловно пишет `1` в byte `+0x18`; bootstrap вызывает его сразу после создания manager. В пределах самого класса других чтений нет. Поэтому имя `initialized` не присваивается, несмотря на вероятную роль.

## Открыто

1. Original source/header path и method/member names.
2. Исходный тип контейнера и причина one-byte allocation anomaly.
3. Семантическое имя/consumer флага `+0x18`.
4. Точное SDK-имя loader `0x0041B2A0` и meaning его return value кроме знака.
