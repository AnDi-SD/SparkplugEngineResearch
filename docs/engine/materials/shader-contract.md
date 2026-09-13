# PC shader resources: исходник, программа и численный результат

## Воспроизводимая цепочка

- `spPCShaderManager::BuildSourceInputsForAnalysis` специализирует ключ;
- `spPCEffectTemplate::BuildShaderSourceForAnalysis` собирает header,
  declarations, code и вставку у исходного маркера;
- `spDXShader::LookupParameterTypeForAnalysis` определяет смысл имён.

Пилот с `d3dx9_31.dll`, flags=0, также точно воспроизвёл программу №9.
`d3dx9_43.dll`, flags=0, отверг запись в глобальные переменные старого HLSL;
compatibility flag позволил компиляцию, но не дал того же кода в этом пилоте.
Смена компилятора зафиксирована явно, исходник ради неё не переписывался.

## Все исходные программы

Формулы ниже описывают исходные выражения; ограничения старых GPU-регистров,
клиппинг и последующие texture stages учитываются отдельно.

| Источник | Входы и результат | Значение для переноса |
| --- | --- | --- |
| `BallisticPFX.vsh` | Позиция, скорость, время рождения/жизни; c4–c10 задают ускорение, камеру, размеры и цвета | Сам вычисляет движение частиц, COLOR0 и размер точки; нужен путь частиц |
| `Blur.vsh` | Экранный quad, TextureSize c0, Distance c1 | Четыре сдвинутых UV; позиция становится sign(x/y), z=0, w=1 |
| `Blur.psh` | Четыре texture samples | Среднее четырёх RGBA |
| `BlurLuminance.psh` | Те же samples и коэффициенты c1 | Средний RGB умножается на его скалярную яркость, alpha=1 |
| `Luminance.psh` | Один sample и c0 | RGB умножается на `dot(sample.rgb,c0.rgb)^16`; alpha берётся из sample |
| `Fixed.rfx`, VS | Skinning, матрицы, выбранный свет, ColorMode, UV | Основная семья наблюдавшихся игровых VS |
| `Fixed.rfx`, PS | COLOR0 | Возвращает входной цвет; сам не читает текстуры |
| `Bumpmap.rfx`, VS | Позиция, веса, normal/tangent/binormal, свет и inverse-view | Кодирует tangent-space light/half vectors в COLOR0/1, передаёт UV0 дважды |
| `Bumpmap.rfx`, PS | Texture0, normal map Texture1, COLOR0/1, c0–c2 | Diffuse и specular вычисляются по нормали; COLOR0/1 здесь не albedo |

В Bumpmap PS c0/c1/c2 подписаны в самом RFX как AmbientCol/LightMatDiff/
LightMatSpec. Последовательность умножений даёт **восьмую степень** dot(n,h),
хотя комментарий говорит «power of 32». В расчёт входит исполняемый код.

Ballistic: возраст t уменьшается на lifetime **один раз**, если loop включён
и возраст достиг lifetime. Это не операция modulo. Позиция содержит
`P0 + t*V0 + t²*AccelBegin/2 + t³*(AccelBegin-AccelEnd)/(6*lifetime)`.
Размер точки использует расстояние от исходной P0 до камеры, линейный размер
по нормализованному возрасту и нулевую маску вне диапазона [0,1].
Деление на ноль и иные вырожденные входы здесь не квалифицировались.

ShadowVolumePoint: для d=P−L и r=length(d), Q=L+d*max(r,Range)/r;
результат Q+(P−Q)*P.w. Вершина с w=1 сохраняет P, с w=0 получает Q.
Назначение согласуется с отдельным
[потребителем ShadowVolumePoint](../rendering/scene-render-runtime.md).

## Что действительно встретилось в игре

В указанном закрытом прогоне standalone №1–7 созданы при старте, но в draw
до последнего snapshot не использовались. Все девять №8–16 использовались.
Четыре редких смешанных варианта имели всего 1, 1, 6 и 5 draw-вызовов.

| Shader ID в этом прогоне | Точно воспроизведённая семантика |
| --- | --- |
| 8 | Четыре веса, ColorMode 2: vertex color |
| 11 | Четыре веса, ColorMode 1: MatDiffuse |
| 12 | Четыре веса, ColorMode 4, один directional |
| 9 / 10 | То же, два directional, UV без/с transform |
| 15 / 13 | То же, point затем directional, UV без/с transform |
| 16 / 14 | То же, directional затем два point, UV без/с transform |

## Fixed.rfx: значимые правила

Пусть L — накопленный RGB света, V — vertex color, D — MatDiffuse:

| ColorMode | RGB | Alpha |
| --- | --- | --- |
| 0 | 1 | 1 |
| 1 | D.rgb | D.a |
| 2 | V.rgb | V.a |
| 3 / остальные else | L | D.a |
| 4 | L + V.rgb | D.a |
| 5 | L × V.rgb | D.a × V.a |
| 6 | ConstColor.rgb | ConstColor.a |

AmbientCol — уже произведение выбранного ambient light и ambient материала;
LightMatDiff/LightMatSpec также уже умножены на соответствующий материал.
Нельзя считать эти параметры цветом отдельной глобальной лампы.

Не-specular point при положительном dot сначала добавляет diffuse, затем
умножает **весь накопленный L** на `1/(a0+a1*distance)`. Порядок источников
влияет на результат. Point со specular этой attenuation не использует.
Spot без specular сохраняет отрицательный diffuse dot, умноженный на cone.
Эти особенности подтверждены исходником и сохранёнными инструкциями.

Все включённые UV-выходы начинаются с UV0 и собственного UVTransform[i].
Числа регистров зависят от специализации: например, AmbientCol занимает
c55/c59/c60/c62/c63/c65/c68 в разных реально снятых программах. Использовать
регистры одной программы для остальных нельзя; полная CTAB сохранена.

`ColorBegin`, `ColorEnd`, `Time_Loop_Scales`, `Range`, `TextureSize` и другие
специальные имена не обслуживаются общим `spDXShader` switch (type=0).
Это не доказательство отсутствия констант в игре: их отдельных вызывающих
потребителей ещё нужно связать с реальными записями регистров. BlendWeightCount
в Fixed — параметр генерации, а не оставшаяся runtime-константа шейдера.

## Доказанная ошибка современного backend и исправление

В контрольной точке P=(0,0,1,1), LightPos=(1,0,1,1), N=(1,0,0), power=4:
исходный specular = `(sqrt(2/3))^4 = 4/9`, прежний GLSL даёт 1/4.
Ambient и diffuse в этом входе нулевые, specular RGB единичный.

В общем `SmoGpuSceneRenderer` теперь сохраняется vec4 position; light delta
и view vector нормализуются до извлечения xyz. Геометрическое расстояние
attenuation по-прежнему использует xyz. Математика CPU-производителей прежняя.

Теперь источник и семантика всех наблюдавшихся программ установлены. Сохранять уже доказанные skinning, UV, alpha и порядок цветовых операций; представление добавочного вклада проверить отдельно на штатном Remix API.

## Повторение

В Developer PowerShell с настроенным CMake build общей библиотеки:
