# `spTextNode`: общая RenderNode-секция и корпусный профиль

Обновление11 сентября: [CPU class/reader/ownership восстановлены](tool-text-runtime-2026-09-11.md).
Ниже сохранено описание прежнего среза; полный text render ещё открыт.

Уточнение 10 сентября 2026: PC4423D0/4423B0/4423C0 напрямую делегируют
Read/Write/Index общему RenderNode serializer. Поэтому отдельная обязательная
«ровно одна inline TextRenderable» grammar не подтверждена. Metadata decoder
использует общую Node/RenderNode проекцию; concrete runtime construction/layout
и полный teardown TextNode пока не заявляются.
[Оригинальные проверки](tool-text-original-defaults-2026-09-10.md).

`spTextNode` (`0x52E86EFE`) — наследник `spRenderNode`. Строго декодированы 20
PC-объектов из двух `Menus/menu.smo`; все 10 PC-пар совпадают побайтно. В
доступном PS2-корпусе экземпляров нет, но class registration и serializer есть в
PS2 executable.

В выбранном menu объект хранит обычную node-секцию и ровно одну inline relationship
`render_node.renderable` на `spTextRenderable`. Это корпусный профиль, не
общее требование reader. Отдельной дополнительной TextNode-секции нет.
Наблюдаемая цепочка ownership:

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
