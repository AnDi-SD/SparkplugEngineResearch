# `spPCAsyncFileStreamManager`: синхронный PC leaf

Статус: класс разобран, реконструирован и проверен 4 сентября 2026 года. Несмотря
на `Async` в имени, единственный request slot полностью читает файл до возврата
и лишь затем синхронно вызывает callback. Leaf не добавляет instance fields.

## Идентичность и layout

Контрольный binary — `local-data/pc-pristine/WinxClub.exe`, SHA-256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

| Свойство | Значение |
|---|---:|
| class ID | `0x15B533A4` |
| base | `spAsyncFileStreamManager / 0x7EA51364` |
| registration | `0x0084D410` |
| primary vtable | `0x00729078` |
| support vtable | `0x00729074` |
| factory | `0x006BD490` |
| native size | `0x18` |

Registration initializer `0x006D7C50` связывает literal
`spPCAsyncFileStreamManager` с base registration `0x0084D530` и factory.
Factory выделяет ровно `0x18`, вызывает common constructor `0x006BE6E0` и
только заменяет два vptr. Собственных полей нет. Exact source/header path не
найден; `SparkBasePC` подтверждается именем/platform graph, но созданные файлы
помечены inferred.

## Vtable

| Slot | Target | Роль |
|---:|---:|---|
| `+0x00` | `0x006BD550` | deleting destructor |
| `+0x04` | `0x005B7A00` | notification no-op |
| `+0x08` | `0x006BD500` | clone |
| `+0x0C` | `0x00413120` | inherited clone-copy |
| `+0x10` | `0x006BD3D0` | registration getter |
| `+0x14/+0x18` | `0x00408350/0x00408370` | exact/kind checks |
| `+0x1C` | `0x006BD400` | request/load |
| `+0x20` | `0x0048EAA0` | inherited no-op update |

Non-deleting destructor `0x006BD3E0` восстанавливает leaf vptrs и переходит в
common destructor. Support adjustment thunk `0x006BD480` вычитает `0x14`.
Clone создаёт fresh leaf через factory и копирует только inherited object state;
как побочный эффект base constructor временно заменяет process-wide instance.

## Request contract

`0x006BD400` принимает:

```cpp
(const char* streamName,
 spMemoryStream* destination,
 void (*completion)(void*),
 void* context)
```

Тип destination подтверждается прямым вызовом
`spMemoryStream::ResizeAndSetSize` (`0x00465500`), а не только virtual stream
операциями. Последовательность неизменна:

1. создать `spPCFileStream`;
2. вызвать `Open(streamName)` и не проверить результат;
3. вызвать `GetSize(&size)` и не проверить результат;
4. вызвать `destination->ResizeAndSetSize(size)`;
5. вызвать destination stream-to-stream slot `+0x34` с source и size;
6. без null-check вызвать `completion(context)`;
7. закрыть и удалить source;
8. вернуть true.

Таким образом callback означает «синхронная процедура закончилась», а не
гарантию успешного чтения. Native требует валидные destination/callback. При
ошибке `GetSize` локальное слово сохраняет загруженный из `0x00744230` мусорный
sentinel `0xBB40E64E`, что может привести к огромной аллокации. Portable версия
на этом недействительном пути инициализирует size нулём — явная safety-разница,
чтобы тест/инструмент не повторял потенциальный runaway allocation; на успешном
пути байты и вызовы совпадают.

## Реконструкция и тест

Добавлены inferred `Code/SparkBasePC/spPCAsyncFileStreamManager.h/.cpp`, RTTI
factory и exact PC layouts/addresses. Изолированный тест создаёт четырёхбайтный
temporary file, выполняет request, проверяет немедленный callback, полный размер,
позицию и exact payload destination, затем удаляет файл. Сборка не запускает игру
и не обращается к пользовательским ресурсам.

## Открытые вопросы

1. Original TU/header и spellings request/callback.
2. Назначение sentinel `0xBB40E64E`: обычный неинициализированный placeholder,
   значение общей константы либо артефакт protected PC build.
3. Есть ли callers, которые используют результат request (известные игнорируют).
4. Почему интерфейс назван async на PC, хотя этот leaf не ставит работу в queue.
