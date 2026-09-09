# Полный разбор `spSkyBox`

`spSkyBox` (`0x7A7124AF`) имеет сериализуемую node-секцию и упорядоченный
список renderables; в исследованном корпусе это 1..3 inline `spModel`.
Количество и inline encoding не являются ограничениями reader.
Это **не direct C++ inheritance от `spNode`**:
PC runtime RTTI/factory подтверждают `spSkyBox -> spRenderNode -> spNode`,
exact1D4. Строго декодированы 126 объектов
(43/43/40) и все 159 вложенных models. Собственная секция содержит только
повторяемый field 0 `sky_box.model`; все relationships физически inline, а
каждый model проходит полный model/mesh/material decoder.

Найдены три serializer-варианта по числу моделей: один, два или три. В SMO нет
enum стороны куба и нет отдельного признака sky/cloud/moon/sun/fog-shell:
визуальную роль несут геометрия и материалы моделей, поэтому их порядок нельзя
терять.

Все 43 PC-пары совпадают побайтно. Из 40 PC/PS2-пар 39 имеют одинаковую node
семантику и число models; единственное расхождение — ненулевая PC-only Position у
`DatingAssets/mini_level_date_02`. Полные bytes вложенной геометрии
платформозависимы. Ещё три sky box существуют только в PC-ресурсах.

Воспроизводимый отчёт: [`analyze_smo_sky_box.py`](../../research/analyze_smo_sky_box.py).

Serializer не кодирует имена визуальных ролей. PC camera-follow теперь
подтверждён original game→SkyBoxManager snapshot reparent на DefaultCamera:
position наследуется, orientation сохраняется local override. Отдельный draw
снимает fog с models и идёт через base render support; normal sky support —
no-op. [Доказательства](native-pc-scene-special-managers.md). Геометрический
результат structural reorder в игре ещё не проверен; он отложен до relationship writer. План:
[`smo-runtime-validation-plan.md`](smo-runtime-validation-plan.md).

9 сентября общий tools core подключил actual SkyBox и тот же
RenderNodeSerializer, который оригинал регистрирует в 6D4B00. Три
reader/writer cases и два raw world cases совпали с PC. Отдельного
SkyBoxSerializer нет. [Досье внедрения](tool-skybox-shared-core-2026-09-09.md).
