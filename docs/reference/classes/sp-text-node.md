# spTextNode

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spTextNode](../../../Sparkplug/Code/Sparkplug/spTextNode.h).

```text
spTextNode
  -> inline spTextRenderable
       -> inline spFont
            -> inline или shared-reference spTextureData atlas
```

Все 20 четырёхуровневых цепочек разрешаются без поиска границ по эвристикам.
Первый узел в каждой копии доходит до inline atlas, остальные девять — до общей
ссылки; это два ownership variants. Viewer показывает transform, concrete text
renderable, строку, font metrics и glyph table. Безопасное редактирование должно
сохранять всю цепочку согласованно.
