# spSkyBox

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spModel](../../../Sparkplug/Code/Sparkplug/spModel.h), [spNode](../../../Sparkplug/Code/Sparkplug/spNode.h), [spRenderNode](../../../Sparkplug/Code/Sparkplug/spRenderNode.h), [spSkyBox](../../../Sparkplug/Code/Sparkplug/spSkyBox.h).

Найдены три serializer-варианта по числу моделей: один, два или три. В SMO нет
enum стороны куба и нет отдельного признака sky/cloud/moon/sun/fog-shell:
визуальную роль несут геометрия и материалы моделей, поэтому их порядок нельзя
терять.

Все 43 PC-пары совпадают побайтно. Из 40 PC/PS2-пар 39 имеют одинаковую node
семантику и число models; единственное расхождение — ненулевая PC-only Position у
`DatingAssets/mini_level_date_02`. Полные bytes вложенной геометрии
платформозависимы. Ещё три sky box существуют только в PC-ресурсах.
