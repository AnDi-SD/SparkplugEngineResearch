# `spAsyncFileStreamManager`: общая async-загрузка stream

Статус: общий PC/PS2 класс разобран и реконструирован 4 сентября 2026 года.
Это abstract singleton-like граница между клиентами и platform leaf. Доказаны
RTTI, размер `0x18`, отдельный dispatch/support word, callback contract и
платформенные vtable offsets. Original header/TU и имя supporting base пока не
найдены.

## Идентичность

| Свойство | PC | PS2 |
|---|---:|---:|
| class ID | `0x7EA51364` | `0x7EA51364` |
| base | `spCrossPlatform / 0x20A72504` | то же |
| registration | `0x0084D530` | `0x004A25B0` |
| primary vtable | `0x0072922C` | `0x0048C950` |
| support vtable | `0x00729228` | `0x0048C974` |
| factory | null | null |
| native size | `0x18` | `0x18` |

PC registration initializer `0x006D7CE0` и PS2 initializer `0x0047F890`
независимо дают одинаковые ID/base/name. В обеих сборках platform leaf начинает
собственные поля после `+0x18`, а constructor записывает дополнительный vptr по
`+0x14`:

| Offset | Размер | Роль |
|---:|---:|---|
| `+0x00` | `0x14` | `spCrossPlatform` |
| `+0x14` | `4` | полиморфный support/dispatch subobject |

Ранее этот subobject был только общей гипотезой `SingletonSupportSubobject`.
Текущий класс усиливает её: constructor записывает process-wide instance, а
destructor support-части очищает его. Original template/base name всё ещё не
доказано, поэтому реконструкция не придумывает `spSingleton<T>`.

## Lifetime и RTTI

PC constructor `0x006BE6E0` и PS2 constructor `0x001120A0` вызывают
`spCrossPlatform`, устанавливают оба vptr и без проверки заменяют global
instance (`0x0075DB94` на PC, GP-relative `0x0049F828` на PS2). Destructors
так же без проверки очищают global. Вложенные/конкурирующие экземпляры поэтому
не восстанавливают прежний pointer.

Registration factory null; clone тоже null. Platform leaf создаётся своим
factory и повторяет singleton installation через base constructor.

## Два manager slots

Callers доказывают следующий analytical contract:

```cpp
bool Request(const char* streamName,
             spMemoryStream* destination,
             void (*completion)(void*),
             void* context);
void Update();
```

Original spellings отсутствуют, поэтому в portable header они остаются
`vfunc_Request` и `vfunc_Update`.

Из-за ABI и размещения support-vtable offsets различаются сильнее обычных
восьми PS2 service bytes:

| Операция | PC slot | PS2 slot | common implementation |
|---|---:|---:|---|
| request/load | `+0x1C` | `+0x30` | abstract/null |
| update/pump | `+0x20` | `+0x34` | immediate no-op |

PC caller `0x00458683` лениво создаёт `spPCAsyncFileStreamManager`, передаёт
имя, destination stream, callback и context через slot `+0x1C`. PS2 caller
`0x0017D160` делает то же через `+0x30`, включая пятый MIPS argument register
для context. Главный PS2 update path `0x001314E0` вызывает slot `+0x34`.

PC common request slot указывает на `_purecall`; PS2 common slot null. Common
update на PC — однокомандный `0x0048EAA0`, на PS2 — `0x00112000`. То есть
полезное поведение принадлежит только platform leaf.

## Реконструкция и тест

Добавлены inferred `Code/SparkBase/spAsyncFileStreamManager.h/.cpp`:

- сохранены class/base IDs и null RTTI factory;
- абстрактный request имеет доказанные четыре аргумента;
- update сохраняет common no-op;
- portable process-wide pointer повторяет overwrite/clear lifetime contract;
- native `0x18` layouts и обе vtable пары записаны отдельно для PC/PS2.

Тестовый leaf проверяет RTTI chain, отсутствие factory, singleton installation,
callback/context и очистку instance. Host-layout намеренно не сравнивается с
32-bit native layout: portable class не маскирует неизвестный support type.

## Следующие зависимости

1. `spPCAsyncFileStreamManager / 0x15B533A4` — stateless leaf размером `0x18`,
   synchronous request implementation.
2. `spPS2AsyncFileStreamManager / 0x57746EF7` — leaf размером `0x37A0` с queue,
   shared buffers и platform async state.
3. Тип destination подтверждён как минимум совместимый с `spMemoryStream`:
   обе реализации прямо вызывают его невиртуальный resize helper до stream-copy.

## Открытые вопросы

1. Original header/TU, имена request/update и callback typedef.
2. Original имя и C++ форма support subobject по `+0x14`.
3. Возвращаемый тип request: callers игнорируют результат, leaves формируют
   `true`; portable модель использует `bool` как наиболее узкий подтверждённый
   контракт.
4. Thread-safety process-wide instance: в доступном коде синхронизации вокруг
   создания/удаления нет.
