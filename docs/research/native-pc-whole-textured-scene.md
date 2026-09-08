# PC: whole loading.smo с текстурой и общей геометрией

Продолжение CP114: [целый gem.smo с двумя текстурами и UV runtime](native-pc-whole-uv-scene.md).

CP109, 8 сентября 2026. Неизменённый `Menus/loading.smo` полностью прочитан
original `422B50` и C++: совпали11 объектов,13 ссылок и2956 байт состояния,
material layers, buffers/declarations и mip pixels. Впервые в whole scene
выполнена генерация недостающих mip-уровней из [CP108](native-pc-texture-missing-mips.md).

Файл2442 байта, SHA-256
`0E8EB7A89E952CD0CF096AE4F5E3F1FE4D56BEC3696DB427567F0BB2BF04427E`.
Original whole load:364725 инструкций, около2,48 с;89 assertions;
arena84224 байта. Все177 tracked native allocations освобождены, включая
explicit retained-combiner cleanup; texture/buffer COM references и locks
сбалансированы. Восстановленные имена, class IDs и все FAT ID references точны.

## Граф и общий путь

Root Node владеет двумя RenderNode. Каждый из них содержит две ссылки на
один и тот же Model — эти повторения сохранены и в capture, и в C++.
Два Model ссылаются на разные materials/meshes и один общий Fog.
Второй material содержит ссылку на единственную DXTexture ID10 `load_default`.
Два DXMesh используют результат original DX batch hook и общую declaration.

TextureData wire ID `78EA082B` разрешается через DXData serializer42B660;
header42DD10 создаёт runtime DXTexture `3F3651B6`. Embedded reader42C640
вызывается дважды, включая внутренний source wrapper. Затем actual4ABBA0,
4AB030 и четыре61039A/60FDB4 вызова создают полный набор пяти уровней16×16…1×1.
Все1364 texture bytes сопоставлены с source. Из остальных данных734 байта
приходятся на object state,170 на material layers,688 на mesh buffers/declarations.

Общий native/source capture расширен texture state и material→texture edges.
Native COM identity проверяется по runtime object+3C; packed pixels читаются
из тех же поверхностей, которые заполнил оригинал. C++ snapshot берёт состояние
из общего reader и CPU shadow. В новых frontend ветвях нет отдельного codec.

## Пределы и проверка

Сохранены file limits:1 млн инструкций/8 с, процесс30 с, arena128 КиБ,
один native allocation32 КиБ, COM mesh buffer1 КиБ. Для текстуры заранее
заданы стороны16×16, до5 уровней и до2048 байт на поверхность. Manager/RTTI
остаются явной startup fixture; texture I/O и ограниченные CRT/debug inputs
из CP108 установлены на тот же COM device, что и mesh buffers.

Для этого общий compact texture installer принимает класс объявленного стенда.
Прежний single-level класс остаётся стандартным; отдельная проверка отклоняет
неподходящий класс до обращения к памяти. Старый textured Skin путь и standalone
missing-mip fixture проверяются повторно после общего изменения.

```powershell
python research/probe_pc_scene_file_profile.py loading --file-ids
```

[CP109 manifest](../../research/native-cycle-checkpoint-2026-09-08-cp109.json)
содержит свежие результаты и fingerprints. Это whole load при заявленных
consumer inputs. Полный CRT startup, arbitrary external resources, runtime save,
все failures, одновременный actor/render frame и live graphics backend остаются
открыты. Class scores за композиционный checkpoint не повышены.
