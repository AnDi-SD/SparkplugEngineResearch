# Проверка явного комплекта runtime

`scripts/runtime/validate_runtime_cohort.py` проверяет собственный manifest
перед будущим запуском PC-игры. Он читает только перечисленные входы, source
includes и записанные сведения сборки. Не запускает процессы, не загружает DLL,
не скачивает зависимости, не копирует и не изменяет входные файлы.

```powershell
python -X utf8 -B tools/WinxRemix/scripts/runtime/validate_runtime_cohort.py --manifest <workspace-relative-cohort.json>
python -X utf8 -B -m unittest discover -s tools/WinxRemix/tests/runtime -p test_validate_runtime_cohort.py
```

Выход — JSON в stdout, код0 при `COHORT_BYTES_AND_CONTRACT_MATCH`, код1 с причиной
при отказе. Проверка означает совпадение заявленных bytes и связей manifests.
Она не подтверждает фактическое исполнение compiler, совместимость DLL,
изображение, производительность или полный capture/replacement цикл.

## Manifest

Корневые поля: `schema:1`, `name` (буквы ASCII, цифры, `_`, `-`),
`backend:"system"|"remix"`, `apiVersion:[major,minor,patch]`, `files` и `build`.
Каждый элемент `files` содержит уникальный `id`, `role`, `path`, `bytes`,
`sha256`; PE-входы дополнительно содержат `machine:"x86"|"x64"`.
Hash — SHA256 исходных bytes. Совпадающие case-insensitive пути отклоняются.

Для обоих backend нужны одиночные роли `game`, `adapter`, `d3d8to9`,
`launchDriver`, `dependencyLock`, `generatedDependencies`, `apiHeader`,
`adapterBuild`, `toolchain` и хотя бы один `config`. Первые три PE имеют x86.
Все используемые конфигурации и launcher должны быть перечислены явно;
инструмент не знает, что caller забыл дополнительную configuration dependency.

Remix дополнительно требует x86 `client`, x64 `server`, x64 `renderer`,
`runtimeProvenance`, а также `patchManifest` и `patch`. В system-комплекте
перечисление Remix client/server/renderer запрещено. Это исключает неявную
подмену объявленного режима, но не анализирует поведение launcher.

`build` содержит:

```json
{
  "sourceRoot": "<frozen-snapshot>/tools/WinxRemix",
  "entryFile": "probe-source-id",
  "exportsFile": "probe-def-id",
  "manifestFile": "adapter-build-manifest-id",
  "toolchainFile": "toolchain-receipt-id",
  "includeRoots": ["<frozen-snapshot>/tools/WinxRemix", "<generated-include-root>"]
}
```

Поля с суффиксом `File` ссылаются на `files[].id`. Entry и exports имеют роль
`source`. Snapshot готовят существующим `Copy-WinxRemixSourceSnapshot` из
`tests/Copy-SourceSnapshot.ps1`, сборку — `Build-Probe.ps1 -SourceDirectory`.
Build manifest должен связать те же entry/exports/DLL hashes и snapshot path;
для нового cohort требуется `exportsFromSourceSnapshot=true`.

Все literal quoted includes от entry разрешаются сначала относительно
текущего header, затем по `includeRoots`, и должны присутствовать в inventory.
Это консервативная проверка, без вычисления preprocessor conditions. Она не
считает literal include полным compiler dependency graph. Фактически записанный
`/showIncludes` проверяется отдельно: каждая строка с объявленным prefix
должна разрешаться к inventoried `source`, `generatedHeader`, `apiHeader` или
`sdkHeader`. `includeLog` имеет UTF-8; `toolchain.includeLogPrefix` допускает
`Note: including file:` (по умолчанию) или фактически наблюдавшийся
`Примечание: включение файла:`. `VSLANG=1033` не гарантирует английский вывод
установленного compiler. Пустой log и неизвестный prefix отклоняются.
При транскодировании сохраняют отдельный `includeLogRawFile` с ролью
`includeLogRaw` и `includeLogRawEncoding` (`utf-16`, `utf-16-le`, `utf-8-sig`, `cp866`).
Validator требует точное совпадение декодированного raw с UTF-8 artifact;
переписывание языка либо переводов строк не принимается.

## Toolchain и pins

Toolchain receipt имеет `schema:1`, `target:"x86"`, `recordedAtBuild:true`,
`adapterSha256`, `sourceSha256`, `remixRevision` и ссылки `compilerFile`,
`linkerFile`, `environmentFile`, `commandFile`, `includeLogFile`. Их роли:
`compiler`, `linker`, `environment`, `buildCommand`, `includeLog` соответственно.
В `environment` записывают непустые `VCToolsVersion`, `VCToolsInstallDir`,
`WindowsSDKVersion`, `WindowsSdkDir`, `UniversalCRTSdkDir` фактической сборки.
Неизвестное историческое окружение нельзя заполнить данными новой установки.

Обычные пути находятся внутри workspace. Вне него допускаются только явные
`cl.exe`, `link.exe`, `vcvarsall.bat`, связанные с receipt, и `sdkHeader` внутри
записанных VC/SDK roots. Проверяются bytes, а не только version strings.
Receipt — свидетельство caller; validator не восстанавливает историю сборки.

`dependencyLock` — `dependencies.json`, `generatedDependencies` — результат
`Prepare-Dependencies.ps1`, `apiHeader` — фактический `remix_c.h` сборки.
Ревизия, hash lock, API version и generated hashes должны согласовываться.
Нужны обе `generatedHeader`: `xxhash.h` и `remix_light_conversion.h`.

Remix `runtimeProvenance` содержит `remixRevision`, `binaries` с hash ключей
`client`, `server`, `renderer`, `patchManifestFiles` и `patchFiles` с полными
наборами соответствующих file IDs. Base pin manifests должен совпасть;
если manifest задаёт `patchSha256`, эти patch bytes должны быть перечислены.
Порядок применения, результирующие upstream files и фактическое происхождение
исторических бинарников проверяются отдельными build/patch proofs.

## Пределы и повторная проверка

Не более4096 файлов,512МиБ на файл и768МиБ суммарно; JSON, текст source parser
и include log ограничены4МиБ. Hash считается потоково. Изменение размера или
mtime открытого файла во время чтения отклоняется. Это последовательная
проверка, не атомарный snapshot всего дерева: выполнять заново непосредственно
перед запуском и сохранять manifest/result hashes рядом с его evidence.

Смена DLL, source, config, dependency либо toolchain создаёт новый cohort.
Принятый результат не переносится на изменённый комплект.

[Устройство адаптера](development.md) · [Владение процессами](process-lifetime.md)
