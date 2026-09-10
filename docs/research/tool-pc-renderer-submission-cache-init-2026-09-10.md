# PC renderer: начальные кеши общего Submit

Свежий original `4C5AB0` и затем полный `4BCF20` штатно вернулись. Это закрывает
ранее остановленную cold-selector границу и даёт узкий общий cache initializer.
Полный готовый контекст draw или OpenGL multipass этим результатом не объявляется.

## Original proof

Запущен один fresh guest с неизменным профилем `protected-constructor` 6M/24s,
30s на child, один worker. Constructor выполнил4 780 619 инструкций за12,1741с;
state initializer —123 313 за0,3009с; весь child —12,9627с. Обычный heap64КиБ
использовал19 168 байт, отдельный bounded renderer mapping64КиБ содержит actual
allocation62 312 байт. Это размеры guest storage, не измерение process peak RAM.
Предыдущие capped guests не возобновлялись, игровые функции и VM не заменялись.

После original factory return сохранён before snapshot. Единственное добавленное
поле — `renderer+C9E8`, указывающее на явно объявленный внешний COM recorder.
Он возвращает `80004005` для setter и останавливает probe перед любым readback,
не записывая выдуманных device values. Окно, GPU, игра и DirectX runtime здесь
не запускались. Before/after включают весь объект и маску original writes.

Cold `4BCEC0→431760` естественно завершился: все256 render IDs и64 IDs на каждом
из8 stages вернули AL0, lazy flag `7405C4` стал0, cache `764240[256]` остался0.
Ни одного `GetRenderState`/`GetTextureStageState` не было. Это полный вызов;
предыдущий анализ последней итерации сохранён как историческое evidence.

| Поле original renderer | После constructor → initializer |
|---|---|
| `+40` frame | 1 |
| `+C868[12]`, `+C898[72]` raw caches | FFFFFFFF |
| `+C29C[8][9]` desired texture states | 8×`[0,3,1,0,0,FF000000,2,0,0]` |
| `+E8F4[512]` stage caches | FFFFFFFF, включая coordinate/transform caches |
| `+E4F4[256]` render cache | FFFFFFFF кроме пяти desired writes ниже |
| `+E480[8]`, `+E47C`, `+CA20` | FFFFFFFF: bound textures, installed material identity, fog |
| `+CA0C` palette cache | FFFFFFFF, уже записан constructor |
| `+C1B8[8]` dirty flags | 0 |
| `+C190` borrowed light list | 0, запись в `4BCFC2` |
| `+E4E8`, `+E4EC` color source caches | 11/10, записи `4BD036`/`4BD040` |

Original cached writer `4B0A90` вызван строго в порядке143=1,27=1,15=1,24=192,
25=7. Каждый callback видел предыдущий cache word FFFFFFFF. HRESULT проигнорирован,
после вызова в cache сохранён desired value. Это не успешное применение на GPU.
Render171 cache остаётся FFFFFFFF; отдельно наблюдённый actual device171=1
из [device-reference](tool-pc-device-state-reference-2026-09-10.md) — другой факт.

`+C18C` selected material, `+C194` packed color, `+C198[8]` desired texture
identities и `+E4A4[17]` lighting payload/power не получили original writes ни
до, ни после initializer. Маркер CC в этих полях принадлежит host allocator;
это не игровой default и не значение для production.

В исходном JSON capture два field labels были ошибочны: `palette` указывал на
E47C, `installed_material` — наC18C. Адресные инструкции `4BB1CB`/`4BB1D3`
подтверждают palette CA0C, а `4BE186` — installed material E47C. Исправленный
read-only analyzer читает правильные адреса из прежних BIN; исходный JSON,
bytes и write masks сохранены без изменений. Числовое production palette
FFFFFFFF остаётся верным; это исправление annotations, не алгоритма.

## Общий перенос и граница

`spDXRenderer::InitializePCSubmissionCachesForAnalysis` инициализирует только
подтверждённый subset существующего `SubmissionStateForAnalysis`: frame, raw и
device caches, desired texture states, bound identities, dirty flags, palette
и два color-source caches. В word invalidation используется общий helper старого
`spRenderer::InvalidateStateCachesForAnalysis`; desired texture words берутся из
существующего `spMaterialTexture`; пять writes проходят существующий cached writer.

NULL callback отклоняется до изменения состояния. В остальных случаях HRESULT
сохраняет original семантику. Geometry, installed material, lighting payload/power,
packed color/globalBlackARGB, desired texture identities, draw state и UV matrices
сохраняются как явно переданные inputs. Их валидность обязан установить потребитель
до Submit. Fog/light cache fields вне этого DTO не добавлялись. Инициализатор не
эмулирует native invalid pointer E47C в типизированном host `installedMaterial`:
этот указатель также остаётся caller input до обычного InstallMaterial.
Инициализатор не
является factory готового context; исходные analytical zero defaults не менялись
и ошибкой реконструкции не объявлялись. Device startup/reset и renderer destructor
не переносились. Default material приходит из [отдельного original producer](tool-pc-renderer-default-material-2026-09-10.md).

`spRendererSubmitTests` содержит отдельные checks точных known values, порядка
callbacks/E_FAIL caching, сохранения sentinel inputs и отказа без callback.
Исторические `--case` fixtures и их numerical comparisons не менялись.
Первая общая сборка скомпилировала production, но отклонила два новых тестовых
lambda без явного capture локального `invalid` в C++17. Root добавил только
`[invalid]`; failed log сохранён отдельно. Повторная согласованная сборка прошла:
RendererSubmit227, RendererScene574, FunctionEval290, ColorFunction118,
MaterialColor235, UVFunction386, ParticleSerialization46, FullLoader213;
CTest8/8 за19,29с. Read-only verifier сохранённых captures отдельно прошёл27
проверок без исполнения guest. Это проверки затронутого общего блока, не нового
готового tools context. Сохранённый DLL имеет SHA256
`2B0B7D474DE0EFF16F160DCD59895A4C0ECED6B3169855F49DDA9D08761266EB`.
Оба build logs, CTest log и immutable DLL находятся в локальном
`local-data/results/tools-core-cycle-20260910-0730/particle-loop-init/`
с префиксом `shared-random-cache-`.

[Manifest](../../research/tools-core-pc-renderer-submission-cache-init-2026-09-10.json)
содержит hashes. Локальные script, полный report, before/after BIN и coverage,
read-only verifier и original listing находятся в
`local-data/results/tools-core-cycle-20260910-0730/material-preview/fallback-material/`
под именами `probe_renderer_state_init.py`, `renderer-state-init-fresh-run1*` и
`analyze_renderer_state_init.py`. Эти артефакты не входят в Git.
