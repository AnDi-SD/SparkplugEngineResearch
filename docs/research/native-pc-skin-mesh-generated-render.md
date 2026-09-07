# PC decoded mesh → retained Skin → generating draw (CP73)

2026-09-07; pristine EXE SHA-256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

В связке [CP69](native-pc-skin-generated-render.md) подготовленный DXMesh
заменён объектом из original header42AFD0 и полного DXMeshData reader429BC0.
Прочитан native field1 с прежним ignored17-byte header, затем actual CPU
index/vertex readers,4AA000 materialization и4AE0E0 declaration cache/create.
Flags803, три вершины по28 bytes, три16-bit индекса0/1/2. Все84 vertex bytes,
6 index bytes, declaration elements и восемь metadata words совпали с C++.

Тот же mesh присоединяется actual Model setter479E20 к прежнему decoded
Skin;46A240 доходит до first shader generation, constants и indexed draw.
Реальные COM handles нормализованы declaration1/index2/vertex3. Теперь
наблюдаемый draw — primitive4,baseVertex0,minVertex0,vertexCount3,startIndex0,
primitiveCount1,stride28. Декодированные metadata не переписываются стендом.
Source вызывает общий spDXMeshDataSerializer и существующий Skin renderer.

Normal и failed-device: **2 exact captures,92 native assertions** (37+9
на сценарий). Mesh read25186,render42407 instructions;peak65472 bytes,
61 owner generations all freed. Declaration map cleanup4AE140 и COM
declaration/buffer releases выполнены. Старые CP64 5/5, CP72 4/4 и CP69 2/2
после fixture изменений успешны; source Skin default suite70/70.

## Внешние границы и память

Один declared COM interface page содержит только два ≤512-byte buffers и
interface tables. Страница writable, потому что original engine копирует
туда vertex/index bytes через Lock. Engine wrappers/CPU temporaries/map/
declaration elements остаются в64KiB arena. После завершённого Skin read
стенд снимает prepared RTTI lookup tree; реальные RTTI records сохранены.
После последнего mesh read заканчивается lifetime exact raw32-byte stream
и64-byte vtable prefix, ранее зарезервированных до reusable allocator.
Новая операция проверяет исходные адреса/размер и запрещает повторный release.
Живые объекты не перемещаются, caps не увеличены.

Первый fresh probe выявил read-only mapping для Lock output; исправлен доступ
только external COM page. Следующий fresh probe не уместил88-byte allocation
при65472 reserved; после явного завершения stream lifetime новый запуск
прошёл. Остановленные состояния не продолжались.

Skin и mesh здесь два завершённых чтения плюс actual setter, не один whole
SMO load/reference transaction. Полная mesh relationship внутри Model reader,
материал/texture acquisition, simultaneous frame и GPU остаются открыты.
Compiler SDK bytes по-прежнему условные1020304050/reflection fixture.

```powershell
python research/native_workbench.py run pc-skin-mesh-generated-render --deadline-utc 2026-09-07T16:00:00Z
```
