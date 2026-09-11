# Общая подготовка проходов материала

Блок 7 цикла 11 сентября: CPU-подготовка результатов для современного backend.
Восстановленные игровые классы не изменены; полного GPU parity этот блок не заявляет.

`tools/SparkplugViewer.Native/MaterialSubmission` владеет общей resource graph,
PC default material и отдельным renderer cache. Он вызывает существующие
`InitializePCSubmissionCachesForAnalysis`, `InstallMaterialForAnalysis`,
`ApplyMaterialStateSetForAnalysis`, actual pass Update, UV conversion,
`ApplyPassTextureStatesForAnalysis` и `ApplyPassBlendForAnalysis`.
Таблицы переводов, анимация, pass defaults и blend mutation не переписаны в C#.

Оригинальный порядок сохраняется: installation копирует цвета **до** color update;
pass обновляет textures/UV; последний blend записывается в сам material. Состояние
между вызовами сохраняется, поэтому подавленные cache callbacks не теряют вывод.
Возвращаются все проходы, 16 нужных mapped render states, 8 texture stages,
канонические texture IDs и actual UV matrices. Known masks отличают нетронутые
регистры от записанного нуля. DTO — transport, не формат файла или оригинальный ABI.

Host границы явные: draw order/frame supplies caller; cache context отдельный от
game startup. Начальные diffuse/ambient device sources заданы как 1/0 в соответствии
с восстановленными raw caches 11/10; неизвестные остальные регистры не дополнены
выдуманными defaults. Lighting payload context имеет явно нулевой packed color и
opaque-black global input. Light selection, shader generation и геометрия этим
адаптером не выполняются. Palette callback не вызывает устройство: общий texture
loader уже подготовил BGRA upload. Используются loaded PC DXTexture и known power;
прочие ресурсы явно отклоняются.

C ABI передаёт 1044 bytes на pass, максимум 8. Ошибочный buffer/ID/capacity
отклоняется до мутации; outputs публикуются только после подготовки всех passes.
Ошибка actual controller/pass может сохранить уже сделанные изменения graph/cache:
rollback не обещается. Context удерживает graph после удаления внешнего handle.
`SparkplugMaterialRuntime.CaptureDraw` делает только interop/DTO projection;
`SparkplugSceneRuntime` освобождает material context перед graph.

## Проверка

Четыре настоящих PC SMO: Icy, igmenu_opt_pc, Alfea01 и Alfea02 — **2184 материала,
2236 проходов**. Проверены canonical updated textures, known states, наличие
producer для активного UV, цвета до анимационного обновления, final blend mutation,
повторное использование cache и обратный порядок вызовов. Всего 73460 проверок,
большая часть — обход заполненных DTO, а не новые независимые игровые доказательства.
Native RendererSubmit/MaterialController/MaterialColor: 3/3, 9,77 s.
FormatTests Release: 0 warnings/errors. C ABI: 22 boundary/lifetime checks.

В двух первых Alfea02 запусках новый тест ошибочно требовал равенства **отключённых**
UV registers: поздний проход законно оставлял свою матрицу. Исправлено ожидание;
production code не менялся. Первая C ABI проверка остановилась на Python
`bytes(list[ctypes.Structure])`; исправлен срез исходного byte buffer, свежий запуск
прошёл. Это ошибки нового harness, не нештатный результат игры.

Исходные native slices имеют independent original PC evidence:
[installation](native-pc-material-install.md),
[lighting state](native-pc-material-lighting.md),
[pass states](native-pc-material-pass-states.md),
[default material](tool-pc-renderer-default-material-2026-09-10.md).
Повторно выполнять protected initialization каждого файла для DTO не требовалось.
EXE scores не изменяются. Следующий шаг — использовать этот вывод в общем OpenGL
backend и проверить реальные pixels. Ядра всех приложений ещё не объявляются готовыми.
