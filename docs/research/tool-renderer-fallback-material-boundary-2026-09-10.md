# Источник renderer fallback material: PC / PS2

10 сентября 2026. PC producer остаётся неподтверждённым. Независимый PS2 static
audit установил создание material/pass/двух StdLayers; переносить этот результат
в PC `+C9C0` нельзя. Новые guest-пробы, GPU, сборки и production edits в этом
follow-up не выполнялись.

## PC: известная граница

Сохранённый dispatch observation прошёл только `004563F0` до первого входа в
resolved target:1608 instructions,0 seams, micro100k/2s, child30s,1 worker.
Тело конструктора исполнило **ноль** инструкций. Оригинальный путь:

```text
4C5A10 -> 443480 -> 4AE370 -> 13D4A50 -> 4563F0
                                       [13B209C] -> 13B85C0
                                                   call A0D3E0 -> 888940
```

Ранее полный constructor достиг cap; здесь он не повторялся и не продолжался.
Сохранённый literal audit обнаружил18 C9C0 references:17 reads и один destructor
clear (`45622E`). Он не исключает computed/protected stores. CP36/42 вручную
создают fallback fixture и присваивают C9C0; actual `4A9460` material factory
даёт пустой material, а не доказанный renderer-owned fallback graph.

Новый ограниченный static follow-up проверил только reachable dispatcher
prefixes `888940..88895D`, `888999..888A99`, `A0D3E0..A0D3FD`. ASCII gap не
декодировался как исполняемый путь. `888947` захватывает semaphore `11B6264`
(raw image byte1); `8889B7` читает таблицу из10 context pointers по `11B66F4`,
проверяет tags `LOCK`/`API#`, затем выбирает ветку по context byte и сохранённому
caller stack. `888A39` очищает выбранный context, `888A6F/8D` публикует данные
из caller stack. `A0D3F8..FC` содержит conditional INT3 route.

Этот prefix не называет material factory или store в C9C0 и не даёт отдельного
входа с установленным producer contract. Raw semaphore1 не доказывает причину
старого cap и не разрешает подменять runtime context. Общий harness исполняет
инструкции с cache invalidation, но не содержит восстановленного decoder этого
protected payload. Единственный доступный cached xref package в
`local-data/research-cache/xrefs/` относится к семи другим целям; нового
dispatcher/producer trace в нём нет. Полный scan или повторный guest не запускался.

Следующая PC-цель конкретна: получить независимую границу инструкции producer
в payload `13B85C0` — положительный store в C9C0 и его actual allocation/class,
pass/layer count и state writes. Нужен новый justified decoder/call trace с
известным контрактом, а не исполнение прежнего capped constructor до большего
лимита. Пока такой границы нет, production fallback factory и полный BloomX
multi-pass на его основе остаются отложенными.

## PS2: отдельное доказательство producer

PS2 leaf `001FC30C` вызывает common constructor `0017A350`. Полное тело
`[0017A350,0017B118)` сохранено с original SHA. На ветви успешных allocations:

| Original call/store | Наблюдаемое действие |
|---|---|
| `0017B02C -> 001F2A90`, store `0017B040` | concrete PS2Material, allocationD0/alignment16, renderer `+CB88` |
| `0017B03C -> 00170320` | pass blend0, count0,8 null layer slots |
| `0017B058 -> 0016EFB0` | устанавливает pass0, material pass count1 |
| `0017B060/78 -> 00170D30` | два StdLayers, каждый outer14 + nested MaterialTexture80 |
| `0017B070/A8 -> 0016FEF0` | устанавливает layers0/1, count2 |
| `0017B0A0` | второй nested Texture `+14` (raw state1) получает0 |

Общий material constructor `0016F570` задаёт11 raw states:
`[0,0,1,2,1,1,3,0,4,1,6]`. Nested ctor `00173440 -> 0017B1F0` пишет только
первые9 texture states:

| Layer | Raw states0..8 |
|---|---|
| 0 | `[0,3,1,0,0,G,2,0,0]` |
| 1 | `[0,0,1,0,0,G,2,0,0]` |

`G = current raw word[00476CB0]`; initializer `0047F2D8` задаёт `FF000000`.
Known mask для PS2 texture states — `0x1FF`, **не `0xFFF`**: storage words9..11
эта цепочка не пишет. Матрица nested texture — identity3x3, has-static-UV0.
Material power `+C0` не инициализируется этим producer. Raw color sources и
исправление прежней документации приведены в
[PS2 material dossier](native-class-sp-ps2-material.md).

Получены static instructions и initializer writes, не runtime snapshot,
не порядок полного startup, не доказательство отсутствия последующих writes.
PS2 raw state1=0 не подтверждает PC stage mapping, GS output или отключение всех
неиспользуемых stages. C9C0 (PC) и CB88 (PS2) — разные layouts и разные evidence.

## Evidence и воспроизводимость

Pristine PC SHA256:
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Pristine PS2 ELF SHA256:
`198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE`.

Локальные артефакты (не входят в Git):

- `local-data/results/tools-core-cycle-20260910-0730/material-preview/fallback-material/`:
  previous resolution/audit reports и новый `dispatcher-static.txt`.
- `local-data/results/tools-core-cycle-20260910-0730/material-preview/fallback-ps2/`:
  `extract.py`, exact disassembly, `notes.md`, `manifest.json`;15 literal
  instruction anchors и11 exact/prefix ranges. Небольшие предварительные
  window captures не выдаются за полные соседние функции.

Tracked fingerprints и конкретные pending boundaries:
[`tools-core-renderer-fallback-material-boundary-2026-09-10.json`](../../research/tools-core-renderer-fallback-material-boundary-2026-09-10.json).
Глобальные cycle/status отчёты и reconstructed production source не менялись.
