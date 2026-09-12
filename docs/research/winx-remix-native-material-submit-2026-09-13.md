# Native material → Remix API: первый ordinary unlit путь

13 сентября 2026. Для уже поддержанной native geometry **операции RGB/alpha,
UV selection и sampler теперь берутся из обычного игрового MaterialTexture
через общий восстановленный mapping**. Подключённый путь проверен интеграционным
fixture и закрытым запуском Алфея → Домино, включая переключение native/D3D
источника материала. Прежняя unlit-политика яркости сохранена: этот блок меняет
происхождение данных, а не обещает готовое физическое освещение.

Run `play-rtx-20260913-015622-920`, PID **7920**, завершён. Проверенный x86 adapter:
`B5D7D817955DE484B03CCF53CCD7D1EF93C87AB4AFE2C616D4215FE99BF8F6C3`.
x64 compile-only:
`22CE2B5E83A67F4F4445F238282559170C8A66578E8B632D59E7690EC7E63383`.
Полные hashes исходников, EXE, raw logs, снимков и проверок — в
[JSON-манифесте](../../research/winx-remix-native-material-submit-2026-09-13.json).
Общая функциональная оценка остаётся **45%**, без начисления процентов за этот
ограниченный перенос источника.

Группа выбирается по данным: qualified native geometry, exact `spDXMaterial`,
один `spMaterialPassLayer`, один обычный `spStdLayer` и `spMaterialTexture`,
исходный и эффективный mode 2, UV0 без transform. Имена уровней, материалов и
texture hashes в выборе не участвуют. Новая ABI-константа ordinary texture
holder — vtable **6E8440**. Qualification сверила pristine/debug в **3/3**
областях: таблица, запись таблицы в destructor, getter **467BE0**
`mov eax,[ecx+34]; ret`. Защищённая factory не исполнялась; новых эмуляций нет.

| Вход | Источник и граница |
|---|---|
| RGB / alpha operation | Native `textureStates[1/2]` → общий `ApplyPCTextureStateForAnalysis` → существующий Decode |
| Address U/V и filtering | Native raw `[3/4/6]` → тот же mapping → API sampler |
| UV source / flags | Native raw `[7/8]`; принимаются UV0 и disabled transform |
| Texture identity | Ordinary holder `+34` → exact DXTexture; device и COM texture сверяются с привязанным D3D объектом |
| Color coefficients | Существующий `UnlitInput`; source material colors / Renderable28 не подставляются |
| Arguments, RESULTARG, render/blend states и texture bytes | Сохраняются D3D guards/transport и прежний backend |

`winx_native_material_source.h` читает snapshot на текущем draw внутри native
mesh submission, **после** оригинальных material/controller/pass updates.
Начало `4BC670` для этого слишком раннее. Проверяются renderer, mesh, sequence,
thread, selected==installed и точные классы. Material override запрещён;
texture selectors `[1..8]` должны быть нулевыми, effective texture state —
совпадать с holder, shader coordinate override — отсутствовать. **C1C4 может
быть установлен**, если mode/source/selectors и остальные проверки проходят:
сам этот флаг не доказывает подмену потребляемых входов.

Arguments ограничены `TEXTURE/CURRENT`, result — `CURRENT`, следующая stage
отключена. Результат общего mapping сверяется с actual D3D operations, UV и
sampler. Несовпадение native-контракта сохраняет прежний D3D-source API путь;
неподдержанный общий D3D-контракт или ошибка API оставляет оригинальный draw.
Native-use credit начисляется только при успешной подаче. Отдельного material
address cache и копии игровой таблицы преобразований не добавлено. Общий helper
был выделен без изменения тела и проверен ранее в
[этапе resource pressure](winx-remix-resource-pressure-2026-09-13.md).

Интеграционный `Test-MaterialChannels.ps1 -Name native-material-v1` прошёл с
первой попытки: **541 проверки**, из них **137 native material** и **171 native
source**. Использованы настоящий system D3D9 device и recording Remix API;
это проверка обвязки, не исполнения native game factory. Source snapshot и EXE
сохранены в `local-data/rtx-remix/material-channel-tests/native-material-v1`.
Их production sources после запуска независимо совпали с рабочими файлами.

Проверены точная native/observer подача, равенство vertices/world/blend/sampler
и полных DDS mip bytes, один API draw, отказы scope/owner/sequence/thread,
классов/pass/layer/overrides, UV1/transform, source/effective lighting и actual
device state. Изменённые native sampler/alpha дошли до recording API. При
отказе API native-use не начислялся, fallback завершался, последующая подача
восстанавливалась. Fixture вернул D3D state и своё native состояние.

| Проверка в игре | Результат |
|---|---|
| Алфея, первый полный кадр 99 | 1050 matched, **871 native material instances** |
| Source A/B | Выключение записано на frame 335, включение на 407; кадры 336–407 используют D3D material source, на 408 native снова 871 |
| Домино, первый полный кадр 1332 | **446 native material instances** |
| Recorded totals всего запуска | 1 801 474 attempts/matched; **1 522 172 used**, 0 mismatches/rejections |
| Geometry | **743 upload generations**, 0 byte/layout errors; sampled material sources: 6189 native / 871 D3D |

Корректные A/B изображения — `alfea-material-native.png` и
`alfea-material-d3d.png`: просмотрены, внешний вид окружения сохранён, позы
персонажей различаются. `alfea-material-initial.png` — splash и не используется
как свидетельство Алфеи. Основные view/projection в двух state snapshots
совпадают; player X/Z те же, Y изменился с `25.87337875366211` до
`25.873394012451172`. Снимки сделаны при диалоге `[54,27,70]`, поэтому это
не pixel-identical кадры и не доказательство перемещения по Алфее.

Первая попытка Sweep в Домино сохранена как `test_interrupted`: после 32
коротких Enter диалог ещё владел вводом. Только длительность Enter для диалога
изменена `.08 → .25` секунды; ограничение попыток и проверки состояния
сохранены. Повторный переход `loaded_and_presenting` успешен. Дополнительное
движение подтверждено двумя snapshots в state 4: позиция Блум изменилась с
`[-1693.00305, -0.04013, 700.59906]` на
`[-920.16650, -269.01401, 340.68265]`; view изменился, projection сохранился.
На просмотренном `domino-material-motion.png` Блум находится у ледяного уступа
и воды. Это ограниченная проверка движения, не прохождение уровня.

**Поздняя фаза отделена от охвата мира.** На кадрах **2491–10189** остаются
только **2 native instances/frame**; в записанных light frames scenes=0 и
owned=0, main camera apply отсутствует. Получено **7699 консервативных пропусков
camera window**; 25 подробных записей содержат `missing_current_native_apply`.
Это согласуется с UI/выгрузкой, но точная причина не установлена. Сохранился
прежний D3D camera fallback. Эти 15 398 native material uses не включаются
в подтверждение полноты Алфеи/Домино. Histogram всего журнала: 871 — 1138
кадров, 446 — 1156, 2 — 7699, 0 — 73. Камера выполнила **2370 SetupCamera**
без API failures/state mismatches/late updates; утверждать «у камеры нет
дефектов» или полное GPU/temporal-history соответствие оснований нет.

Закрытые source logs material/geometry/camera не достигли лимитов. Старый
`native-source-analysis.json` с историческим текстом «material still D3D»
сохранён; для этого блока используется **native-source-analysis-v2.json**.
Отдельный общий `materials.jsonl` достиг собственного порога 16 MiB и не
объявляется полным журналом запуска.

Настройки `user.conf`, `winx.ini`, именно `.trex/bridge.conf` и все четыре
файла `Media/Saved` побайтно совпали с `.before` / `saves-before`.
Автоматический watcher восстановил bridge config; повторного восстановления
не выполнялось. Предыдущие adapter/canonical DLL сохранены в
`native-material-tests/install-v2`. EXE и исходные игровые ресурсы не менялись.
Wrapper/qualifier/analyzer snapshots сохранены при подготовке отчёта отдельно
от executed test source. Фактические renderer logs лежат в
`<run>/workdir/rtx-remix/logs`: обе стороны bridge сообщили успешный cleanup,
но осталось **41 common device object not disposed** и info об Opacity Micromap
budget exhaustion. Общий lifecycle этим этапом не закрыт.

Остаются material curves/installed colors для lit modes, UV COUNT2 и остальные
текстурные пути, отделение baked color от физического освещения. Историческая
[карта material frontier](winx-remix-native-material-frontier-2026-09-13.md)
сохранена без переписывания. Следующий законченный блок по
[этапу 3 плана](winx-remix-direct-scene-plan-2026-09-12.md) — свежий static
ownership snapshot и сопоставление support → Model → mesh/material/world на
одной операции, с сохранением producer updates и событиями жизни сцены/root.
CP12–13 подтверждают ownership, но visibility stamp не является Hidden или
generation. До этой проверки расширение D3D visibility vector остаётся;
кэш последнего видимого объекта не вводится.
