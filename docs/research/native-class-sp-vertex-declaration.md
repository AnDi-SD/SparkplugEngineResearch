# PC: spDXVertexDeclaration и spPCVertexDeclaration

6 сентября2026. Только PC, authoritative image hash в
[DX materialization](native-pc-dx-materialization.md). Названия обоих классов
взяты из executable, а не придуманы по роли. Source/header paths пока неизвестны.

| Класс | ID | Прямой base | Registration |
|---|---|---|---|
| spDXVertexDeclaration | 33C42E58 | spCrossPlatform20A72504 | 763CC0 |
| spPCVertexDeclaration | 66353288 | spDXVertexDeclaration33C42E58 | 764D10 |

PC initializer6D5A30 содержит строку spPCVertexDeclaration@6F2E7C и factory4C9C20.
Actual factory4C9C20:exact1Ch allocation,vtable6F2E58; words10/14/18 нули.
Base init4B23C0 пишет в14 результат ComponentFlagsToFVF4B21E0, например
840h→112h,940h→152h,20h→1002h,93Eh→114Eh. Слово10 — унаследованный
shared name entry из spNamedObject через storage-free spCrossPlatform,
а не новое declaration field. Это уже известная base-layout связь.
COM declaration pointer хранится в18. Base direct sizeof ещё не объявлен.

## Map и device protocol

`spDXRenderer` helper4AE0E0 использует map по offsetF358:
headF35C/countF360; key — **engine component mask**, value — declaration pointer.
Actual hit возвращает pointer без factory/Init; stored null тоже возвращается,
не создавая замену. Empty miss отдельно остановлен передfactory4AE111.

Полный miss также исполнен:factory4C9C20→vslot1C4C9D20→baseFVF init→
element builder4C9A00→COM CreateVertexDeclaration(device slot158h)→
native map insertion4AE060. Повторный ключ возвращает прежний объект без
повторного COM create. Это общий cache, не уникальная declaration каждого mesh.

Init4C9D20 **игнорирует HRESULT**, освобождает временный element array и
сохраняет out pointer в18. Directed failure с явно нулевым COM out тоже
кэширует объект, но с null COM field. Это не доказательство всех failed-out
вариантов. Bind4C9D00 использует свой18 и device slot15Ch, игнорируя переданный
аргумент и HRESULT. Destructor4C99A0 отпускает COM18, обнуляет его и вызывает
base destructor. Renderer owns cached objects; mesh84 заимствует pointer.

## Element builder

Native4C9A00 строит8byte elements, завершая записью
`FF 00 00 00 11 00 00 00`. Allocation size **не равен использованному размеру**:
93Eh выделяет72 bytes, terminator заканчивается на48; хвост24 остаётсяCC.
1FFFFFh выделяет176, использует152, хвост24 также не инициализирован.
Передавать этот хвост как дополнительные элементы нельзя.

Первоначально проверены flags0/840/940/20/93E/1FFFFF. Normal840h:

```text
0000000002000000
00000c0002000300
0000180001000500
ff00000011000000
```

Packed20h после position даёт элемент offset12,type3,usage2, согласуясь
с преобразованием4u8→4float в native mesh copy.

Checkpoint7: emitter4C9A00 → slot13B2E2C →13D6F00 перенесён в исходники
и сравнён с actual instructions на **106** масках: все32 одиночных bits,
направленные комбинации/FFFFFFFF и64 детерминированных mixed masks.
Совпали FVF, native allocation size и **8656** использованных bytes;
неинициализированный хвост отдельно проверен какCC. Max emitter2203 instructions,
heap10112/65536. Это покрытие выявленных bit branches, не перебор2^32 inputs.

UV count — highest set bit800..40000 →1..8, не popcount. Helper4B22B0 считает
weights по priority bit8→4,bit4→2,bit2→1, **игнорирует bit10**. FVF converter
имеет другой priority, с bit10; эти два алгоритма нельзя объединять.
Allocation=(1+UV+weights+число включённых40/80/100/200/400/20/80000/100000+1)*8.
Weights независимо от count занимают **один** element, что объясняет лишний хвост.

| Условие, порядок | type | usage/index | advance offset |
|---|---:|---|---:|
| position, bit1 off/on | 2/3 | 0/0 | 12/16 |
| weights1 / weights2or4 | 0/3 | 1/0 | 4/16 |
| bit20 | 3 | 2/0 | 16 |
| bit40 | 2 | 3/0 | 12 |
| bit80 | 0 | 4/0 | 4 |
| bit100 /200 | 4 | 10/0,10/1 | 4 |
| bit400 | 2 | 5/0 | **16**, хотя type2 содержит12 |
| UV elements | 1 | 5/0..UV-1 | 8 |
| bit80000 /100000 | 2 | 6/0,7/0 | 12 |

Каждая8byte запись: stream u16=0,offset u16,type/method/usage/index u8;
method=0. Bit400 и первыйUV могут дать одинаковые usage/index5/0.
Emitter воспроизводит это; допустимость D3D-декларации не подменяется
успехом COM fixture. Старшиеbits>100000 не добавляют elements.

## Clone, mutation и исходные ошибки

`probe_pc_declaration_lifecycle.py`: **59checks /4 children**. Actual clone
4C9C90 выполняет factory, настоящую412F70 clone-map insertion и413120 name copy.
Shared-name byte count растёт1→2; FVF/COM остаются0. Opaque base fieldC
**не копируется**. Isolated original52FD90/6D7DB0 и lazy clone-manager выполняются;
name entry задан явно, без присвоения evidence его storage ownership.
Base clone4A1BF0 действительно возвращаетNULL.

Reinitialize4C9D20 обновляет FVF и **перезаписывает прежний COM pointer без
Release**. Это подтверждено и для успешного Create, и для failed Create,
явно записавшегоNULL в out. Cache key840 остаётся прежним, хотя cached object
уже имеет FVF152 от940. Обычное удаление объекта не освобождает потерянный
старый COM resource. Тест освобождает его отдельно как fixture cleanup;
это не native rollback. Bind при failing HRESULT всё равно возвращаетtrue.

## Восстановленные исходники

`Sparkplug/Code/SparkplugDX/spDXVertexDeclaration.*` и
`Sparkplug/Code/SparkplugPC/spPCVertexDeclaration.*` сохраняют original class
names/base IDs; директории/header paths пока **inferred**, не original evidence.
CPU-only emitter и blank clone используют существующие Base/Named/CrossPlatform.
`spDXRenderer::GetVertexDeclarationForAnalysis` теперь хранит объекты по mask;
`spDXMesh` получает этот объект при явном renderer argument, прежний numeric0
stub удалён. Без renderer остаётся явно ограниченный CPU-buffer-only analysis.
COM Bind/Create в portable class пока не реализованы.

Host использует shared_ptr вместо native borrowed pointer, не воспроизводя
dangling references при раннем уничтожении renderer. Эти host lifetimes не
выдаются за native teardown. Source test338 checks включает общий cache,
два disjoint mesh ranges, clone/name и lifetime. All27 CTests pass.

## Проверяемая граница и продолжение

`probe_pc_vertex_declaration.py` проверяет factory/map boundaries/FVF/elements;
`probe_pc_declaration_device.py` — complete miss/reuse/bind/clear и HRESULT case.
`pc_declaration_fixture.py` задаёт initialized-empty map и COM inputs, не
native renderer startup. Actual lookup/insert/factory/base init/builder и
destructors не заменяются seam-ами. COM calls не пересылаются в Windows/GPU.

Открыты исходные API/paths, остальные base lifecycle/name-storage ветви,
полный failure/out-pointer contract, reset/device-loss, владение при native
renderer startup/teardown и интеграционная проверка реального PC backend.
