# PC: DX metadata, buffer materialization и declaration

6 сентября2026, продолжение [full loader](native-pc-full-loader.md).
Original PC hash `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Все вызовы ограничены100k instructions/2s,30s child и64KiB guest arena.
COM-объекты здесь явные тестовые inputs/outputs; Windows/game/GPU не запускаются.

## Metadata и первый проход hook

`4AA4E0` исполнен отдельно в19 вариантах (92 checks). Полный header ID/marker
не проверяет. Последний field1 перезаписывает предыдущий; неизвестные поля
пропускаются. Отсутствующий field1 возвращает true **не меняя outputs**.
Даже nullptr от общего ReadHeader (EOF/missing terminator/failed compact header)
превращается в true при уже выданном diagnostic. Ошибки пяти scalar reads,
Tell, seek и rewind отдельно возвращают false; частичные outputs сохраняются.
Последний bool читается как исходный byte, напримерA5, без нормализации.

`4AA870` first scan:7 направленных cases/35 checks, остановка **до allocation**
combiner-а, без возобновления. FVF grouping и strict total<20000 подтверждены.
Значения outputs helper-а не обнуляются между entries: normal10 vertices +
entry безfield1 дают20; missing-first может посеять нулевую batch. Это original
stale-state hazard, не допустимость этих файлов для безопасного writer-а.
Portable foundNativeField/overflow guards сохраняются и не выдаются за native.

## Исправленный PC wire contract

`429A40` читает четыреu32 иbyte в переиспользуемые temporaries: **planning
header не участвует в самой материализации**. После него реально вызываются:

1. `45F9D0` factory и `45FB80` обычного spIndexBuffer: type, primitiveCount,
   flags, index bytes.
2. `460130` factory и `460300` обычного spVertexBuffer: componentFlags,
   vertexCount, flags, vertex bytes.
3. Выходной mesh virtual initializer -> `4AA000`: преобразование/копирование
   CPU buffers в выбранное DX storage.

Прежнее описание «сырой vertex payload, затем сырой index payload» было
неверным для PC. В частности, unchanged `Menus/logo_screen.smo`:
file hash `DBD6A1F261008BBF1C2971030517B7C9D60A5E27F58A4A69F7C14EAF10E2E3C7`,
object5@471 size207, field1@484 size193; header=(940h,4,144,8,0), затем
index=(type3,primitive2,flags0,indices2/0/1/3), vertex=(940h,4,0,144 bytes).
Это readonly corpus confirmation; весь этот SMO пока не выдан за native-loaded.

Directed payload: обычный triangle, полностью неверный planning header,
32bit indices и packed component20. Actual CPU readers/copy/commit/FVF выполнены.
В `triangle-complete` и `packed-complete` helper429A40 возвращается нормально,
включая actual declaration map/create и native temporary CPU cleanup.
Первые четыре режима отдельно остановлены перед4AE0E0; explicit frame abort
и fixture teardown не выдаются за native rollback.

## Buffer и mesh lifetime

Actual factories подтвердили DX vertex wrapper20h, index wrapper1Ch,
**spDXMesh88h**. Vertex ctor:COM pointer/14/18 нули,byteSize1C остаётсяCC;
index ctor:COM pointer/14/18 нули. Роль reserve fields18/14 всё ещё неизвестна.
Combiner2Ch ctor не пишет target4, обнуляет остальные собственные слова.

6 wrapper/combiner cases дают97 checks. Create sequence:index→vertex;
Lock:vertex→index; partial commit меняет cursors/counts, exact-full unlock-ает
vertex→index; destructor отпускает index→vertex. Mesh и combiner каждый
держат по одной intrusive ссылке на общие wrappers; COM resource принадлежит
wrapper-у. Подтверждены ignored Create/Lock/Unlock HRESULT. Failed Lock leaves
null cursors; unsafe copy после него **не исполняется**. Это не полный device-loss
или allocation-failure контракт. Host fail checks намеренно строже.

Два дополнительных original hazards:

- 32bit index bytes копируются как4byte, но combiner создаёт COM INDEX16(65h).
  Native CPU copy evidence не доказывает корректное рисование такого input.
- Packed20 превращает4u8 в4 ненормализованных float:3 vertices дают84 bytes
  вместо48, stride28. После copy/commit native повторно добавляет36 к stored
  mesh byteSize64h:получается120, хотя реально скопировано84. Portable хранит
  фактические84 и явно не воспроизводит ошибочную метаинформацию.

## Declaration — следующий закрытый стык

[spDXVertexDeclaration/spPCVertexDeclaration](native-class-sp-vertex-declaration.md):
renderer map4AE0E0 возвращает **объект**, а не числовой FVF/handle. Native mesh
field84 исправлен в PC ABI на Address32 vertexDeclaration. Checkpoint7 добавил
CPU-only source declaration/cache с явным renderer argument в mesh Init;
прежний numeric0 stub удалён. COM creation/bind ещё открыты; CPU-only whole
loader source дополнен в checkpoint8 ниже.

Доказанные полные helper calls всё ещё не являются whole mesh-containing
FFPS load, whole save, source differential всей геометрии или GPU rendering.
Неизвестные ветви перечисляются дальше;100% не объявляется.

## Checkpoint7: whole native hook и source declaration

`probe_pc_dx_hook_materialize.py`: triangle, triangle-pair и logo-field.
Это полный `4AA870`, не stop-before. Actual FAT load466B90/RTTI membership,
serializer factory4297C0 **exact14h**, Register422D90/Find4224F0, два metadata
scan, combiner creation, header42AFD0, fields429BC0, helper429A40, CPU buffers,
DX copy/commit, declaration map/create и IsKindOf/SetName dispatch выполнены.

Triangle50316 instructions/heap64416; pair53284/65152; logo-field50452/64528.
Все укладываются в прежние100k/2sec per call/64KiB heap/30sec child.
Logo-field использует **неизменённые193 bytes** из указанного выше SMO,
но directory/object envelope в fixture составлены явно: это не whole file load.
COM device, stream и name-storage — явные seams; поля/фабрики/контейнеры нет.

FAT entry20 ещёNULL во время создания первой declaration, публикуется после
успешного payload. Два меша получают разные DXMesh pointers, общие wrappers и
declaration, index/vertexBegin второго=(3,3). Index bytes остаются local0/1/2,
не rebased в storage. Обе mesh-ссылки плюс combiner дают wrapper refcount3.
Completed hook очищает763148, **оставляя combiner allocation живой**. В его
теле нет переноса ownership или вызова destructor. Fixture освобождает этот
остаток явно; это не доказательство полного native renderer teardown.

На первом scout был пропущен required RTTI startup input: loader вошёл в уже
известный cold414C60, allocation guard остановил запрос. Не возобновлялся;
валидный отдельный fixture получил ранее доказанный one-entry RTTI tree.
Ограничения и код неизвестного конструктора не менялись.

Source cache/declaration:106 masks/8656 exact bytes,338 portable checks;
native declaration lifecycle59checks раскрывает clone и reinitialize leak.
См. [declaration dossier](native-class-sp-vertex-declaration.md).
Убрана ошибочная host проверка `combiner FVF == converted mesh FVF`:
реальный logo planning header=component940, mesh converted FVF=152.
Capacity/range/overflow guards сохранены. COM fixture принимает native
CreateVB argument940; это **не** утверждение допустимости такого FVF на GPU.

На checkpoint7 следующими были concrete mesh payload adapter, полный FFPS
с graph/root, save dispatch и PC backend. Первые два стыка дополнены ниже;
native proof и portable integration всё ещё учитываются отдельно.

## Checkpoint8: concrete readers и whole mesh-containing FFPS

`probe_pc_mesh_serializer.py` исполняет42B420/429BC0 полностью на6 tiny inputs:
base field0, DX field0(platform1), DX field1(platform2), обаfield0/1,
packed20 и preceding unknown field. Обе actual factory дают14h; specialised
42AFD0 игнорирует оба header words и создаёт DXMesh, не wire MeshData.
В standalone path Create/Lock/Unlock идут index→vertex, в отличие от
combiner Lock/Unlock vertex→index. Packed standalone copied/stored84 совпадают:
аномалия stored120 относится именно к combiner branch.

156 native checks; `compare_pc_mesh_readers.py` дополнительно сравнил60
полей source/native: одинаковый input целиком, cursor, FVF, stride, sizes,
component count и все vertex/index bytes. Макс30439 instructions/63760 heap.
Field80 прежде ошибочно назывался UV count: он использует priority bits
02/04/08/10→1/2/3/4, а на840 даёт0 при наличии одного UV. Алгоритм source
уже был правильным, исправлены только ABI/API/field labels; inferred blend
weight role и оригинальное неизвестное имя не выдаются за доказанный symbol.

`probe_pc_mesh_full_loader.py` запускает actual422B50 на явном292byte FFPS:
empty Animation root и один MeshData. Header→FAT/RTTI→file index→4AAB80→
whole4AA870→registry/header/CPU buffers/DX copy/declaration→outer422940→
RTTI Animation factory/reader→FAT clear пройдены без подмен этих тел.
44 checks,78325 instructions,65040 из65536 heap. Native root не подменяется
prebound mesh. Финальный cursor138 (конец root), не292: outer пропускает уже
созданный mesh. Ресурсы остаются живы послеclear. Combiner тоже остаётся
allocated; explicit fixture teardown не закрывает original scene ownership.
Это составной тестовый FFPS, не полный реальный SMO уровня и не native Save.
COM/stream/name/initialized RTTI containers остаются явными fixtures, не GPU.

Source `spMeshDataSerializer`/`spDXMeshDataSerializer` используют существующие
Index/Vertex codecs и DXMesh initializer. `MaterializePreparedForAnalysis`
вызывает общий registry/header/payload core, публикует FAT послеpayload,
выставляет scoped active combiner и требует явный CPU renderer/cache.
Host guards:32MiB field envelope,64MiB batch bytes,4096 objects,64 depth;
перед CPU buffer allocation проверяется оставшийся объём serialized payload.
Неизвестные fields пропускаются, но lossless preservation пока не реализовано.
Native missing-terminator/stale/partial-success hazards не воспроизводятся.

При source whole-file test обнаружены две ошибки собственной RTTI адаптации:
cross-TU registration терялась, если base record ещё не инициализирован;
lazy Animation target не регистрировался от одного создания serializer.
147 static registrations используют deferred host queue (130 class sites и
17 bootstrap sites); Find/Count пробуют прежнюю строгую Register, не обходят
FAT/base checks. Animation/MeshData serializers явно удерживают target RTTI.
Static lifetime queue и static-library linkage — только portable startup
адаптация; protected original RTTI startup остаётся неизвестным.

Source131 checks:6 readers,полный FFPS с двумя мешами, две одновременно живые
загрузки, освобождение первой и7 malformed/rejected cases. Ещё9 проверок
deferred/strict RTTI в SparkBase. Все28 CTests прошли18,69sec; workbench10/10,
platform12/12, два новых bounded profiles6/6 и1/1. Новых native caps нет.
Далее: Node/RenderNode/Model graph readers и save dispatch, texture/material/
skin зависимости, renderer lifetime и настоящая backend validation.
