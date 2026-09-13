# PC save: индексация объектов и запись ссылок

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

- failure ID или initial size возвращает false, оставляет0/4 bytes,
  payload flag ещё false;
- failed outer patch seek игнорируется: size дописывается в конец, исходный
  placeholder остаётся0, AL=1;
- failed outer size write тоже игнорируется: placeholder0, AL=1;
- failed outer restore seek даёт корректные bytes, но position8 вместо63,
  AL=1;
- failed SAN writer final seek ставит terminator внутрь pool header;
  outer writer затем фиксирует size25 вместо55 и тоже возвращает success.

Host failure guards отдельно тестируются, не выдаются за native bug parity.

Открыты: полная FFPS save orchestration и запись FAT/file index, source producer
fileID, все concrete relationship payloads/cycles и error gate states,
lossless unknowns. Один успешный reference не считается готовым exporter-ом.
