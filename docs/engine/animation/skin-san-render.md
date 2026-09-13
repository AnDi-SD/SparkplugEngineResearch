# PC SAN actor → прочитанная кость → Skin palette/draw

Выполнены original factories manager `454640`, actor `5A3620`, animation
`41A090`, serializer `43DAB0`; name registry, discovery `5A33F0`, start
`5A1E30`, manager frame `4535A0` → actor tick `5A2380` → track sampling
`479290` → controller application. После явного world update `421420(1)`
полный Skin `46A240` доводит эту кость до palette, BlendMatrices constants
и indexed draw. Не подменены actor/evaluator/controller/Node/render bodies.

## Сравнение

Например, в0.25 исходная cubic scale даёт
`{0.7766315937042236, 0.9197549819946289, 0.7766315340995789}`: небольшое
различие X/Z сохранено. С inverse-bind translation `{5,6,7}` это достигает
разных palette translation и shader constant rows; результат не заменён
identity или заранее вычисленными Skin input matrices.

## Владение и границы

Новые крайние float inputs, остальные SAN tracks, blend/restart в этой
render-связке, whole SMO resources, lit/custom material и живой GPU открыты.
Прежние actor-only suites отдельно покрывают более широкий playback набор.
