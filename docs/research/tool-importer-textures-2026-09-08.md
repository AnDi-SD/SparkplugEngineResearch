# CP130 — новые текстуры Importer и порядок нативных ссылок

Importer worktree 0.6.1 использует структурные `spTextureData` fields для
проверенных PC BGRA ресурсов. Исправлены реальные отказы импорта; при этом
проверка выявила отдельную регрессию skinned-пути. Измеритель становится
**15/16 (93,75%)**, `import.skin` временно `partial`. Исторический результат
16/16 не переписывается, знаменатель остаётся прежним.

## Исправленные ошибки

Два writer-пути ещё проверяли legacy signature `0x32E3/0x43E3` и использовали
фиксированные offsets. Часть signature является размером блока: законная
текстура после resize переставала подходить. Compact header также сдвигал
позиции данных. Лишнее условие площади, кратной 64 пикселям, отклоняло 17×9.

Теперь `SmoTextureDataWriter.BuildReplacementObjectBgra` возвращает один
структурно переписанный leaf для graph import. Полный SMO ради промежуточной
текстуры не копируется. Старый `ReplaceBgra` использует тот же код и сохраняет
проверку полного результата. Branch writer вызывает общий путь Importer.
Несколько mip, вторая representation и неподтверждённый формат явно отклоняются.

Новые ресурсы без подходящего native template создаёт `CreateBgraObject`:
реальный class ID `0x78EA082B`, embedded field3, source-none base, platform6,
native field1 и один field0 BGRA mip. Формат0 сохраняет alpha, dimensions/stride
согласованы с пикселями. Legacy template исходного файла не изменяется;
новый ресурс получает подтверждённую PC-форму. Это также устраняет ошибочную
запись pixel size4 вместо pixel format в прежний cross-platform header.

Отдельный исходный DX reader на старой bare legacy-обёртке вернул1, но записал
ошибку чтения и не создал pixel surface. В отчёте это **accepted=false**;
`status=passed` означает только совпадение с ожидаемым диагностическим отказом.
Три новых leaf прошли оригинальный reader с точными BGRA: compact17×9,
созданные17×9 и8×8. Это stream/COM fixture, не реальный GPU.

Полный статический импорт выявил следующий дефект: writer добавлял inline
TextureData после первой reference-only ссылки на тот же ID. C# catalog reader
принимал файл, portable reader отказал, оригинальный loader остановился с
ошибками stream/невалидным fetch. Теперь inline-данные разворачиваются на месте
первой ссылки. Проверка перед установкой output отдельно запрещает обращение
к импортированной текстуре до её inline-определения.

## Проверки

| Проверка | Результат |
|---|---|
| Baseline двух template writers | 31 отказ из44 checks; исходный отчёт сохранён |
| Итоговые template/canonical пути | 50 passed +9 guard checks, 0,374 с, около44 МиБ |
| Выборка | Icy, staff_projectile, Cloud01/chest; 5 исходных texture entries, resize1×1/13×7, compact header, выход8×8/17×9 |
| TextureTool | 4 248 checks; все32 SMO побайтно совпали с CP125 |
| Статический public writer | Старый тест двух разных материалов прошёл; сохранённые proofs с1/2 частями и общей17×9-текстурой прошли |
| Native whole file, 1 часть | Все7 объектов,69 checks, точное portable/native state,1,631 с;132 allocations освобождены |
| Native whole file, 2 части | Все11 объектов,1,814 с;167 allocations освобождены; native-only capture, без заявления portable comparison |
| Native whole file, 2 разных текстуры | Все12 объектов,1,930 с;209 allocations освобождены; native-only capture |
| Старый сломанный output | Новая проверка порядка ссылок отклоняет его до установки |

Native whole load использует прежний file profile:131072 байта arena,
до1 млн инструкций/8 с на вызов,30 с на отдельный процесс. Prepared RTTI/startup
и COM inputs явно заданы; реальные loader, serializers, mesh/texture creation
и destructors исполняются. Нативные данные источников остаются локальными.
Повторный полный corpus scan, неизменённые SAN/FBX/GPU tests не выполнялись.
После совпадения32 output SHA повтор старых529 native assertions не требовался.

## Новая проблема, не скрытая процентом

Реальный `--material-group-native-integration` отказал для Bloom_body и Tecna.
Адресный [survey](../../research/ImporterGraphProbe/Program.cs) показывает:
у Bloom_body один rigid `spModel` mesh под Head; у Tecna5, Flora5 и Icy10
skinned meshes не получают binding из-за `AMBIGUOUS_SKIN_MATERIAL_INHERITANCE`.
Эти числа описывают отсутствие bindings, а не количество доказанных ошибок
нативного движка. Для переноса состояния материала требуется восстановленный
порядок renderables; произвольный выбор ближайшей текстуры не добавлялся.

Исторические gate4/gate7 остаются свидетельством прежних запусков, но уже не
достаточны для текущего `import.skin`. Эта операция получает `partial` до
адресного исправления и проверки. Статический импорт подтверждён текущими
полными native proofs. Следующий этап цикла посвящён этим текущим потребителям
`spRenderNode`, `spSkin` и `spModel`, без восстановления целых классов.

## Повторение и привязка evidence

```powershell
dotnet run --project tools/SmoImporter/SmoImporter.FormatTests --no-restore -- --texture-template-regression OUTPUT ICY_SMO STAFF_PROJECTILE_SMO CHEST_SMO
dotnet run --project tools/SmoImporter/SmoImporter.FormatTests --no-restore -- --texture-static-integration ROCK_SMO NEW_OUTPUT_SMO 2
python research/probe_pc_imported_texture.py OUTPUT_SMO TEXTURE_INDEX accepted REPORT_JSON
python research/probe_pc_importer_scene.py SMALL_STATIC_OUTPUT_SMO REPORT_JSON
```

[Validation manifest](../../research/tool-importer-textures-validation-2026-09-08.json)
фиксирует источники, отчёты и SHA; [assessment CP130](../../research/tool-readiness-assessment-2026-09-08-cp130.json)
сохраняет частичную готовность. Первоначальная test-harness проверка трактовала
cross `AuxiliaryValue` как raw третье слово; baseline-v2 исправляет само ожидание
на `FormatValue`. Неудачный draft mip fixture также исправлен до итоговых9 guards.
Эти ошибки стенда не считаются исправлениями приложения.
