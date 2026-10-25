# spScene

Частичная PC-реконструкция: [объявление](../../../Sparkplug/Code/Sparkplug/spScene.h)
и [CPU-фаза occlusion](../../../Sparkplug/Code/Sparkplug/spScene.cpp).
Семантика, native anchors, порядок вычислений и ограничения описаны в
[контракте сцены](../../engine/scene/scene-world.md#исходник-cpu-фазы-occlusion).

Класс имеет явный аналитический путь создания и собственные ordered entries
со stamp. Native factory/clone, создание System Root, четыре owned managers
и автоматическая регистрация не реализованы этим путём. Metadata не
предоставляет успешную factory substitution. Наличие исходника не означает
полного восстановления класса или всего вызова Scene Render.
