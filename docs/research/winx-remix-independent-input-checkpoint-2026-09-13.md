# RTX Remix: independent world/material input checkpoint, 13 сентября 2026

Для измеренного ordinary Static cohort Алфеи входы world/material, прочитанные
до original Prepare, теперь совпадают с последующей native/API отрисовкой:
**665 сравнений на активный кадр, 1 318 030 совпадений в закрытом v6 run**.
Оригинальный renderer и прежняя visibility extension продолжают работать.
Independent observer во всех трёх запусках имеет **`submit=false`**: это ещё не
самостоятельная передача scene geometry, ресурсов или transport.

Общий ориентир готовности RTX-интеграции остаётся **около 45%**. Это оценка
оставшейся работы, не доля поддержанных уровней, объектов или shaders. Этот этап
не исправляет освещение и не подтверждает исправление visibility.

Все цифры, фазы, hashes файлов и source manifests сохранены в
[checkpoint JSON](../../research/winx-remix-independent-input-checkpoint-2026-09-13.json).
Прочитаны только закрытые логи и замороженные install snapshots; текущие
развивающиеся resource headers и результаты следующей стадии сюда не включены.

## Закрытые версии и происхождение

| DLL | Run в `local-data/rtx-remix/runs/` | Завершённый PID | Archived DLL SHA256, начало |
|---|---|---:|---|
| v4 | `play-rtx-20260913-032710-504` | 7508 | `2F5E369FF2A5BF16` |
| v5 | `play-rtx-20260913-033056-537` | 25536 | `3E4208C58E682238` |
| v6 | `play-rtx-20260913-033915-856` | 25384 | `8165D41B631DCF6E` |

Для каждой версии архивная DLL совпала по полному SHA256 с build DLL,
`installation.json` и `launch.json`. Используются
`local-data/rtx-remix/independent-scene-tests/install-v4/v5/v6/source/`:
**по 62 замороженных файла**, полный список и aggregate manifest hash находятся
в JSON. Активные рабочие headers не подставлялись вместо этих версий.

Сам independent header различается между версиями:

| Snapshot | SHA256 `winx_independent_scene_source.h` |
|---|---|
| install-v4 | `CA9220B8D717CB8CE3A091A543140F1D29B36F550A8DDCE82183E8ACB53E3E8F` |
| install-v5 | `E473D056BFF1805156364BD4431CEA15C9FEDB6562D37942089844B7AE0D9FD8` |
| install-v6 | `0DAB925C8E2E419BA4CD1DC18EA45400F09EB37CE03BA7CC034D5CDD2FAC4AD8` |

World-update header во всех трёх snapshots совпадает с прошедшей 97 CPU checks
версией `30E609E1A51B471CD229C6C6EA2EDE2786D00762B841D4CEA927D87F16785A86`.
Его точный hook/lifetime scope описан в
[world-update checkpoint](winx-remix-native-update-checkpoint-2026-09-13.md).

## Что именно сравнивается

Observer получает текущие unique `(support, Model)` inputs внутри scene scope
до original Prepare. Затем сопоставляет их с фактически состоявшимися native/API
submits. `candidates`, `selected` и `compared` имеют разные знаменатели:
823 candidates в активных кадрах Алфеи не означают 823 draws, а 665 сравнений
не означают 665 новых independent instances. `selected` относится к исходной
выборке supports до существующего расширения visibility.

Все sampled comparisons этих запусков относятся к **owner kind 0: exact Static**;
`computedWorld=false`, `dirty=0`. Успех нельзя переносить на динамические
RenderNode, mesh-generation lifecycle, собственную geometry submission или все
материалы уровня. Каждый credited comparison проходит current scope/owner,
world update, hierarchy и epoch guards своей версии; в v6 добавлена финальная
проверка после borrowed reads.

## v4 → v5: причина несовпадения и A/B/A

В v4 имеется **262 675 comparisons**, у всех `worldDifferences=0`,
`materialDifferences=262675`, `matched=0`. Подробного device-state breakdown эта
версия ещё не писала; причину локализовал следующий v5 run.

В v5 **10 628 диагностических samples** показывают ровно одно различие:
`D3DRS_ALPHAFUNC`, state **25**, expected **7 / GREATEREQUAL**, actual
**8 / ALWAYS**; `rawDifferenceMask=0`. Это собственная уже существующая
нормализация adapter: тривиальный alpha test `alpha >= 0` временно выражается
как ALWAYS. Сам native material input не менялся.

Контрольный `winx.keepTrivialAlphaTest` временно сохраняет исходное условие:

| Фаза v5 | Фактический режим | Logged frames | Результат на активный кадр |
|---|---|---:|---|
| A, 392–1083 | Обычная нормализация | 689 | 665 material differences |
| B, 1084–1493 | `keepTrivialAlphaTest=True` | 410 | 665 matched |
| A, 1494–1804 | Восстановлено `False` | 311 | 665 material differences |

Первые попытки записать True не включили режим:
`alpha-native.conf` содержит ключ, слитый с первой строкой комментария:
`# Diagnostic settings only; restart to clear them.winx.keepTrivialAlphaTest = True`.
Парсер не распознаёт это как точный ключ `winx.keepTrivialAlphaTest`.
Только **`alpha-native-enabled.conf`** содержит самостоятельную строку True и
является настоящим B. В **`alpha-restored.conf`** последняя самостоятельная
строка задаёт False; итоговый `live.conf` байт-в-байт совпадает с этим snapshot.
Неудачная первая запись не засчитана за переключение.

Суммарно v5 даёт **937 650 comparisons: 272 650 matched и 665 000 material
differences**, world differences ноль. Фазы определены последовательностью
frame outcomes; filesystem timestamps не выдаются за точную синхронизацию кадров.

## v6: учтено происхождение собственного alpha изменения

Исправление v6 передаёт в comparison текущий `ScopedOpaqueAlphaTest`. Только
если этот scope действительно изменил тот же device, state25 и текущий ALWAYS,
comparison берёт сохранённый original input. Это не общее разрешение игнорировать
state25. Обычная нормализация остаётся включённой, device behaviour сохраняется;
исправляется сопоставление native input с собственным преобразованием adapter.

В Алфее, frames **492–2476**, получены **1982 logged frames × 665**:
**1 318 030 matched**, world/material differences **0/0**. Все **4655 samples**
имеют `alphaInputBeforeAdapter=true`; `maxWorldError=0`, `maxInverseError=0`.
Последнее поле действительно присутствует в этих samples. У v4 его нет:
default zero старого analyzer не засчитан за измерение inverse matrix.

В том же v6 run затем открыт Домино; native state log содержит переход
**state27 → state4**. Frames **2498–23774**, **21 274 logged frames**, имеют
`candidates=0`, `compared=0`, `callbacksRejected=251`. Это не доказательство
неисправности world transform: в замороженном install-v6 header ещё находится
ошибочный guard `node.callbackBegin != node.callbackEnd`.

Установлено, что RenderNode vector `1C8..1D0` содержит membership/lifecycle
listeners, в том числе обратные partition registrations. Непустота обычна и
сама по себе не является producer на Prepare/Draw. Это
[ошибка предположения adapter о cohort](winx-remix-rendernode-listener-contract-2026-09-13.md),
а не сломанное восстановленное поведение. Условие позднее исправлено в source,
но **данная v6 DLL его ещё содержит**. Успешный Domino comparison после этого
исправления этим checkpoint не заявляется.

## World witness, limits и завершение запусков

| Run | Manager `begun/completed` | Witness `queries/accepted` | Aborted / overflow |
|---|---:|---:|---:|
| v4 | 720 / 720 | 717 / 717 | 0 / 0 |
| v5 | 1785 / 1785 | 1782 / 1782 | 0 / 0 |
| v6 | 23 739 / 23 739 | 23 729 / 23 729 | 0 / 0 |

`sequence` — invalidation epoch, не число manager updates. `rootCalls` включает
descendant/standalone calls; три published tokens на qualified batch не
идентифицируют три уникальные сцены. SystemRoot — `scene+14`, а owner `root`
принадлежит partition system: эти адреса не взаимозаменяемы.

World-update и independent logs во всех трёх runs находятся ниже своих caps,
полностью прочитаны с проверкой завершённых PID и стабильности файлов.
Вспомогательный v6 `native-owner-source.jsonl` достиг 16 MiB cap; он не
используется как полный источник totals этого checkpoint. Активность после
последнего Present не добавляется задним числом в frame counters.

Для всех runs сохранены `bridge.conf.before`, temporary config/state и
`bridge-config-restored.json`; записи восстановления датированы соответственно
**03:29:13**, **03:36:26**, **04:10:40 +03:00**. Хеши temporary config совпадают
с launch metadata. Это закрытые записи восстановления; текущая общая установка
не перечитывалась во время следующей работы root.

Артефакты изображений учитываются отдельно от числовой проверки:

- v4 **`alfea-independent-loaded.png` отсутствует** из-за ошибки capture+close
  helper. Он не засчитан за сохранённый или просмотренный capture.
- v6 `domino-before-close.png` существует, root сообщил о просмотре.
- v6 `alfea-current-inputs.png` существует, но его визуальная проверка здесь
  не заявляется. Остальные существующие screenshots перечислены с hashes в JSON;
  одно имя файла не подтверждает режим alpha или качество изображения.

Использованы [analyze_native_update.py](../../research/rtx-remix/analyze_native_update.py)
и [analyze_independent_scene.py](../../research/rtx-remix/analyze_independent_scene.py).
Финальный hash второго analyzer —
`3FAEFC8E435C70586237CE4CFDAFADCCD19E701A36302D5FA32C90B562CC3052`;
его optional resource fields для этих старых runs имеют
**`resourceObservation=false`**. Они не являются результатом новой стадии.
Для подготовки checkpoint изменены только этот документ и связанный JSON;
игра/GPU не запускались, DLL/config/source не изменялись.
