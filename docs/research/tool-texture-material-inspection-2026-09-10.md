# Общий material reader в TextureTool

`TryReadMaterialPath` и его массивный UInt32 parser удалены. TextureTool получает
actual material states и все pass/layer из публичной проекции уже существующего
cached `SmoMaterialInspection`. Новый parser, C# defaults и новый native reader
не добавлялись. Восстановленный `spMaterialSerializer` уже читает field8/17 и
задаёт реальные состояния holder; его код этим блоком не менялся.

В старом TextureTool `SFX/book.smo` с legacy field8 получал пустой LTS-массив.
Девять getters, включая `ColorOperation`, выбрасывали исключение. `MaterialDetails`
вызывается привязкой Avalonia для каждой строки, поэтому отображение этих
сведений было неисправно; падение всего GUI-процесса не утверждается. Теперь
материал получает 11 render states и 9 texture states из общего класса,
включая исходные defaults при отсутствии authored LTS. Значение StatesField=-1
сохраняет различие между defaults и записанным field8/17.

Generic `SmoObjectFieldReader` оставлен только для точного расположения field10.
Его absolute payload offset и размер строго сопоставляются с последней
texture reference, наблюдаемой actual reader. Сохранённый orphan или
неоднозначный owner не назначается текущим материалом: `Material=null`, причина
отдельно в `MaterialIssue`. Она выводится существующей строкой сведений и не
подменяет `ReplacementIssue` или политику возможности замены текстуры.
UI indices преобразуются в прежние 1-based только при создании DTO.

Запись legacy/orphan/repeated LTS и FAT/envelope writer не менялись; их
согласование этим read-only переносом не подразумевается.

## Проверка

Один итоговый test binary запускался с сохранённым старым TextureTool Core DLL
и затем с новым; Viewer Core/native DLL оставались одинаковыми. Старый DLL
временно подставлялся только в тестовый bin и восстановлен через finally.
Оба запуска фиксируют hashes фактически загруженных assemblies.

- Исходные book и Bloom_body плюс один synthetic book без field8 конкретного
  владельца: **70 checks**, до переноса 8 failures, после 0.
- Сравниваются реальные 11/9 states, девять getters, pass/layer, reference
  extents и исходные raw words. Все исходные файлы и массивы неизменны.
- Первоначальный synthetic test ошибочно искал единственный field8 во всём
  book. Выбор исправлен на владельца конкретной текстуры; первые отчёты с
  этим тестовым отказом сохранены, production ради теста не менялся.
- Focused preview: **71 checks**, три PNG совпали побайтно с результатом до
  переноса header/material readers.
- Отдельный final-only guard: **14 checks**. Общий mutation добавляет NULL
  assignment после прежнего field10; native reader сохраняет последнюю ссылку,
  исходная TextureData остаётся побайтно неизменной и получает явный
  `MATERIAL_BINDING_NOT_CURRENT`. Возможности замены текстуры не изменились.
  Этот метод с новым `MaterialIssue` не входит в запуск со старым Core DLL.
- FormatTests и Viewer builds: ноль warnings/errors. TextureTool GUI build:
  ноль errors, два CS9057 о более новой версии компилятора Avalonia analyzers.
  Обновление toolchain и визуальный GUI acceptance сюда не включены.

Исходники и локальные проверки:
[manifest](../../research/tools-core-texture-material-block-2026-09-10.json).
Закрыта описанная операция чтения, всё ядро TextureTool пока не объявлено готовым.
