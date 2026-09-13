# PC Skin-owned Fog/material/bone → SAN/scene/mesh draw

После actual SAN actor/scene sampling и mesh reader тот же Fog проходит
423FD0→4AD390→4B0A90 перед Skin world transform/palette/draw. Все Fog device
events видят прежний active bone count9 и ещё не bound geometry. Unknown
Fog type4 возвращает false из setter, но original pre игнорирует результат:
Skin продолжает draw и успешно завершается. Post false сохраняет count1.
Материал и Fog освобождаются actual Skin destructor, массив bone references
остаётся отдельным borrowed native контрактом.

Граф bounded/synthetic; реальная SAN bbush прежняя. Model mesh reference
по-прежнему составлена actual setter после отдельного mesh read; scene view
prepared, owner lifetimes разделены на завершённые фазы. Shader здесь cached,
не генерируется. Ни GPU/OS, ни caps/arena не менялись.
