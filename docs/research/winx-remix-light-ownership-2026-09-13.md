# Владение Remix lights: 13 сентября 2026

Исправлен собственный адаптер `research/rtx-remix/winx_scene_lights.h`. В прежней версии неудачный Destroy терял handle при erase; обновление продолжало Create с тем же hash после неудачного удаления. Ошибка Create с выданным output и повторный вход через API также не имели устойчивого владельца. Это ошибки обвязки, не восстановленных классов игры.

Проверенный снимок: `local-data/rtx-remix/light-ownership-tests/ownership-v6/source/winx_scene_lights.h`, SHA-256 `DFADDC228414A25CD9D34D841BC4D675D4E3301D90CD5064DB548162EBA83E00`. После этого checkpoint основной header может получить отдельный native-source observer; этот отчёт относится к указанному снимку.

## Контракт

- Все необходимые cache nodes выделяются до первого Create. Выданный handle сразу записывается в заранее существующий node без новой allocation, включая error/`bad_alloc` после записи output. Error, отсутствующий output и повторный вход запрещают использование результата. Невыданный API handle восстановить невозможно.
- Destroy стирает владельца только при SUCCESS доступного клиентского API. При error, отсутствующем API или `bad_alloc` запись остаётся `retiring`, `usable=false`. Cleanup повторяется последующими Sync/Clear/Retire/EndFrame; один handle удаляется не более одного раза за внешнюю операцию. Обновление сохраняет node и hash, но Create разрешён только после успешного возврата Destroy старого handle.
- Используется существующий recursive guard. Повторный вход через Sync/Clear/Retire инвалидирует outer revision и запрашивает очистку, без рекурсивных API-вызовов и удаления owner nodes. Через API переходят копии ключей/параметров; borrowed итераторы/ссылки общего map не удерживаются. Изменение кадра, gain или comparison также отвергает результат подготовки.
- Предел — 4096 owner nodes, включая quarantine, и 128 записей scenesSeen. Исчерпание бюджета или allocation failure не освобождает неудачно удалённые handles: новая подача отклоняется, имеющееся владение переводится в cleanup. Зацикленного drain нет. Постоянный отказ Destroy может удерживать quarantine до конца процесса; успешную очистку он не имитирует.
- После первого DrawLightInstance типа в текущем frameId, включая ошибочный/выбросивший `bad_alloc` вызов, нельзя считать подачу отменённой. Ошибка останавливает остальные lights этого типа, переводит их в cleanup и откладывает включение legacy того же типа. Это действует также при comparison, Reset и смене сцены. При продвижении frameId отложенная политика обрабатывается существующим EndSceneLightFrame даже без Sync. При недоступном/ошибочном SetConfigVariable запрос сохраняется для повтора. Это может дать неполный свет при ошибке; двойная подача legacy поверх уже отправленного API-света не используется как fallback.

Функции Normalize/Convert побайтно совпадают с предыдущей версией: SHA-256 их текста `EFCFE2CA264927B650D05F09F1D39EAC198DD2C7DF99E9BB761F732C81DBBB27`. NVIDIA helper, gain, физические параметры, native enabled/hierarchy predicate и места вызова игровых хуков не изменены. `destroys` теперь считает успешные клиентские Destroy returns; `owned` включает quarantine.

## Граница реального bridge

В закреплённом `bridge/src/client/remix_api.cpp:265–377` Create выдаёт новый клиентский LightHandle, Destroy и Draw проверяют его локально и отправляют команду. У этих операций нет server ACK. В серверном `main.cpp:3127–3156` Create устанавливает mapping только при успехе renderer; Destroy игнорирует renderer result и инвалидирует mapping, Draw также игнорирует result. Поэтому исправление гарантирует собственное владение по доступному результату, но не восстанавливает потерю mapping при скрытой серверной ошибке.

Renderer source `src/dxvk/rtx_render/rtx_remix_api.cpp:1135–1241` использует hash как light handle и EmitCs для добавления/удаления/подачи. SUCCESS не означает GPU completion. Исследованы закреплённые исходники, а не объявлена новая проверка всех installed renderer binaries. Bridge, protocol и renderer в этой задаче не менялись.

## Проверка

`Test-LightOwnership.ps1 -Name <fresh>` собирает x86 fixture с настоящим header и recording API, без создания D3D device, игры или GPU. Внешний лимит — 30 секунд. Финальный `ownership-v6`: **16 576 PASS**, 4125 выданных handles, remaining 0, duplicateHashes 0. В число проверок входят операции pressure-сценария с 4096 живыми handles: все сохранены после 4096 отказов cleanup, новый Create запрещён, повторная очистка освободила всё.

Покрыты update, отсутствующий API, output на Create error/exception, Destroy error/exception, отказ каждого pre-Create allocation, reentrant Clear/Sync/Retire/gain, отказ policy, частичный Draw/error/exception, Reset/comparison/смена сцены в одном кадре и последующий EndFrame без Sync. Полная сборка игрового DLL и live/GPU-проверка этим fixture не заявляются.

Сохранены попытки: v1 — compile failure из-за отсутствующего fixture-only REMIX_ALLOW_X86; v2 — ошибочное тестовое ожидание запрета legacy сразу для всех типов, хотя защита относится только к уже поданному типу; v3 — PASS178; v4/v5 — PASS16576; v6 добавляет quarantine всего типа после Draw error. Машинный manifest с хешами: `research/winx-remix-light-ownership-2026-09-13.json`. Игровая установка не менялась, commit не выполнялся.
