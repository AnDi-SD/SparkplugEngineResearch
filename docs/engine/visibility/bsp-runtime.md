# PC `spBSPNode`: lifetime, queries и выбор Zone

## Идентичность и исходник

Original RTTI `spBSPNode` **7362AB22 → spPartitionNode67672341 → spBaseObject**;
это не наследник spNode. Registration CALL6D4230, record7613F8/base75E1B8,
factory480B90 через protected slot13B23E4. Exact native size **AC**,
primary table6EBA30 имеет33 slots. Factory/lifetime реально исполнены;
не приписываем отдельному адресу предполагаемый constructor symbol.

Добавлены частичные `Sparkplug/Code/Sparkplug/spBSPNode.h/.cpp`,
`Tests/spBspTests.cpp`, observed ABI `spBSPNodeLayoutAC` и ray record8.
Путь реализации **inferred**: limited ASCII search не находит BSP .cpp/.h,
также нет `spBSPNodeSerializer.cpp`, хотя serializer class name есть.
`GetPlane`, `GetPolygonVertex`, `GetPolygonVertexCount` — original names из
diagnostics; остальные descriptive API намеренно `ForAnalysis`.

| Runtime поле | Доказанное назначение |
| --- | --- |
| 00..83 | inherited PartitionNode, включая owned child array58/count5C |
| 84..90 | split plane normalXYZ/constant; constructor **не инициализирует** |
| 94/98,9C/A0 | две внутренние ray `{child,parameter}` записи, initially untouched |
| A4/A8 | directly owned optional polygon XYZ array / vertex count, initially0 |

## Точные запросы к плоскости

Distance = `(z*nz + y*ny) + nx*x - constant`, native x87.

| Slot / entry | Правило |
| --- | --- |
| 40 /480710 | Если stopAtZone и Zone60, вернуть this до проверки plane. Иначе distance>0 → child0, всё остальное → child1; затем child.v40 |
| 44 /480780 | distance>+0.001f →mask1, distance<−0.001f →mask2, band/NaN →mask3 |
| 48 /4807D0 | abs(distance)<=radius →mask3; иначе positive→1, прочее→2 |
| 5C /480200 | unchecked two-word table740374:8/1, три бита на child, order0,1 или1,0 |
| 54 /4D6550 | no-op: BSP не выключает clip planes как Octree |

Таким образом **положительная сторона — slot0**, отрицательная и точка ровно
на plane в leaf lookup — slot1. Это доказано исполнением, а не статистикой
координат SMO. PointMask и lookup различаются на epsilon band: Debug traversal
берёт первый установленный mask bit, поэтому его starting side на plane может
отличаться от strict leaf query. Не заменять эти функции одной общей эвристикой.

## Ray candidates и видимые дети

### Кэш кандидатов луча на PC

Метод `00480360` вычисляет скалярные произведения нормали с направлением
и началом луча. Он складывает произведения в порядке `(Z + Y) + X` с
64 значащими битами x87. Для начала он вычитает константу плоскости перед
записью `binary32`. Условие пересечения использует оба сохранённых слова.

Первая пара содержит сторону 0 при положительном расстоянии, иначе сторону 1,
и положительный ноль. Вторая пара появляется, когда сохранённое произведение
с направлением больше `1e-5f` со стороны 1 или меньше `-1e-5f` со стороны 0.
Равенство порогу сохраняет оба старых слова второй пары. Записанное время
пересечения равно отношению расстояния к произведению с направлением со
сменой знака. Эта последовательность сохраняет знак нуля.

`UpdateRayScratchForAnalysis` принимает сырые слова плоскости, луча и двух
пар. Допустимы нули обоих знаков и конечные нормальные числа с модулем от
2⁻²⁴ до 4096. Возврат 1 или 2 указывает число активных пар. Необязательный
результат заимствует первую пару переданного кэша, соответствующую native
`+0x94`. Прежний метод `RayCandidatesForAnalysis` возвращает собственный
вектор. [Исходник BSP](../../../Sparkplug/Code/Sparkplug/spBSPNode.cpp).

### Видимые дочерние узлы

Camera-side здесь distance>**0.001f**, не strict-zero leaf rule. Packed word:
low4=count, далее3bits/child. Both/positive camera =82, both/other=12;
one/positive=01, one/other=11. Native null children не фильтруются.

RenderNode slot28/480540:

- Zone60 + dynamic, либо static billboard (B0 mask300000): touching sphere
  остаётся в текущем BSP; non-touching идёт в единственный child;
- no Zone либо ordinary static400: sphereMask48, все выбранные children;
- actual reciprocal Node1C4/Partition20 lists подтверждены шестью сценариями.

Visibility46D270 сначала вызывает root.v40(cameraPosition,stopAtZone=1),
затем проходит **local roots выбранной Zone**. Этот путь не требует обычного
recursive BSP child clipping из root. Он не закрывает capped45E870 copy из
Octree/portal-near-plane и не доказывает полную normal recursive traversal.
Helpers не заменялись unconditional success и capped calls не продолжались.
