# spDXVertexDeclaration / spPCVertexDeclaration

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spDXVertexDeclaration](../../../Sparkplug/Code/SparkplugDX/spDXVertexDeclaration.h), [spPCVertexDeclaration](../../../Sparkplug/Code/SparkplugPC/spPCVertexDeclaration.h).

| Класс | ID | Прямой base | Registration |
| --- | --- | --- | --- |
| spDXVertexDeclaration | 33C42E58 | spCrossPlatform20A72504 | 763CC0 |
| spPCVertexDeclaration | 66353288 | spDXVertexDeclaration33C42E58 | 764D10 |

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

```text
0000000002000000
00000c0002000300
0000180001000500
ff00000011000000
```

Packed20h после position даёт элемент offset12,type3,usage2, согласуясь
с преобразованием4u8→4float в native mesh copy.

UV count — highest set bit800..40000 →1..8, не popcount. Helper4B22B0 считает
weights по priority bit8→4,bit4→2,bit2→1, **игнорирует bit10**. FVF converter
имеет другой priority, с bit10; эти два алгоритма нельзя объединять.
Allocation=(1+UV+weights+число включённых40/80/100/200/400/20/80000/100000+1)*8.
Weights независимо от count занимают **один** element, что объясняет лишний хвост.

| Условие, порядок | type | usage/index | advance offset |
| --- | ---: | --- | ---: |
| position, bit1 off/on | 2/3 | 0/0 | 12/16 |
| weights1 / weights2or4 | 0/3 | 1/0 | 4/16 |
| bit20 | 3 | 2/0 | 16 |
| bit40 | 2 | 3/0 | 12 |
| bit80 | 0 | 4/0 | 4 |
| bit100 /200 | 4 | 10/0,10/1 | 4 |
| bit400 | 2 | 5/0 | **16**, хотя type2 содержит12 |
| UV elements | 1 | 5/0..UV-1 | 8 |
| bit80000 /100000 | 2 | 6/0,7/0 | 12 |

## Восстановленные исходники

Host использует shared_ptr вместо native borrowed pointer, не воспроизводя dangling references при раннем уничтожении renderer. Эти host lifetimes не выдаются за native teardown. All27 CTests pass.
