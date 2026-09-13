# spNavigationPortal

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spNavigationPortal](../../../Sparkplug/Code/Sparkplug/spNavigationPortal.h).

## Relationship-варианты и платформы

PC writer/reader и независимый PS2 serializer называют те же поля, включая
`m_uSrcSet`, `m_uDstSet` и `m_uPathIndex`. Viewer показывает node state, graph,
обе стороны, пары узлов и path membership. Изменение пока read-only: оно требует
атомарно перестроить оба navigation set и таблицы `spNavigationGraph`.
