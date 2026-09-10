# PC MaterialColorController: factory и начальное состояние

Один fresh original `41A580` успешно создал `spMaterialColorController`:
allocation480B, vtable `6DEBD4`. Это снимает прежнюю границу factory для общего
resource reader. Clone/copy этим результатом не восстановлены.

## Original execution

Использован прежний `ReaderFixture` с64KiB heap и actual Animations prerequisite
`454640`, как в PC ColorFunc evidence. Профиль character4M/16s выбран до создания
гостя; child30s, один worker. Original constructor, embedded leaves и защита не
подменялись. PC globals не заменялись PS2 defaults. Factory использовал только
прежний allocator seam; heap занял688B, включая fixture storage.

Ctor `437550` разрешился в `13B90E0→A0D3E0`. Lookup нашёл actual record628;
естественный XOR byte-loop завершился на инструкции2,275,233. Расшифрованный
диапазон `13B90E8`,276B, имеет SHA256
`A615EF2D9C2D3B26BB396B0F11DF9D879F78AE9D95C77EE4CF44DA402290E730`.
Ему предшествует FS-prefix64 в `13B90E7`; отдельно сохранено естественное окно
constructor с полным прологом. Точный factory return:2,289,772 инструкции,
5.88s в run2. Base `423150` посещён1 раз, ColorFunc ctor `42FA60`4 раза,
FunctionEval ctor `478620`5 раз.

| Состояние actual объекта | Результат |
|---|---|
| enabled+10 / clocks+1C,+20 | true /0,0 |
| material+24 | NULL |
| saved RGBA+28,+38,+48,+58 | четыре `(0,0,0,1)` |
| ColorFunc+68,+B8,+108,+158 | обе endpoint `FF000000` |
| Пять FunctionEval | time0; frequency/reciprocal/amplitude1; offsets/pitch/clamp0; type0 |
| Original writes |442B записаны,38B padding остались исходнымиCC |

## Ownership и исправление harness ABI

Run1 сохранил успешный factory, затем вызвал `4372E0` с лишним аргументом.
Функция вернулась, но harness отклонил ESP: это nondeleting body
`460520…460698` с `ret0`. Отчёт ошибки сохранён без исправления задним числом.

Actual vtable[0] — `437610`: вызывает `4372E0`, при flags&1 освобождает `this`
через `412420`, затем делает `ret4`. Разрешённый fresh run2 выполнил именно этот
wrapper с flags1:6,792 инструкции, успешный return и ровно одно освобождение
controller480B. Object bytes, write coverage и decrypted range совпали с run1.
Destructor удалил controller из Animations;44B manager и52B его списка остались
учтёнными внешними prerequisite allocations до уничтожения гостя. Дополнительных
allocations для embedded leaves нет. Это default/unbound case; teardown
произвольно связанного material не проверялся этой пробой.

## Reconstruction correction

Прежний `spMaterialColorController::saved_` был объявленным consumer input и
содержал четыре alpha0. Original stores `13B9129`, `13B912C`, `13B9138`,
`13B9144` явно записывают alpha1: это доказанное расхождение реконструкции.
Общий класс теперь сохраняет эти defaults и предоставляет RTTI factory;
existing ColorFunc/FunctionEval, base lifetime и serializer используются без
второй реализации. ResourceGraph уже регистрирует этот target/serializer.
Отказ отдельных clone/copy сохраняется.

Адресные `spMaterialColorTests` проверяют factory, все semantic defaults,
empty-section codec, регистрацию/удаление controller и прежний clone refusal.
Общий source проверен native сборкой и четырьмя suites: MaterialColor 235/235
(0.91s), MaterialSerialization 653/653 (0.98s), FullLoader 213/213 (1.14s),
OcclusionTopology 149/149 (0.68s). CTest: 4/4 PASS, общее время 5.37s.
Отдельный whole-graph acceptance на реальном SMO приведён ниже.

Точная команда root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -Command "& 'tools/SparkplugViewer.Native/Build-Native.ps1' -Configuration Release -RunChecks -CheckSuites @('MaterialColor','MaterialSerialization','OcclusionTopology','FullLoader') -BuildWorkers 1"
```

Проверенная DLL имеет SHA256
`939B42687CE8806F910334D17B19AB3917769F08A867D040F6041B53A3456D97`.
[Build log](../../local-data/results/tools-core-cycle-20260910-0730/material-preview/fallback-material/shared-color-shape-native-build.log)
и [CTest log](../../local-data/results/tools-core-cycle-20260910-0730/material-preview/fallback-material/shared-color-shape-ctest.log)
сохранены вместе с evidence; source и logs закреплены хэшами в manifest.

## Whole-graph acceptance

Один pristine `Media/Characters/Knut/lightbeam_projectile.smo`, 5,584B, SHA256
`7BEA3AF6DC3643BBA1CED3E72B61743EB12F77EE9ACCDCC6C86D092707D53482`,
проверен через тот же C ABI без изменения исходного файла или P/Invoke ABI.

До изменения DLL `F422A92E84008E837D402B6EB6B621B743D4E2F49BD0CC7E543B0EDAE2F95028`
отказывала с `Inline object header or factory failed`. Проверенная новая DLL
`939B…6D97` загрузила все 11 objects / 6 nodes / root ID1. Material ID5 содержит
controller ID6 с class `4C633E85`, clocks0/0, enabled1. После чтения связей
`graph_destroy` завершился: `destroy_completed=true`.

[Before](../../local-data/results/tools-core-cycle-20260910-0730/material-preview/fallback-material/color-factory-graph-before.json),
[after](../../local-data/results/tools-core-cycle-20260910-0730/material-preview/fallback-material/color-factory-graph-after.json)
и [проверочный script](../../local-data/results/tools-core-cycle-20260910-0730/material-preview/fallback-material/validate_color_factory_graph.py)
сохранены. Единственные измерения загрузки — 0.0134165s до и 0.0007907s после;
это acceptance, не замер выигрыша производительности. Renderer, оригинальный
game process, clone/copy и весь corpus в этой проверке не выполнялись.

[Manifest](../../research/tools-core-pc-material-color-factory-2026-09-10.json)
содержит hashes неизменённых локальных probes/captures и проверенного source.
[Run2 proof](../../local-data/results/tools-core-cycle-20260910-0730/material-preview/fallback-material/material-color-factory-run2-proof.json)
и [run1 ABI diagnosis](../../local-data/results/tools-core-cycle-20260910-0730/material-preview/fallback-material/material-color-factory-analysis.json)
остаются в local-data; игровые bytes в Git не добавлены.
