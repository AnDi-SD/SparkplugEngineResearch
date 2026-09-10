# Общий reference reader в диагностике placement — 10 сентября 2026

В `SmoSharedPlacementCloner.ResolveReferenceIds` два ручных чтения UInt32
заменены существующим `SmoNodeDecoder.TryDecodeRelationship`. Этот метод
составляет `field0=...` в сообщении о неудачной проверке
`CloneNodeModelPlacement`; он не выбирает ресурсы для успешной записи.

Сохранены selection по field type, payload ровно 8 bytes, nonzero ID,
reference-only / inline size 0, исходный порядок и повторы. Три проверки ID
перед переназначением физических inline prefixes, числовые записи ID,
физические границы и authoring policy не изменены. Новый helper не создан.

Используется уже подтверждённый [общий prefix contract](tool-reference-prefix-shared-core-2026-09-09.md):
null содержит только четыре bytes, nonnull добавляет size word; metadata
инспекция не разрешает target и не доказывает успешную загрузку.
Это продолжение [узкого Importer prefix-переноса](tool-reference-inspector-cached-texture-2026-09-10.md).

Root выполнил только контрольную сборку:

```powershell
dotnet build tools/SmoImporter/SmoImporter.Core/SmoImporter.Core.csproj -c Release --no-restore -p:SkipSparkplugNativeBuild=true -m:1 --nologo -v:q
```

**PASS: 18,57 с, warnings 0 / errors 0.** Лог:
`local-data/results/tools-core-cycle-20260910-0730/bone-slot-owner/placement-diagnostic-build.log`.
Native DLL `F422A92E84008E837D402B6EB6B621B743D4E2F49BD0CC7E543B0EDAE2F95028`
сохранена без пересборки. Общий игровой код/API не менялся.

Новый runtime-прогон пяти вариантов, повтор 70 Text checks и полный clone
сценарий **не выполнялись**. Проверка пяти вариантов была только сопоставлением
неизменного prefix contract с прежним predicate. Этот build-only срез не
получает новый runtime credit. `git diff --check` прошёл.

[Компактный manifest](../../research/tools-core-placement-diagnostic-reference-2026-09-10.json)
сохраняет команду, результат и hashes source/API/log.
