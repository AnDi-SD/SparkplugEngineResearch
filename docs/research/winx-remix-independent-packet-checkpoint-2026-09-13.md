# Winx / Remix: пакет сцены перед прямой подачей, 13 сентября 2026

Закрыт наблюдательный запуск **v9**, `play-rtx-20260913-044409-814`, PID 20716:
Алфея → Домино. Текущие native world/material/resource и draw-bootstrap данные
совпали с последующей успешной API-подачей во всех **1 044 930 сравнениях**.
Texture token совпал в **1 043 724** случаях; **1206 несовпадений приходятся на
первые world frames двух уровней**. Это checkpoint подготовки входного пакета:
сам наблюдатель API instances не создаёт (`init.submit=false`).

Доказательства и SHA256 перечислены в
[manifest](../../research/winx-remix-independent-packet-checkpoint-2026-09-13.json).
Все пути `local-data/` обозначают локальные артефакты вне Git. Установленный x86
adapter: `BE7007660426518C74DE513B28E065204AD2425D22BBCBD387D41E5545C8B3BB`;
он совпадает с DLL запуска и build-v9. Единственный источник описания v9 —
`local-data/rtx-remix/independent-scene-tests/install-v9/source`.

## Что было подготовлено до original Prepare

После подтверждённого update и original selection наблюдатель читает свежий
registry, прежде расширения selection и original Prepare. Он не вызывает native
producer, не меняет игровые данные и не воспроизводит сохранённый last-visible
пакет. Снимок ограничен текущей операцией сцены; own upload verification cache
может отмечать уже выполненное сравнение CPU bytes.

| Часть | Источник и граница v9 |
|---|---|
| Owner / world | Текущие support→Model, membership, Enabled и update/retirement fences. Для разрешённого RenderNode — общие PRS/inverse helpers; для static support — текущие world/inverse matrices. |
| Материал | Ordinary mode2, один pass / StdLayer, обычный texture holder без UV/animation controllers и static UV override. Общие material/texture state mappers работают с собственным локальным cache; игровые классы не копируются. |
| Геометрия | Текущие native mesh/ranges/layout и покрытые CPU bytes; successful-Create lifecycle witness для VB/IB/declaration. Captured upload bytes проверены через общий resolver; в сохранённом пакете нет borrowed byte pointers. |
| Draw bootstrap | Один текущий D3D снимок оставшихся состояний на scope плюс native default material stage1 mapping. Позже проверяется соответствие actual draw. Полностью native происхождение всех D3D bootstrap состояний этим не доказано. |
| Texture | Native material COM key → successful-Create registry, device, creation/content token. `CurrentTexture` проверяется при сравнении. Реестр не удерживает COM references. |

Поддерживается узкая обычная статическая группа; skins, particles, lit/multipass,
неизвестные callbacks/controllers и неподдержанные texture formats остаются за
её границей. Предыдущее расширение D3D visibility selection сохранено.

## Результат закрытого запуска

`independent-scene-source.jsonl`: 3 292 578 bytes, лимит 16 MiB не достигнут;
2431 frame records в диапазоне 0–2476, 724 590 candidate occurrences,
3422 sampled comparisons. Candidate — уникальная пара `(support, Model)` внутри
scope; actual comparison считает повторные успешные API submits. Эти количества
нельзя делить друг на друга как процент покрытия всего уровня.

| Участок | Candidates / scope | Actual comparisons / frame | Frames | Texture matches |
|---|---:|---:|---:|---|
| Алфея | 922 | 766 | 375 | 374 полных кадра; frame 354: 0/766 |
| Домино | 220 | 440 | 1722 | 1721 полный кадр; frame 752: 0/440 |
| Прочие recorded frames | 0 | 0 | 334 | Сравнений нет |

World/material/resource/draw differences и upload differences — 0. Все
724 590 подготовленных кандидатов имели resource/draw/texture readiness;
это не означает, что все прошли более ранние cohort guards. Например, отдельные
rejection totals: model 34 317, pass 90 640, texture 16 194, missing 49 707.
Они имеют другие места учёта и не суммируются в общий coverage denominator.
В sampled world/inverse сравнениях максимальная ошибка 0; это не объявляет
общий float helper битово тождественным оригинальному x87 для любых входов.

В 16 сохранённых texture failure samples `textureReady=true`, `textureMatch=false`.
Лог содержит токен, снятый **при подготовке** (`textureGeneration/textureContent`),
и итог `CurrentTexture`; новый токен и конкретный writer он не записывает.
Texture match означает актуальность COM key и токена, а не сравнение pixel bytes.

В installed-v9 `ResubmitTextures` (`winx_d3d9_probe.cpp:565`) один раз перед
использованием managed texture делает `LockRect(..., flags=0)` / Unlock каждого
mip без изменения pixels. В `DrawIndexed` он вызывается до SubmitSurfaceOverlay;
успешный writable Lock проходит через `ChannelTextureLock:870` и увеличивает
content serial (`winx_native_transport_source.h:124`). Опция была включена.
`texture_frame` показывает writes **1160 на frame 354** и **382 на frame 752**;
на следующих 355/753 — 24/36 при уже полном совпадении, на 356/754 — 0.
Это подтверждённый механизм инвалидирования и корреляция по frame, **вероятная
причина первых несовпадений**. Общий writes включает других producers; всех
1206 случаев конкретному Resubmit без per-texture caller trace не приписываем.

Sweep F1→4 завершился `loaded_and_presenting`: 9,51 с, проверочный интервал
996→1064 (+68 frames), active state 4. Просмотренные `idle.png` / `moved.png`
показывают изменение ракурса и ориентации Bloom. Result не содержит player/world/view
координат, поэтому измеренное пространственное перемещение не заявляется.
`alfea-packet-initial.png` — одиночный кадр диалога, не доказательство движения.

## Проверки и граница следующего шага

Переиспользованы завершённые evidence: CPU observer default-stage **96 PASS**,
texture transport **157 CPU / 697 system D3D9 PASS** (в том числе 171 native
source, 137 native material, 53 buffer transport и 83 texture checks).
Их не запускали повторно ради отчёта. Изменён только
[анализатор](../../research/rtx-remix/analyze_independent_scene.py): optional пять
texture counters, sample ready/match/generation/content и строгие проверки.
Schema=1 и прежние поля сохранены. Новые JSON parser fixtures: **17 PASS**;
первый отрицательный fixture ошибочно менял сразу две optional schemas и получил
ожидаемый отказ более раннего resource guard; исправлен только fixture, попытка
сохранена. Обновлённый анализатор однократно прочитал закрытый v9 log; других
готовых analyzer JSON в run не было, старые анализаторы не перезапускались.

Ранний x64 backend build `1922E8114BD7BC6D5F77E40D84D78BF27A2820D87D88489F9A4D054D359EAD17`
предшествует default/texture observer refinements: это общая контрольная сборка,
не проверка финального x64 native пути. Дополнительный guard текущего stage1
добавлен в рабочий observer **после** install-v9; результат этого запуска его
не проверяет. Ни новый direct route, ни эти последующие изменения сюда не входят.

Следующая операция — подать свежий пакет непосредственно из разрешённой фазы,
с повторными resource/texture/scene/owner fences после подготовки и перед submit.
При изменении токена группа должна отказаться от direct подачи и сохранить
fallback. Убирать защиту или глобально отключать Resubmit ради совпадения
счётчиков нельзя. Offscreen completeness, удаление D3D visibility extension,
durable object identity и полный lifecycle всех ресурсов этим checkpoint не закрыты.
