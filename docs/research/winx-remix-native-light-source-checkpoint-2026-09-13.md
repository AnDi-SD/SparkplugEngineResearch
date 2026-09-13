# Winx Remix: native light source, ownership и закрытый прогон — 13 сентября 2026

Переход к исходным полям `spLight` проверен на Алфее и Домино: общий PC producer дал точное совпадение используемых слов подготовленного payload. Освещение визуально не исправлялось: прежняя конвертация, gain=10 и оранжевый вид Алфеи сохранены. Проверка журнала выявила две неверные подписи режима на границе A→B; строгий результат анализатора **FAIL**, а не общий зелёный статус.

Закрытый run `play-rtx-20260913-073622-393`, PID 6432, 07:36:23–07:43:29 МСК. Игровой EXE `C27EA9DB…` не менялся. Установленная x86 DLL — `2D3CB324D793B5DF2397BDEDCBF2623209612B9183F532B9100B646C89679949`, собрана именно из `native-light-v1/source`. Client/renderer прежние; server audit `D7FEE509…` относится к мешам. Полные SHA, frozen source, install receipt и закрытые логи — в [manifest](../../research/winx-remix-native-light-source-checkpoint-2026-09-13.json).

## Что действительно меняется

Адаптер читает текущую подтверждённую registry-запись `spDXLight` (`primary=6F0C88`, ненулевая сцена, Enabled, корректные type/bytes), отказывается от dirty low flags и передаёт цвет, интенсивность, положение/направление мира, range/attenuation и параметры конуса в общий `spPCLightPayload.h`. Сам не вызывает native producer, не очищает игровые dirty flags и не меняет порядок оригинального обновления.

Побитовое сравнение ограничено словами, потребляемыми существующим конвертером: маски type0=`0x7000F`, type1=`0xE8E00F`, type2=`0x3FFE00F`. Только после совпадения заменяются эти слова; остальные сохраняются из payload. `used` означает выбор вычисленных слов для конвертера, а не новый CreateLight, подтверждение renderer или GPU completion. Mutable defaults не угадываются.

Общая арифметика и её происхождение описаны в [shared producer checkpoint](winx-remix-shared-light-producer-2026-09-13.md): 14 существующих exact profile groups, 13 неизменных участков pristine/debug. PS2 не изменён; выделен PC backend math, DirectX backend в приложения `tools/` не добавлен.

## Проверки и результат

| Проверка | Результат и граница |
|---|---|
| CPU native-light-v1 | 50 PASS; точный production snapshot установленной DLL |
| CPU native-light-v2 | 50 + 110 PASS; production идентичен v1, изменено только прежнее test-ожидание deferred Reset |
| CPU ownership-v6 | 16 576 PASS, 4 125 issued, remaining=0; самостоятельная fixture с отключённым native-source |
| Новый JSON/IO analyzer | 42 PASS; malformed counters/masks, duplicate keys, partial rows, PID/current file identity и строгий comparison guard |
| Игровой native source | 27 287 attempts = matched; 26 000 uses; mismatch/dirty/invalid=0; sampled type1=95, type0=10, live type2 не доказан |

Новый [анализатор](../../research/rtx-remix/analyze_native_light_source.py) проверяет `attempts=matched+mismatches+dirty+invalid`, `used<=matched`, типы/маски, PID, стабильность файлов и предел 16 MiB. В frames **484 и 1900** указаны `comparison=true`, но соответственно used=20 и 1. Frozen `DevicePresent` сначала применяет новый `keepNativeLights`, затем пишет счётчики прошедшего кадра. Диагностический режим сохраняет исходные totals и обе ошибки, пишет **FAIL** и возвращает exit 2; исключений по уровню или номеру кадра нет. Root исправил порядок EndFrame/ApplyLiveConfig позднее; это исправление не принадлежит данной DLL и ещё не проверено этим прогоном.

Стабильные A/B/A2 отдельно подтверждены точным совпадением камеры и уровня во всех шести случаях:

| Мир | Native A | Device payload B | Native A2 |
|---|---|---|---|
| Алфея | frame481: matched20/used20 | frame540: 20/0 | frame600: 20/20 |
| Домино | frame1896: 1/1 | frame1984: 1/0 | frame2063: 1/1 |

Исходные snapshots асинхронны; в дополнительной закрытой сверке строки соединены по точному frame/presentIndex. Root просмотрел обе пары A/B и вернувшуюся Алфею; `native-light-first.png` — splash. Алфея→Домино и обратно: `loaded_and_presenting`, 9,51 с/60 кадров и 9,52 с/35 кадров. Только штатное нажатие right=0,25 с в Sweep; полного прохождения или расширенного теста движения не было.

Регрессии: native camera 2 876 успешных подач, ноль failures/stateMismatch/late/missed; direct 1 112 858 и selected 561 169 успешных client enqueue, без API/post-commit/transition faults. Mesh server audit зарегистрировал 1 860 370 renderer API success, errors/invalid handles=0, закрытие `queue_exit_normal`, без достижения log cap. Это не аудит световых API и не GPU completion.

## Lifetime и оставшиеся ограничения

Новая собственная обвязка хранит ownership при ошибках и reentry, выделяет запись до Create, ограничивает повторные cleanup попытки, сохраняет failed handles для последующей очистки и не позволяет deferred Reset потерять владение. CPU fixture проверяет эти границы; она не исполняет игру.

Игровые **events**, включая границы переходов: создано 41, удалено 21, ошибок API в этих events нет. Старые 20 источников Алфеи удалены на frame1038, один Домино — на frame2648. После возврата создано ещё 20 на frame2672. Для последних 20 журнал не содержит shutdown DestroyLight: recorded remaining=20, полного завершения lifecycle не заявляем. Успешное закрытие моста этого не доказывает. Renderer по-прежнему пишет `41 common device objects were not disposed of` и информационное сообщение OMM memory budget.

Копии семи настроек/сохранений после прогона побитово совпали с before; watcher восстановил `.trex/bridge.conf` в 07:43:29.977. Автор checkpoint ничего повторно не восстанавливал и игру не запускал. Проверены 56 frozen run inputs, 209 существующих upstream artifacts; новый inventory содержит 172 файла с SHA. Сохранены первоначальный strict analyzer rejection и ошибка неполного Python snapshot (`analyze_native_draw` отсутствовал до копирования зависимости); окончательная fixture — 42 PASS. Внешность физического света, spot-источники в игре, окончательная очистка последних handles и исправленная маркировка переходного кадра остаются отдельными проверками.