# Asset ID внешней API-геометрии

Собственный патч `external-asset-identity-v2.patch` для revision
`b81a7b566b1eeb9edb4dc2b3c9d3972e0f253ad4` отделяет ID захвата и поиска замен
от внутренних geometry hashes и истории экземпляров. Это изменение renderer;
публичный C API и bridge wire не меняются. Восстановленные исходники игры
не затрагиваются.

## Контракт

`rtx.enableExternalAssetIdentity = True` задают **до создания mesh**. По умолчанию
настройка выключена. Caller обязуется передавать постоянные ненулевые 64-битные
content IDs: `MeshInfo.hash` обозначает одну геометрию с неизменяемой привязкой
материала, `MaterialInfo.hash` — её материал. Изменение содержимого или привязки
требует нового ID. Адреса и последовательные client bridge handles для этого
не подходят. На стороне server используется исходный material hash, сохранённый
в 64-битном external handle.

В первой версии поддержана одна поверхность без Skin, с существующим исходным
Opaque-материалом, без particle descriptor и GPU instancing. Неполная пара ID
или неподдержанный draw возвращает прежний external-путь. Multi-surface ordinal
и Skin этим протоколом не определены. Обычный WinxRemix launcher настройку
не включает: legacy Surface/indexed/Skin ещё используют другую схему ключей.
Включение допускается только для контролируемого caller с указанным контрактом.

`RasterGeometry` хранит отдельную пару asset ID. Внутренние `GeometryHashes`,
BLAS cache, anti-culling и external instance matching сохраняют прежние ключи.
`RtInstance` хранит оба capture ID отдельно для каждого экземпляра. Общий BLAS
может сохранять admission первого draw кадра, поэтому брать из него явный mesh
asset ID нельзя. Нулевой mesh asset ID сохраняет прежнее именование по внутреннему
BLAS hash; неподдержанный экземпляр не заимствует ID поддержанного соседа.
Исходный материал из BLAS по-прежнему извлекается по renderer content hash. Capturer именует mesh,
material, привязки и texture assets по соответствующим capture ID, используя
существующий [экспорт внешнего Opaque](../renderer-usd-fix/README.md).
Capture сохраняет исходный материал; применение material mod не меняет его
на материал замены в исходном захвате.
В закреплённой x64 Release-сборке два новых поля увеличивают `RtInstance`
с 768 до 784 байт. Точная проверка размера сохранена; copy constructor и
`updateFromReference` переносят оба ID через `copyInstanceDataFrom`.

Версия v2 заменяет v1 целиком и применяется к той же базе с четырьмя USD-патчами.
Для смешанных draw одного общего BLAS версия v1 не подходит. Её patch и
`manifest-v1.json` сохраняются отдельно; поверх неё v2 не накладывается.

Для допущенного external draw поиск mesh/material mod использует ту же пару ID.
История движения остаётся у прежнего external `ReplacementInstance`.
Материал Opaque mod наследует неавторские поля исходного API-материала.
Mesh mod ограничен пустым набором (скрыть исходник) либо одной mesh/includeOriginal
записью без Skin, particles и instancing. Неподдержанный mod явно отклоняется
с предупреждением и выводом исходника. Удаление mod возвращает исходную geometry.
Multi-prim, lights, graphs и другие типы material mod здесь не квалифицированы.

Замена geometry получает её собственные поля; `includeOriginal` сохраняет asset ID
исходника. Geometry category tags и UI `associatedGeometryHash` продолжают
использовать прежние renderer hashes. Их перевод на asset ID — отдельный контракт.
Смена допущенного/недопущенного режима одного живого экземпляра во время
многокадрового захвата не квалифицирована.

## Применение

Работают в отдельном checkout указанного revision. Сначала применяют
[USD stack](../renderer-usd-fix/README.md) в порядке `api-usd-v1.patch`,
`animation-v1.patch`, `buffers-v1.patch`, `zero-scale-v1.patch`, затем этот патч.
[Manifest](manifest.json) фиксирует patch SHA256 и LF-хеши до/после каждого
затронутого файла. Скрипт откажется от другой версии или изменённой базы;
совместимость с остальными renderer patches нужно проверять отдельно.

```powershell
python tools/WinxRemix/scripts/patches/renderer-resource-identity/apply_patch.py --source <renderer-source> --check
python tools/WinxRemix/scripts/patches/renderer-resource-identity/apply_patch.py --source <renderer-source> --apply
```

Скрипт не собирает, не устанавливает renderer и не включает настройку в игре.
После применения нужны сборка и отдельная квалификация runtime.
Изменение layout `RasterGeometry` и `RtInstance` требует свежей пересборки всех
зависимых единиц компиляции и их PCH. Старые объекты и статические библиотеки
с прежним layout нельзя подмешивать к новым файлам. Проверка отдельных C++-файлов
через `/Y- /Zs` проверяет семантику без прежнего PCH, но не создаёт объекты
и не заменяет компоновку DLL.

## Проверка

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/tests/patches/renderer-resource-identity/Test-ExternalAssetIdentity.ps1 -RendererSource <patched-renderer-source> -Name fresh-external-identity-x86 -Platform x86
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/tests/patches/renderer-resource-identity/Test-ExternalAssetIdentity.ps1 -RendererSource <patched-renderer-source> -Name fresh-external-identity-x64 -Platform x64
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/tests/patches/renderer-resource-identity/Test-ExternalAssetIdentity.ps1 -RendererSource <patched-renderer-source> -Name fresh-external-capture-x64 -Platform x64 -Suite Capture
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/tests/patches/renderer-resource-identity/Test-ExternalAssetIdentity.ps1 -RendererSource <patched-renderer-source> -Name fresh-external-draw-x64 -Platform x64 -Suite Draw
```

`Identity` компилирует настоящий применённый std-only header. `Capture` извлекает
настоящие `newInstance`, `captureMaterial`, getters и assignment/copy seams;
`Draw` — настоящую `submitExternalDraw`. Функции не переписываются; renderer,
GPU, mod loader и USD collaborators заменены recording типами. Тела и их исходные
файлы сохраняются с SHA256. Проверяются 64-битные ID, restart/order, парные fallback,
capture naming, внутренний material key, mod lookup, hide/removal и admission.
`Capture` также проверяет два экземпляра с общим BLAS и разным admission в обоих
порядках исходного draw и обхода capture, копирование ID и прежнее legacy naming.
Требуются
MSVC и 512 МиБ свободной памяти; CPU-процесс ограничен 10 секундами.
Это проверка политики и recording fixtures. Она не заменяет сборку production
renderer с USD, GPU capture, открытие USD в Toolkit и повторный запуск игры
с применённой заменой на том же runtime. Этот полный цикл остаётся обязательным.

На production renderer отдельно квалифицирован однокадровый GPU capture
контролируемой rigid Opaque geometry без текстур и чтение полученного USD через
USD SDK. После перезапуска с другим порядком создания ресурсов и изменившимися
bridge handles сохраняются имена mesh/material по постоянным ID, массивы вершин,
топология, нормали, UV, vertex RGBA, материальные коэффициенты и привязки.
Две позиции экземпляров соответствуют одному общему кадру caller. Это не
квалификация истории движения, текстур, применения mod, Toolkit или исходной
игры и Skin; обычный launcher по-прежнему не включает этот opt-in.

Для этого контролируемого захвата отдельно проверены создание проекта в реальном
Remix Toolkit, открытие workfile с capture-слоем, выбор replacement-слоя и
сохранение `mod.usda`. Через настоящий MCP выполнены импорт текстуры с успешной
валидацией, замена существующего diffuse-входа материала с `force=false` и
сохранение. После завершения Toolkit отдельный процесс USD SDK подтвердил
записанную относительную ссылку, содержимое и metadata DDS, а также обе привязки
API-экземпляров. Исходные ресурсы захвата сохранились.
Закрытие и повторное открытие изменённого проекта через Toolkit не квалифицированы:
команда закрытия отвергла несохранённые изменения. Применение материала или
геометрии в runtime, повторный запуск и удаление mod также требуют проверки.
