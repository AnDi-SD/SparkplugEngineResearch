# Полный разбор `spTextNode`

`spTextNode` (`0x52E86EFE`) — наследник `spRenderNode`. Строго декодированы 20
PC-объектов из двух `Menus/menu.smo`; все 10 PC-пар совпадают побайтно. В
доступном PS2-корпусе экземпляров нет, но class registration и serializer есть в
PS2 executable.

Объект хранит обычную node-секцию и ровно одну обязательную inline relationship
`render_node.renderable` на `spTextRenderable`. Отдельной дополнительной
TextNode-секции нет. Полная цепочка ownership:

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
