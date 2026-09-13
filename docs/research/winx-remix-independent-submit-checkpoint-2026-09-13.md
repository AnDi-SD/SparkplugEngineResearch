# Независимая подача сцены: игровой checkpoint, 13 сентября 2026

Первый перенос complete offscreen supports в клиентский Remix API прошёл закрытый запуск **Алфея 27 → Домино 4 → Алфея 27** и обратимые A/B-переключения. Зафиксированы **10 356 612 успешных клиентских DrawInstance**, без клиентских ошибок подачи, неожиданных native draws и аварийного отключения direct-пути. Исходные D3D draws не подавляются; перестаёт добавляться только наша дополнительная visibility-запись квалифицированного support.

**Граница доказательства:** установленный x86-мост возвращает SUCCESS после отправки DrawInstance в очередь, без отдельного server ACK. Счётчик не доказывает принятие renderer API или GPU completion. Скриншоты подтверждают отображение в проверенных ракурсах, но не дают полного геометрического или пиксельного доказательства. SetupCamera отдельно получает обязательный ответ сервера; успешный ответ также не равен проверке GPU history. См. [CPU/source контракт](winx-remix-independent-submit-contract-2026-09-13.md).

## Запуски и исходники

| Закрытый run | Adapter SHA-256 | Результат |
| --- | --- | --- |
| `053315-307`, PID 21472 | `E64652DD502945751D189491390093567258B19CF32E91BBFDF03208E7C328AF` | До исправления ранней камеры: 0 groups/instances, 999 cameraRejected; работал D3D fallback. |
| `054922-750`, PID 432 | `42775D6D50D7B6CBC20DE1D83E1DEAD4E9237817920A31126B86C2AD77A82DB2` | Ранняя камера, A/B в двух уровнях и возвращение в Алфею; direct log frame 0–28554. |

Полные имена: `play-rtx-20260913-053315-307` и `play-rtx-20260913-054922-750`. Первый DLL собран из frozen `direct-v4` (CPU 840 PASS), второй — из `direct-camera-v2` (CPU 1136 PASS), а не из более позднего рабочего дерева. Оба x86. Клиент Remix `79E88A69…`, сервер `C9D3E807…`, stock renderer `F7C31082…`; полные hashes сохранены в launch/install manifests. Нового renderer или bridge build для этого checkpoint не выполнялось.

## A/B: перенос клиентской подачи

A/A2 — direct включён; B — `winx.keepIndependentScene=True`, работает прежняя visibility extension. Числа ниже взяты из совпадающих frame записей, а не выведены из screenshots.

| Уровень / frame | Direct groups / клиентские instances | Обычные пакеты через D3D, matched | Original / own added supports | Draw-счётчик при выходе из scene scope |
| --- | ---: | ---: | ---: | ---: |
| Алфея A / 600 | 723 / 725 | 41 | 53 / 263 | 496 |
| Алфея B / 1200 | 0 / 0 | 766 | 53 / 986 | 1221 |
| Алфея A2 / 2100 | 723 / 725 | 41 | 53 / 263 | 496 |
| Домино A / 6000 | 219 / 438 | 2 | 10 / 22 | 55 |
| Домино B / 7200 | 0 / 0 | 440 | 10 / 241 | 493 |
| Домино A2 / 9300 | 219 / 438 | 2 | 10 / 22 | 55 |
| Алфея после возвращения / 28500 | 716 / 718 | Нет лога после cap | 64 / 264 | 518 |

`725 + 41 = 766` относится только к ordinary independent cohort. Более широкий native mesh path в этих кадрах даёт `725 + 146 = 871`; в Домино `438 + 8 = 446`. Суммы не являются числом уникальных объектов или доказательством полноты мира. Исходно выбранные supports продолжают оригинальный producer; при движении и изменении камеры доля direct закономерно меняется.

Переходы comparison по полному direct log: A frame 0–1070, B 1071–1800, A 1801–6912, B 6913–9235, A 9236–28554. В B нет direct instances. Полные суммы: 5 784 116 claimed groups, 10 356 612 успешных клиентских instances, 19 226 отказов квалификации групп; cameraRejected, queueRejected, apiFailures, unexpectedDraws, suppressedDraws и selectionTransitions равны 0, failure latch не установлен. Это повторяющаяся покадровая работа, не уникальная опись сцены. Причины каждого отказа группы отдельно не инструментированы.

## Переходы, движение и изображение

F1→Домино завершился `loaded_and_presenting` за 8,51 с; sweep прошёл state 4→4 и 73 кадра между снимками. Обратно F1→Алфея — 8,52 с, state 27→27 и 40 кадров. Root выполнил дополнительное движение в Домино, включая короткое движение влево от стены; финальный state сохранён как 4. Точных координат персонажа этот запуск не записал, поэтому количественного доказательства смещения здесь нет.

Root просмотрел Алфея A/B/A2, Domino moved/B/A2/away и финальную Алфею. Автор отчёта дополнительно просмотрел Алфея A/B, Domino B/A2 и `alfea-reloaded-final.png`: интерьер/персонажи/диалог в Алфее и персонаж/UI у синей стены Домино отображаются. Пересвет и оранжевый оттенок Алфеи сохраняются в обоих режимах; анимация и частицы между кадрами различаются. Это не pixel equality и не обзор всей геометрии. `domino-direct-A.png` отсутствует: сохранён A config, поэтому полного трёхкадрового Domino A/B/A набора не заявляем. `alfea-early-camera-initial.png` — заставка, не Алфея.

## Покрытие наблюдательных логов

Семь неизменённых анализаторов отработали на закрытых inputs. Independent observer достиг 16 MiB и заканчивается на **frame 21588**: 2 601 857 world/material/resource matches, без расхождений world/material/resource/upload. Draw bootstrap: 2 601 091 matches и 922 rejected на frame 474. Texture tokens: 2 601 720 matches, 137 differences на frame 465/466/474/2508. Эти пограничные события сохранены; нулевых расхождений текстур за весь run не заявляем.

Возвращение в Алфею (sweep frames 28203–28243) произошло после observer cap. Его нельзя покрывать предыдущими совпадениями. Direct, camera, native mesh/material/owner/update logs продолжаются до frame 28554 и не достигли своих caps. Camera: 28 044 submitted, 0 failures/mismatches/late/missed. Legacy native mesh: 3 179 853 использований; 134 зарегистрированных compare-upload generations, 0 upload/layout errors. Legacy material: 3 613 928 matches, 0 mismatches. Это метрики оставшегося старого пути, не дополнительное подтверждение всех direct-команд. Retirement events учитываются отдельно от EndFrame: 3274 RenderNode, 33 PartitionNode, 3 Scene; durable object identity этим не закрыта.

## Сохранённая неудачная попытка и cleanup

Исторический E646 run остаётся failed admission, а не экспортом: direct 0/0 при 999 cameraRejected; обычная camera submission затем успешна 1003 раза. Его observer имеет 765234 world/material/resource matches. Texture differences 766 относятся к первому world frame 273 (transport writes 1160); drawRejected 922 — к отдельному frame 282, где textureMatched уже 766 (writes 18). Точная причина draw rejection не установлена. Root видел fallback Алфеи с dialogue state 70; initial image была заставкой.

Оба запуска использовали отдельные workdir и существующий маршрут startLevel. После закрытия второго запуска **user.conf, winx.ini, .trex/bridge.conf и четыре save/settings файла совпали с before обоих запусков**. Watcher receipts: 05:37:00 и 06:15:56 МСК; автор отчёта restore повторно не выполнял. Все after bytes сохранены отдельно.

В обоих actual `workdir/rtx-remix/logs` сохраняются OMM budget info и ошибка выхода `[41] common device objects were not disposed of`; bridge сообщает успешный shutdown cleanup. Полный lifecycle и эти старые проблемы открыты. Первые семь попыток запуска скопированных анализаторов завершились на отсутствующем общем import до анализа; после копирования неизменённого `analyze_native_draw.py` все семь PASS. Initial stdout сохранены. Игра/GPU повторно для отчёта не запускались.

[Failed-admission evidence](../../local-data/rtx-remix/independent-submit-tests/game-checkpoint-failed-admission-v1/evidence.json): 48 hashes, `B216B85EE5BA88E0F5365F57206A116251C06E543C3FA54C22D2A10300685887`. [Закрытый успешный run](../../local-data/rtx-remix/independent-submit-tests/game-checkpoint-success-v1/evidence.json): 109 hashes, `84C475A32AAF5CB9EFE8A02EFDE0850F1D09637D011E6D8F1FFB1F50EFDF43BB`. Включены raw/derived logs, source/build references, A/B configs/screenshots, transitions, saves/configs и транспортное доказательство no-ACK. Исторические frozen artifacts не переписаны.

Следующая граница — серверное подтверждение instance-команд и отдельная квалификация переноса первоначально выбранных draw. Этот checkpoint завершает первый offscreen-only этап; все уровни, материалы, источники света и полный отказ от D3D он не закрывает.
