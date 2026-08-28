# Полный разбор `spFont`

`spFont` (`0x4693490A`) полностью разобран для чтения. В доступных SMO класс
встречается только на PC: по 10 объектов в двух копиях `Menus/menu.smo`, итого
20. PS2 executable содержит тот же class/serializer, но PS2-корпус не содержит
экземпляров.

Единственный field 0 имеет layout:

```text
relationship<spTextureData> atlas
UInt32 height
UInt32 baseline
for character 0x20..0xFF:          // ровно 224 записи
    UInt8   width
    Vector2 uv0
    Vector2 uv1
```

В корпусе height = 32, baseline = 6; все 4 480 glyph records имеют конечные,
упорядоченные UV внутри `[0,1]`. Первый font каждой копии inline-владеет atlas,
оставшиеся девять ссылаются на него: это два storage/ownership variants, а не два
формата шрифта. Все 10 PC-пар совпадают побайтно.

PC executable подтверждает фиксированный цикл символов 0x20..0xFF и точные
width/UV members; независимый PS2 executable подтверждает тот же serializer.
