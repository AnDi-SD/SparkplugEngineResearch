# spPCAsyncFileStreamManager

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spPCAsyncFileStreamManager](../../../Sparkplug/Code/SparkBasePC/spPCAsyncFileStreamManager.h).

## Идентичность и layout

| Свойство | Значение |
| --- | ---: |
| class ID | `0x15B533A4` |
| base | `spAsyncFileStreamManager / 0x7EA51364` |
| native size | `0x18` |

Registration initializer `0x006D7C50` связывает literal
`spPCAsyncFileStreamManager` с base registration `0x0084D530` и factory.
Factory выделяет ровно `0x18`, вызывает common constructor `0x006BE6E0` и
только заменяет два vptr. Собственных полей нет. Exact source/header path не
найден; `SparkBasePC` подтверждается именем/platform graph, но созданные файлы
помечены inferred.

## Vtable

| Slot | Target | Роль |
| ---: | ---: | --- |
| `+0x00` | `0x006BD550` | deleting destructor |
| `+0x04` | `0x005B7A00` | notification no-op |
| `+0x08` | `0x006BD500` | clone |
| `+0x0C` | `0x00413120` | inherited clone-copy |
| `+0x10` | `0x006BD3D0` | registration getter |
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

## Открытые вопросы

1. Original TU/header и spellings request/callback.
2. Назначение sentinel `0xBB40E64E`: обычный неинициализированный placeholder,
   значение общей константы либо артефакт protected PC build.
3. Есть ли callers, которые используют результат request (известные игнорируют).
4. Почему интерфейс назван async на PC, хотя этот leaf не ставит работу в queue.
