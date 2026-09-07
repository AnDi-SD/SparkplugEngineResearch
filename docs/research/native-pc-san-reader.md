# PC SAN: полный reader объекта `spAnimation`

Checkpoint 2026-09-05, цикл до 21:00 МСК. Продолжение
[keys](native-pc-animation-keys.md) и [lifecycle](native-pc-animation-lifecycle.md).
Исследуется PC. PS2, запуск игры и интеграция в приложения не выполнялись.

## Доказанная граница

Оригинальные factory `0x0041A090` и `0x0043DAB0` создают animation и serializer;
`0x0043ECC0` читает неизменённые поля четырёх pristine SAN после восьмибайтного
`[classID, SBOO]` prefix. Выполнены весь field loop, allocation/preparation
массивов, attach descriptors, имена треков, tag name helper `0x0043D9D0`,
сортировка tags и обычное разрушение объектов. Затем оригинальный sampler
`0x00479290` сравнивается с переносимым кодом в трёх точках времени.

Это **не** запуск всего FFPS/FAT resource loader. Envelope четырёх однообъектных
SAN проверяет отдельный bounded inspector. Byte stream, общая name registry,
allocation/free, diagnostic output и CRT `acos` — явно заданные fixtures.
Внутренний descriptor pool, constructors, reader, tag string copying и sampler
выполняются оригинальными инструкциями. Доступ к Windows API не предоставляется.

## Serializer: layout и повторное использование

Class ID `0xC0ACBFA6`, direct base `spSerializer` **`0x42429877`**; прежняя
опечатка `0x4242AD77` в карточке исправлена. Exact factory allocation — `0x4C`.
В reader передаётся указатель **на secondary interface `serializer + 0x10`**,
а не начало объекта. `ret 8` снимает `(stream, animation)`.

| Offset от начала serializer | Значение |
|---:|---|
| `0x00..0x0F` | physical `spBaseObject` prefix |
| `0x10` | secondary serialization-interface vtable |
| `0x14..0x28` | шесть объявленных размеров value pools |
| `0x2C..0x40` | шесть текущих индексов в value pools, не pointers |
| `0x44` | текущий индекс в общем times pool |
| `0x48` | объявленный размер times pool |

Reader обнуляет все 14 scratch counters в начале каждого вызова. Отдельная
проверка повторно использует один serializer с двумя свежими animation targets
и предварительно загрязнёнными counters. Это не доказывает возможность безопасно
повторно загружать данные **в уже заполненный target**: очистки старых allocations
и transaction rollback здесь нет.

Field `64` — optional reserve hint: оригинал делает **`Resize(hint + 1)`**.
Это не количество завершённых tracks и не terminator. Без него append увеличивает
capacity по одному. Поэтому `barrel.san` имеет capacity 20, `bw.san` — 21 при
одинаковых 20 tracks. Старая формулировка «reserve» уточнена, не заменена догадкой.

## Что означает результат reader-а

Ни `true`, ни наличие диагностики сами по себе не являются строгой проверкой SAN.
Наблюдения оригинальных инструкций в изолированных malformed fixtures:

| Вход | Возврат | Наблюдение |
|---|---|---|
| пустой stream | `true` | диагностика header read |
| обычный terminator | `true` | без диагностики |
| size code 0 с inline ID 5 | `true` | ID превращается в `InvalidFieldID` |
| terminator и лишние байты | `true` | лишние байты не потреблены |
| total time без terminator | `true` | time уже записан, диагностика header read |
| обрыв extended ID / explicit length | `true` | `ReadHeader == nullptr` ведёт к success exit |
| обрыв total-time payload | `false` | отдельная диагностика payload read |
| успешный time, затем ошибочный payload | `false` | предыдущее изменение target сохраняется |

Причина: после `ReadHeader` по `0x0043ED4A` и `0x0043EE80` нулевой результат
ведёт к `0x0043ED5B`, где очищается служебный data-block объект, затем ставится
`AL = 1`. Это не очистка animation. Полный набор allocation failure, malformed
key/tag strings, exceptions и повторная загрузка заполненного target остаются
открыты; небезопасные overrun/shrink случаи намеренно не исполнялись.

## Переносимый исходник

Добавлены `Code/Sparkplug/spAnimationSerializer.h/.cpp` под исходным именем класса
и доказанным TU `Z:\Sparkplug\Code\Sparkplug\spAnimationSerializer.cpp`.
`ReadFieldsForAnalysis` использует уже восстановленные **`spStream` и
`spDataBlockSerializer`**, а tracks/keys — `spAnimation` и `spAnimTrack`.
Нового альтернативного import core нет.

Host policy явно строже оригинала: ограничены extent/counts, проверены pool
capacities, key shapes и finite values, обязателен terminator в конце extent;
partial object не возвращается при ошибке. Optional binding callback вызывается
после проверки полей и является non-owning lookup, не заменой original registry.
Отсутствие callback оставляет slot `-1`. Общий `spStream::ReadString` сохраняет
безопасную обработку неканонических строк; exhaustive malformed-name validation
не заявляется. Позиция source stream не откатывается при неудаче.

Unknown fields пропускаются общим block codec; записываются их ID. Payload не
сохраняется для round-trip: **writer и lossless export ещё не реализованы**.

## Проверки и воспроизведение

| SAN | Tracks / capacity | Оригинальный reader, checks | Reader + PRS comparison |
|---|---:|---:|---:|
| `bbush.san` | 5 / 6 | вместе с bflower: 382 | 234 |
| `bflower.san` | 17 / 18 | включая два именованных tags | 780 |
| `barrel.san` | 20 / 20 | вместе с bw: 652 | 909 |
| `bw.san` | 20 / 21 | без неподтверждённых pool assumptions | 909 |

Итого 1034 assertions полного native reader-а и **2832 leaf comparisons**
portable/native. Сравниваются также capacity, names, binding slots, track duration,
flags, tags и used pools. Это число проверок, не количество исследованных функций.
Синтетический C++ reader suite — **84/84**; семь CTest suites.
Termination/failure/reuse fixtures — **137/137**, reader hash anchors — **8/8**.

```powershell
python research\inspect_san_reader.py
python research\probe_pc_san_reader.py bbush.san bflower.san
python research\probe_pc_san_reader.py barrel.san bw.san
python research\probe_pc_san_reader_errors.py
python research\compare_pc_san_reader.py --portable .codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugSanReaderTests.exe --asset bbush.san
```

Последняя команда повторяется для остальных трёх имён. Каждый guest call ограничен
100 000 instructions / 2 s; child process — 30 s, portable test child — 10 s.
Game image только pristine/hash-checked. Virtual stream fixture entrypoints имеют
отдельную guest RX page, не перекрывают оригинальный CRT array constructor
`0x00401000`. Это ограниченный эмулятор, не утверждение об абсолютной sandbox security.

Следующий [manager checkpoint](native-class-sp-animation-manager.md) проверил
original reader + real name registry на двух SAN: одновременно живые animations,
shared IDs/reference counts, teardown и повторная загрузка. Portable reader
сохраняет прежний non-owning resolver для явных lookup consumers. Последующее
[owned-binding продолжение](native-pc-actor-binding.md) добавило отдельный
`ReadFieldsWithBindingsForAnalysis` на том же field core: track leases,
освобождение/reload и342 differential registry comparisons. Partial lease failure
очищает references, но не откатывает stream/monotonic IDs. Это host transaction
safety, не изменение наблюдаемой native partial-read семантики.
Следующие рёбра — actor input ownership, resource loader и skin/render submission. Полного готового
импортера/экспортера или исправления текущих приложений этот checkpoint не обещает.
