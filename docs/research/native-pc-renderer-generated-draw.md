# PC automatic draw: создание шейдера и повторное использование

CP58, 7 сентября 2026. Оригинал: `local-data/pc-pristine/WinxClub.exe`,
SHA256 `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

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

Оба вызова draw имеют аргументы `(2,11,13,17,19,0x803,0)`. Diffuse RGB
равен `(0.25,0.5,0.75)`, alpha меняется `0 -> 1`. Проверены все четыре raw
float words, последовательность device calls и состояния renderer внутри
каждого callback и после возврата. Временное auto-selection `E454` очищается
после draw, bound identity `E44C` и constant high-water `E444` сохраняются.
Indexed callback получает `(4,17,0,19,11,13)`.

Вариант `failed-device` возвращает отрицательный HRESULT из create, bind,
upload и draw при наличии созданного handle. Engine игнорирует эти HRESULT:
возвращает true, сохраняет cache и выполняет тот же порядок вызовов.
После уничтожения manager device handle освобождается ровно один раз;
все отслеженные engine allocations и SDK result objects освобождены.

## Реализация и воспроизведение

`spDXRenderer::DrawAutomaticForAnalysis` объединяет существующий cached path
с восстановленным manager `SelectOrCreateForAnalysis`. Необязательный
`spPCShaderGenerationForAnalysis` передаёт контракты SDK/device. Старый
`DrawCachedAutomaticForAnalysis` вызывает тот же core без generation input.
Проверка полной инициализации constant rows остаётся явной host safety policy.
Имена новых API и carrier аналитические, не восстановленные symbol names.

```text
python research/native_workbench.py run pc-renderer-generated-draw --deadline-utc 2026-09-07T16:00:00Z
```

Два точных original/source captures, 22 native assertions; отдельный C++
тест — 12 assertions. Максимум 32256 инструкций на вызов и 64992 bytes
engine arena. Лимиты 100000/2s/30s/64KiB и 32KiB allocation неизменны.
Для экономии arena только данные внешних SDK/device fixtures размещены
в их уже выделенных 4KiB interface pages; engine allocations сохраняют
обычное отслеживание и ограничения. Все pointers/slots заданы заранее,
map-on-fault не применяется.

Полный путь geometry/material/pass с первым generating miss, реальные
file startup, lit/textured scene и живой GPU остаются отдельными задачами.
Результат закрывает bounded automatic-draw caller, не весь renderer.
