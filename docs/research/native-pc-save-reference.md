# PC save: индексация объектов и запись ссылок

Продолжение [SAN writer](native-pc-san-writer.md), 6 сентября 2026.
Проверяется общий `spSerializer`/FAT reference protocol, не полный writer
FFPS-файла. PC image SHA-256:
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
PS2 новых доказательств не получает.

## Подтверждённые точки

| PC VA | Наблюдаемая операция |
|---|---|
| `466FA0` | FAT IndexObject: duplicate check, ID, entry, maps/list |
| `465BF0 -> 47DD80` | save-entry constructor |
| `4664F0` | object-pointer -> FAT entry |
| `4672C0` | serializer IndexResource, затем secondary relationship slot |
| `467300` | null или dispatch object -> serializer -> IndexResource |
| `422530` | actual object RTTI -> registered serializer lookup |
| `467260` | actual class ID + `SBOO`, 8-byte object header |
| `467350` | inline/repeated/null reference writer |
| `5A7DB0` | SAN secondary index slot: true/no-op |

Названия методов в переносимом API аналитические. Оригинальные TU
`Z:\Sparkplug\Code\Sparkplug\spSerializer.cpp` и
`Z:\Sparkplug\Code\Sparkplug\spResourceFATSerializer.cpp` подтверждены;
имя самого FAT helper/header/member не придумывается.

## Index и lifetime

Native FAT отклоняет duplicate object pointer без расхода нового ID. Wrapper
`4672C0` превращает это в success и **не обходит его связи повторно**.
Новый объект вносится в FAT до secondary relationship call: это опора для
рекурсивного графа, но пока не runtime-доказательство всех concrete cycles.
`467300` выполняет ErrorManager gate, затем null-success или actual lookup.

Save-entry24 получает ID, fileID, classID, object pointer, zero offset/size,
null name. Обнуляется только flag byte1C, верхние три bytes остаются прежними.
Это отличается от load-entry, где даже flag byte не инициализируется.
Непустое имя deep-copy с NUL; null и empty names превращаются в null pointer.
Engine RTTI `spAnimation` не проходит `spNamedObject`, поэтому physical name
при индексации Animation игнорируется. Все три resource containers и nextID
действительно обновляются. FAT teardown не владеет runtime объектом.

## Reference wire

Null: один `u32(0)`, без следующего size.

Первое ненулевое употребление:

```text
u32 resourceID
u32 inlineSize          // первоначально0, затем patch-back
u32 actualClassID       // отсюда entry.offset
bytes "SBOO"
serializer fields      // entry.size включает восьмибайтный object header
```

Повтор: `[resourceID, u32(0)]`; новый body не записывается даже если source
после первой записи изменился. `entry.offset/size` остаются первыми.
`payloadWritten` выставляется **до** получения offset/записи header/payload;
неудача после этого не откатывает flag.

Для empty Animation первый reference63 bytes: ID1, size55, offset8, fields47.
Повтор добавляет8 bytes; null ещё4. В норме38 stream writes/5 seeks.
SAN writer patch-ит сначала value counters6..11, затем time counter12,
затем восстанавливает конец. После него outer reference patch-ит size и
возвращается в конец; это последние два seek-а.

## Существенные native ошибки

Направленные проверки установили:

- failure ID или initial size возвращает false, оставляет0/4 bytes,
  payload flag ещё false;
- failed outer patch seek игнорируется: size дописывается в конец, исходный
  placeholder остаётся0, AL=1;
- failed outer size write тоже игнорируется: placeholder0, AL=1;
- failed outer restore seek даёт корректные bytes, но position8 вместо63,
  AL=1;
- failed SAN writer final seek ставит terminator внутрь pool header;
  outer writer затем фиксирует size25 вместо55 и тоже возвращает success.

Это настоящие original instruction paths на явно сбоящем byte-stream fixture,
а не повреждение game/OS файла. Missing-index NULL dereference, allocator
failure, nonempty ErrorManager UI/formatting и любые game/GPU callbacks
ради проверки не запускаются. No cap increase/API forwarding.

## Проверки и восстановление

`research/probe_pc_save_reference.py`: 86 directed checks в 10 независимых
bounded cases; `inspect_pc_save_reference.py`: 9 fixed fingerprints.
Allocator/stream/named-string boundaries явно заданы, actual class lookup,
FAT, ErrorManager empty gate, SBOO header, SAN writer и normal cleanup —
оригинальные. Все отслеживаемые native allocations освобождены.

В `spSerializer.*` добавлены общий header/index/reference code и explicit
secondary hooks. Базовые portable hooks по умолчанию отклоняют неподдержанный
срез; Animation подключает проверенные no-op index и fields writer.
Host API принимает manager явно, не пересылает ErrorManager UI, проверяет
missing index/serializer и **все** final patch failures. После ошибки с
установленным one-shot flag caller должен выбросить save context/output;
свойства rollback и безопасного retry не выдумываются.

`SparkplugSaveReferenceTests` сравнивается с native посредством
`research/compare_pc_save_reference.py`: inline/repeated/null75 bytes.
Host failure guards отдельно тестируются, не выдаются за native bug parity.

Открыты: полная FFPS save orchestration и запись FAT/file index, source producer
fileID, все concrete relationship payloads/cycles и error gate states,
lossless unknowns. Один успешный reference не считается готовым exporter-ом.
