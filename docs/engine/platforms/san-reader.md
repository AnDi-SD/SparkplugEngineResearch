# PC SAN: полный reader объекта `spAnimation`

## Serializer: layout и повторное использование

Class ID `0xC0ACBFA6`, direct base `spSerializer` **`0x42429877`**; прежняя
опечатка `0x4242AD77` в карточке исправлена. Exact factory allocation — `0x4C`.
В reader передаётся указатель **на secondary interface `serializer + 0x10`**,
а не начало объекта. `ret 8` снимает `(stream, animation)`.

| Offset от начала serializer | Значение |
| ---: | --- |
| `0x00..0x0F` | physical `spBaseObject` prefix |
| `0x10` | secondary serialization-interface vtable |
| `0x14..0x28` | шесть объявленных размеров value pools |
| `0x2C..0x40` | шесть текущих индексов в value pools, не pointers |
| `0x44` | текущий индекс в общем times pool |
| `0x48` | объявленный размер times pool |

Field `64` — optional reserve hint: оригинал делает **`Resize(hint + 1)`**.
Это не количество завершённых tracks и не terminator. Без него append увеличивает
capacity по одному. Поэтому `barrel.san` имеет capacity 20, `bw.san` — 21 при
одинаковых 20 tracks. Старая формулировка «reserve» уточнена, не заменена догадкой.
