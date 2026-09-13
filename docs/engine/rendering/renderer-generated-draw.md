# PC automatic draw: создание шейдера и повторное использование

Полностью исполнен `4BC290`: построение ключа `4BE310`, первый miss менеджера
`4C8980`, создание shader, компиляция `4CFFE0` с внешним SDK, устройство
`4CA030`, заполнение material constants `4AE930`, cache copy `4BE210`,
привязка shader и indexed draw. Второй вызов берёт тот же shader из кэша,
снова вычисляет изменённый diffuse alpha и не повторяет compile/create/bind.
Оригинальные внутренние функции не заменялись успешными заглушками.

## Вход и наблюдения

Подготовленный template содержит один проход, пустые declarations/code,
entry `Main`, target `vs_2_0`. Оригинальные constructors/reset исполнены;
одиночный pass vector — явно заданное состояние вызывающей стороны.
Это не прохождение file/XML loading. При flags `0x803` SDK получает ровно
`#define USE_TEXCOORD0\n`. Внешний SDK выдаёт пять произвольных bytes
`1020304050` и reflection `MatDiffuse, register0, count1`. Байткод не
объявляется результатом настоящего компилятора или допустимым GPU shader.

Вариант `failed-device` возвращает отрицательный HRESULT из create, bind,
upload и draw при наличии созданного handle. Engine игнорирует эти HRESULT:
возвращает true, сохраняет cache и выполняет тот же порядок вызовов.
После уничтожения manager device handle освобождается ровно один раз;
все отслеженные engine allocations и SDK result objects освобождены.

Полный путь geometry/material/pass с первым generating miss, реальные
file startup, lit/textured scene и живой GPU остаются отдельными задачами.
Результат закрывает bounded automatic-draw caller, не весь renderer.
