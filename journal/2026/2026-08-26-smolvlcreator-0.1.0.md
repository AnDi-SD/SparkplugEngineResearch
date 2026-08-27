# 2026-08-26 — SmoLVLcreator 0.1.0

## Статус

Минимальный план SmoLVLcreator завершён. Версия `0.1.0` зафиксирована как
release candidate редактора SMO-уровней для практической проверки на копиях
файлов Winx Club PC. Публикация во внешний GitHub Release и Git-тег на этом
этапе не создавались: сначала требуется пользовательский прогон собранного
архива.

## Граница выпуска

В выпуск вошли:

- общий GPU renderer и scene pipeline SmoViewer;
- инспектор, дерево уровня и каталоги ресурсов с превью;
- выбор, мультивыделение, камера и WORLD/LOCAL Move/Rotate/Scale gizmo;
- Undo/Redo и точный ввод позиции;
- просмотр, выбор, связывание, перемещение, генерация и удаление коллизий;
- отложенные изменения без промежуточного repack SMO;
- сохранение с проверкой, резервной копией и подробным логом;
- экспорт выделения через SmoExporter.Core;
- замена текстур и моделей через SmoImporter.Core и окно подгонки;
- добавление внешних rigid-моделей, drag-and-drop, размещение и удаление;
- встроенная нативная проверка результата кодом игры.
- рабочие Perspective/Orthographic, Lit/Unlit/Wireframe, Grid и Bounds;
- видимость/изоляция, точные rotation/scale и Copy/Paste/Duplicate;
- collision preview с бюджетом, padding, регенерацией и ручными связями;
- единый Core/GUI preflight импорта, RAW DATA-инспектор и встроенная справка.

После дополнительного прохода первоначальные режимы viewport,
видимость/изоляция, числовые rotation/scale и настраиваемый collision preview
реализованы. В `tools/SmoLVLcreator/ROADMAP.md` оставлены только задачи, требующие
пользовательских игровых тестов или новых подтверждённых сведений о формате.

## Версия и упаковка

- GUI assembly и пользовательская версия: `0.1.0`;
- стабильное имя приложения: `SmoLVLcreator.exe`;
- release profile: framework-dependent single-file, Windows x64;
- bootstrapper проверяет .NET 8 Desktop Runtime x64;
- общий native FBX runtime хранится один раз в каталоге `native/`;
- README, changelog, release notes и roadmap находятся в `docs/` пакета.

## Проверка

- `SmoLVLcreator.CoreTests`: 112 assertions на синтетических fixtures;
- полный regression-прогон `Alfea02_old.smo`: 271 assertions, включая замену
  составной `Muza_smallSpk02`, полную трансформацию node-owned `vase09`,
  сохранение, создание и нативное удаление коллизий, импортный контракт и
  повторный строгий разбор;
- `SmoViewer.FormatTests`: 198 assertions;
- автономные regressions SmoImporter: alpha-компоненты, material matching,
  normal repair, resource safety и texture catalog — успешно;
- `SmoExporter.FormatTests`: 4 native FBX path assertions и 39 assertions на
  полном GLB/OBJ/FBX round-trip `fish.smo`;
- Release-сборка GUI: успешно, 0 ошибок;
- `release-manifest.json`: корректный JSON;
- общий `Build-Releases.ps1` выполнил clean, restore, single-file publish,
  проверку корня пакета и проверку дубликатов;
- native `SmoFbxBridge.exe` собран в Release и включён в пакет.

Предупреждение `NU1900` при прямой сборке относилось только к недоступному
сетевому индексу уязвимостей NuGet; зависимости восстановлены из локального
кэша. Чистая release-упаковка завершилась без предупреждений и ошибок.

## Артефакт

| Архив | Размер | SHA-256 |
| --- | ---: | --- |
| `SmoLVLcreator-0.1.0-win-x64.zip` | 6 894 590 байт | `64140c74846675532b1a2252ecb4bd9fbda52559dc53e4196caed0892fabd57f` |

Локальный путь: `artifacts/release/current/SmoLVLcreator-0.1.0-win-x64.zip`.

## Исправление расхода памяти при сохранении

Финальная проверка выявила опасный рост памяти при добавлении внешней составной
модели в уже разросшийся уровень. Конвейер многократно клонировал весь SMO при
каждом внутреннем разборе и мог занять около 2 ГиБ, создавая системный memory
pressure вплоть до зависания компьютера.

- в общее ядро SmoViewer добавлен явный owned-buffer разбор без скрытой копии;
- вставка полей сразу собирает конечный контейнер без промежуточной копии data section;
- большие временные буферы освобождаются между частями составной модели;
- перед тяжёлой перепаковкой проверяется системный запас памяти; при нехватке
  сохранение безопасно прерывается до записи временного файла;
- в журнал сохранения добавлены показатели managed/working set/private memory.

Регрессия воспроизведена на `Alfea02.smo` размером 80 454 646 байт и восьмичастном
`Model.obj`. Полный цикл сохранения создал и повторно разобрал результат размером
80 536 478 байт с 4 295 объектами за 12,4 секунды. Пиковый working set снизился
примерно с 2,07 ГиБ до 1,19 ГиБ. Исходный файл при неудачной попытке не был
изменён: выполнение остановилось до `TEMP_WRITE` и атомарной установки.
# Project project serializer follow-up

The post-0.1 architecture now has an isolated `SmoLVLcreator.ProjectTool`
entry point. It introduces `.smolvlproj` as a ZIP project containing normalized
FFPS metadata plus the exact data section, and a streaming writer that creates
the FFPS header and object directory rather than patching the source container.
The initial no-edit gate is byte identity; both clean `Alfea02_old.smo` and the
current 80 MiB edited `Alfea02.smo` pass it. Production Save remains on the 0.1
isolated-worker pipeline until editable graph nodes and native game validation
pass the same regression corpus.

The next gate now stores a lossless description of every decoded direct field
and property operations keyed by object ID. A bottom-up layout planner writes
fixed-size transform slices without relocating anything and materializes absent
optional node fields while updating containing data-block headers, inline size
prefixes, ancestor intervals and directory offsets. Clean Alfea translation
changes only the two matrix ranges; a nested field-growth fixture reopens with
the requested transform. `SmoLVLcreator.CoreTests` passes 107 assertions. The
80,454,646-byte edited Alfea project was also imported and rebuilt byte-for-byte
with the new planner.

## Project native and structural graph gate

The contextual native validator originally timed out even though the game log
showed a successful load of the installed `Levels\Alfea\Alfea02.smo`. This
level bypasses `BuildAssetPath` and passes an already expanded absolute path
directly to `ResourceLoad`. The shared validator now recognizes that route,
replaces the direct argument in place and keeps the FFPS/load checkpoints in
the correct target context. Its suite passes 213 assertions.

Pristine `WinxClub.exe` accepted the untouched project rebuild, a fixed-size
placement translation and a size-changing optional-field materialization. Each
case passed FFPS magic/version, returned a non-null resource and survived the
contextual observation window. Results are under
`artifacts/experimental/SmoLVLcreator-native-20260826`.

The project operation journal now also supports removal of an exact inline
branch and relocation of a shared inline leaf. Root registry references are
removed with the deleted entity; references to descendant resources instead
block deletion until one consumer becomes the new physical owner. The resource
keeps its object ID, so all other consumers remain unchanged. Both operations
leave the imported `data.bin` immutable and are compiled together into one new
data stream and catalog.

Two real Alfea scenarios passed strict and native validation: deleting a
non-shared `Tectopecrans` branch reduced the catalog from 4,266 to 4,261 objects;
deleting original placement `Darch_A01` relocated shared resource ID 1373 and
produced 4,263 objects while preserving all 702 meshes and 87 textures. The
core suite now passes 118 assertions.

## Project reference placement gate

The project journal can now create a placement without copying its mesh
resource. A confirmed reference-only `spStaticRenderObject` branch is copied
at build time with fresh object IDs, an independent world/inverse-world matrix
pair and remapped branch-local references. External references keep pointing
to the existing `spMeshData`; immutable `data.bin` is unchanged. The source
placement can be removed later without removing the generated placement or its
shared resource.

The Alfea scanner identified 252 safe templates. A `Darch_A02` copy added a
three-object branch to clean Alfea, producing 4,269 catalog entries and no new
mesh resource. The 7,106,788-byte SMO passed contextual validation in pristine
`WinxClub.exe`: the direct level path was redirected, FFPS magic and version
were accepted, the loader returned a non-null resource and the process
survived the observation window. Native artifacts are in
`artifacts/experimental/SmoLVLcreator-native-20260826/native-reference-copy-results/run-20260826-203613-883`.

Undo/Redo now operates over compact operation-journal snapshots through
`SmoProjectProjectSession`. It never duplicates the immutable imported
data or catalog, rolls failed transactions back atomically, skips no-op history
entries and can discard history while retaining the accepted project state.
Session history is runtime-only and is not written into `.smolvlproj`.

The experimental core suite now passes 137 assertions.

## Opt-in `.smolvlproj` GUI gate

The existing desktop editor can now create and open an experimental project
without replacing the production 0.1 save path. Project mode uses the same
workspace, scene decoder and GPU renderer, but disables legacy mutation paths
so every future edit must enter the compact project journal. The File menu owns
project save/save-as and verified SMO build commands; the window caption marks
the active session as `EXP` and tracks its saved state.

The first journal-backed scene action places the selected visual object at the
viewport centre. Composite selection is committed as one atomic transaction,
each part keeps its relative offset and references the existing mesh instead of
copying geometry. Generated reference placements can be used as placement
sources too. Each edit rebuilds a temporary verified preview for the common
renderer, preserves camera state and participates in Ctrl+Z/Ctrl+Y history.

Move/Rotate/Scale gizmos now commit their completed matrices through the same
stable-ID project operation, including generated references and composite
selections. Delete removes generated branches from the journal or safely omits
an imported branch after relocating any externally shared leaf. Both actions
are atomic project transactions and rebuild a camera-preserving preview.
Direct Position/Rotation/Scale input now commits through the identical matrix
operation, so gizmo and inspector edits cannot diverge.

GUI Debug build succeeds with zero errors. `SmoLVLcreator.CoreTests` passes 137
assertions and `SmoNativeValidator.Tests` passes 213 assertions.

A hidden WPF smoke run opened the real Alfea reference-copy project and stayed
responsive at roughly 303 MiB working set. Rebuilding that archive through the
current serializer produced `artifacts/experimental/gui-project-gate/Alfea02-reference-copy-rebuilt.smo`:
7,106,788 bytes, 4,269 objects, SHA-256
`554141243F2300C44C78287B2A6DBE553AFC14B33B10ACC0C1425C8DCE86A94A`.
It is byte-identical to the previously native-passed reference-copy SMO.

## Project assets and exact native validation

The opt-in GUI now builds the current in-memory `.smolvlproj` state to a
temporary candidate and hands that exact SMO to the shared contextual native
validator. The candidate never passes through the production 0.1 saver, and
the project remembers source and logical Media path hints for the validator.

The experimental core now has the first generic creation primitive. A caller
can reserve stable object IDs and attach an already serialized inline object
forest at a confirmed direct owner boundary. The archive stores the bytes once
as uncompressed `assets/<guid>.bin`; `project.json` contains only SHA-256,
insertion metadata and new directory entries. `data.bin` remains immutable.
Build streams the asset into the output, reconstructs ownership and catalog
indices, and propagates field, inline-prefix and object sizes to every ancestor.
Undo/Redo snapshots copy only the descriptor, never the asset bytes.

A synthetic forest containing a previously unknown object type survives
project save/reopen and final SMO parse under its resized parent. The complete
`SmoLVLcreator.CoreTests` suite now passes 144 assertions. The next gate is an
adapter from the existing importer builders to this primitive; production Save
remains unchanged.

## Importer forests: collision and external models

The first two real importer paths now share a serializer-owned forest-operation
contract. Collision generation emits its `spCollisionInfo`/`spMeshBV` branch
and physics-registry reference directly; both production and project writers
consume that plan. Clean Alfea gains exactly two objects, keeps project
`data.bin` immutable, reopens with a registered collision and builds byte-for-byte
like the immediate collision writer.

Rigid external import uses a deliberately strict compatibility adapter around
the existing mesh/material/texture serializer. It extracts only complete new
inline forests and rejects the result if any imported object was removed,
moved, renamed or modified outside generated fields. Internal generated
references can be remapped when project IDs are already occupied. The GUI runs
the legacy packing step one mesh per child process, then retains only the
resulting immutable project assets; the whole rewrite loop is no longer hosted
by the interactive process.

The real gate used `Alfea02_old.smo` plus the two-part textured `shrek.glb`.
Archive/reopen preserved every mesh and texture, transactional Undo/Redo removed
and restored the entire import, and the 76,079,343-byte project build matched the
immediate writer at SHA-256
`8BE404B455965CF6B3063AB7F90970875336992EDD84F4362E5046BBC6A42ED5`.
Artifacts are retained under
`artifacts/experimental/SmoLVLcreator-project-external-20260826`.

Project reference placements can now override the shell's mesh reference with
a mesh ID owned by an added asset. The GUI routes both the `Place` button and
catalog-to-viewport drag-and-drop through that operation for original and new
models. A second Shrek placement resolved to the first imported mesh with an
independent transform while mesh and texture catalog counts remained exactly
unchanged. Import and subsequent placement are separate Undo/Redo transactions.

The contextual native run is not currently attributable: both an experimental
collision candidate and unchanged `Alfea02_old.smo` crash at `0x00592C71` in
checkpoint `CP02`, before the target FFPS header is requested; the independent
fast route fails at the same pre-trigger point for the unchanged file.
Structural and byte-equivalence gates pass, but native acceptance remains
pending until the unchanged baseline launches again.

## Project pipeline promoted to the editor default

Opening `.smo` now imports it directly into the editor-owned project graph;
opening `.smolvlproj` resumes the same kind of session. The old mutable saver is
no longer reachable from the normal GUI path. `Ctrl+S` saves the project and the
separate `Build SMO...` command performs the one final reconstruction. The old
experimental submenu and `EXP` caption were removed; internal type names remain
temporarily for archive/API compatibility.

The isolated external-model worker now returns only additive forest descriptors
and blob files. Its rolling compatibility SMO stays on disk between mesh-part
workers and is never loaded into the editor process, removing the redundant
full-level result allocation. The clean Alfea/Shrek project remains byte-equal
to the immediate writer at the existing 76,079,343-byte SHA-256 gate.

Initial external placements can now be moved with the same gizmo as later
reference placements. Their world/inverse matrices are compact overlays over an
immutable asset blob, participate in Undo/Redo, survive project archive reopen,
and are applied only to the build copy. The real external project gate now passes
12 assertions.

## Project-native whole-model replacement

The catalog replacement command is no longer routed through the legacy mutable
document after an SMO becomes a project. The fit window accepts any rigid part
count, and its reference transform is expanded over every original instance.
The isolated importer creates a complete new mesh/material/texture graph with
the authored alpha policy. All old visual roots are then removed as one atomic
reachability operation; nested roots collapse into their containing removal,
resources used only inside the removed set disappear, and externally shared
leaves are relocated to a surviving consumer.

The reusable project core also gained `ResourceRedirects`. A redirect connects
stable same-type object IDs, participates in Undo/Redo and archive persistence,
patches references in imported objects, added immutable forests and cloned
reference placements, and excludes the unreachable original branch only in the
final layout. Imported `data.bin` remains byte-identical.

The default core suite remains at 144 assertions. The explicit 80 MiB Alfea02
plus textured two-part Shrek gate now passes 20 assertions covering archive
reopen, move overlays, reference copies, resource redirect, absence of stale old
IDs, grouped removal of all old placements and reversible tombstones for models
already stored in project assets. Tombstoned forests retain their immutable
bytes for Undo/Redo but are omitted from preview and final SMO builds, enabling
repeated whole-model replacement. Debug builds of the core, test runner and WPF
GUI complete with zero compiler errors; only the existing offline NuGet audit
warning remains.

## 2026-08-27 — project v1 freeze and final 0.1.0 package

The project architecture was promoted completely: source types and files now
use `SmoProject`, the CLI is `SmoLVLcreator.ProjectTool`, the old Experimental
commands were removed, and both solution files point at the canonical project.
The on-disk marker remains `SmoLVLcreator.Project` with `formatVersion: 1`.

Project-native exact object replacement was added for imported and asset-owned
objects. Texture replacement now stores an immutable same-size RGB/RGBA payload
under `assets/<guid>.bin`, leaves `data.bin` untouched, survives archive reopen,
and participates in Undo/Redo. The build applies it only to the output copy.

External import now sends every mesh part to one bounded child job. The worker
returns forest plans and blobs and does not write a rolling complete SMO to
disk. Its GC heap is capped at 50% and `GCConserveMemory=9`. The lower-level
compatibility serializer is still used inside that isolated process to derive
the forests and remains a possible later replacement, but the interactive
editor and project pipeline no longer retain intermediate level containers.

Session history is bounded to 256 transactions. Evicted and compacted history
prunes asset blobs that are no longer referenced by current state, Undo or Redo.
The default core suite now passes 1,655 assertions, including 1,500 randomized
transform operations with periodic compaction, immutable `data.bin`, project
archive reopen and final strict SMO build. The real `Alfea02_old.smo` plus
`shrek.glb` gate passes 20 assertions; the collision gate passes 4.

The native-validation window now prefers the executable explicitly saved by
the user and forwards its configured contextual start level. A pristine fast
baseline accepted FFPS and returned a resource before a later game crash; the
contextual pristine run did not request the target level within 120 seconds.
Native runtime acceptance is therefore recorded as inconclusive, not as a
serializer failure and not as a passed release gate.

Release publishing exposed `Assembly.Location` warnings for a single-file app.
Worker entry-point resolution was centralized in `SmoWorkerHost`: a `dotnet
app.dll` development launch carries the managed entry assembly, while the
published editor relaunches its own executable. The final GUI publish completes
with zero compiler/publish warnings. `Build-Releases.ps1` also now refreshes the
selected archive entry in `SHA256SUMS.txt` instead of leaving a stale checksum.

Final artifact:

- `artifacts/release/current/SmoLVLcreator-0.1.0-win-x64.zip`
- size: 7,072,805 bytes;
- SHA-256: `441D3725BCFC57F400457C9B69E38D2FD2E03C8A0675AB6D693D3CB84E66C5AD`.

## 2026-08-27 — project parity audit and stress gate

The promoted project path was compared operation by operation with the mutable
editor. Missing project-native copy/paste/duplicate, source and added placement
transforms/deletion, collision transform/create/delete, manual visual-collision
links and external-asset owner promotion were restored. Texture edits now reject
stale object IDs through the same live-object/type gate as mesh resources. The
direct whole-container importer is internal compatibility-test code only; GUI
and stress callers use the bounded batched worker.

The final project build also recovered the safety contract of the old saver. It
now runs in an owned child process under a 1536 MiB Windows Job Object limit,
reports five coarse progress stages, supports cancellation, writes an adjacent
diagnostic log, strictly parses the temporary SMO, checks the installed SHA-256
and creates a timestamped backup when replacing an existing target. Structural
preview refreshes materialize directly from the project graph and no longer
write a temporary SMO to disk.

Automated results on the final project core:

- default suite: 1,661 assertions;
- SmoViewer format suite: 198 assertions;
- native-validator suite: 213 assertions;
- SmoTextureTool fixture: 14 files and 14 byte-exact round trips;
- SmoExporter gates: 4, 16 and 39 assertions on native/Alfea/fish cases;
- importer GLB alpha/material/normal/resource/texture gates passed, including
  the two-mesh 560-vertex Tekna smoke;
- real project collision gate: 12 assertions;
- real external project gate: 24 assertions.

The long randomized project run completed 1,000 edits (574 transforms, 265
references, external import and whole-model replacement, 65 texture edits, 58
deletes and 36 collision operations). Archive reopen, two builds and re-import
were byte-identical at SHA-256
`428995C07AFAEA18631FB26BB59240914B05001149FC8154B7A0B996A2768A13`;
peak working set was 421.2 MiB. A separate 80,611,584-byte Alfea02 build passed
20 changes under a hard 1024 MiB child limit. Gardenia01 passed 100 changes.
OBJ+MTL and an OBJ fixture without MTL both imported textures from the model
directory and passed 20-change build/reopen/re-import runs. A final 250-change
project and the new isolated GUI worker produced the same SHA-256
`E66D01235AE624F288A7FFB27F666FCB3AACFA2137B971943839C78F88FE896E`;
the second worker build also produced and reported a valid `.bak`.

Native bisection subsequently attributed the two failure forms to reference
placement rules. A project-owned mesh had once been emitted after a placement
which referenced it, and an imported mesh could be paired with a shell from a
different owner/partition. Both produce a structurally parseable file but are
not safe for the native object registry. Definitions are now emitted first,
and generated references stay under the confirmed resource owner.

A second 250-operation seed exposed two more structural cases. A physical mesh
must derive its lightweight shell from its own static branch, removing only the
copied inline geometry and recalculating every enclosing field size. A mesh
directly below `spPartitionRenderable` is baked sector geometry, not an
independently placeable object; borrowing an arbitrary static shell caused the
same late `0x004BC4B9` failure. The project API now reports such resources as
non-placeable unless an exact existing reference shell is present. Both rules
have synthetic regressions in the 1,664-assertion core suite.

The corrected 250-change candidate passed FFPS magic/version, returned a
non-null `ResourceLoad`, produced no target serializer diagnostics and survived
the complete 70-second contextual observation window without EOF or crash.
The route still ends in a controlled timeout because it has no final scene-ready
checkpoint; this is now a validator-completeness item, not a reproduced file
corruption. Results are under
`artifacts/native-validation/SmoLVLcreator-final-placeable-owner-fix-250`.

The previously recorded 0.1.0 archive predates these parity, isolation and
reference-safety fixes and must be rebuilt before publication.

## 2026-08-27 — final 1,000-operation stress and parity closure

The remaining project-parity gap was manual visual-to-collision association.
Those overrides had lived only in two GUI hash sets, so they disappeared after
closing the editor and bypassed project Undo/Redo. They are now editor metadata
in `project.json`, are captured by project history, survive archive reopen,
follow promoted object IDs and are removed with stale endpoints. They do not
invent an engine-side SMO relation or modify a zero-edit SMO build. The core
suite now passes 1,676 assertions.

Two ownership regressions found during the long run were fixed. Removing the
physical placement of a mesh no longer loses edits while generated placements
still reference it: the first surviving generated placement becomes its
deterministic physical owner. A shell containing multiple same-type reference
fields is now patched by exact field type and occurrence rather than by type
alone. Synthetic tests cover both cases, including an unrelated imported
consumer which must remain a lightweight reference.

The final seed `82744` run completed all 1,000 operations: 602 transforms, 238
reference placements, one external import, one complete model replacement, 53
texture replacements, 67 deletes, 16 collision additions, 13 collision
transforms and nine collision deletes. Checkpoints 333 and 666 both built and
reopened. Final metrics:

- 4,781 parsed objects and 7,190,186 output bytes;
- 17.2 seconds for the final build;
- 363.9 MiB peak worker working set under the 1,024 MiB test limit;
- 337.9 MB final private memory in the stress worker;
- byte-identical repeated build and re-imported build at SHA-256
  `5F0966193AB44DC39B890EDAD2C8EA2CC73E60E586857EAF5725C6CA9853EB6D`.

The exact final candidate was passed to the contextual native validator. The
game accepted FFPS magic and serializer version `0x26`, returned a non-null
resource pointer and survived the observation window without a crash. The
isolated staging directory was cleaned and no installed game asset was touched.
The complete stress report is under
`artifacts/stress/SmoLVLcreator-project-final-fresh-1000-fixed2`, and the native
report is under
`artifacts/native-validation/SmoLVLcreator-final-fresh-1000-seed82744`.
