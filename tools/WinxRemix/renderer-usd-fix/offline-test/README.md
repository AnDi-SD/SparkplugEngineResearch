# CPU-проверка USD-экспортёра

Тест вызывает настоящий `lss::GameExporter` на собственной геометрии,
сохраняет составные сцены и повторно читает их через USD. Окно ошибки
экспортёра заменяется записью в stderr с последующим провалом теста.
Все остальные операции записи выполняет библиотека экспортёра.

Нужны Windows x64, MSVC, Meson, подготовленные исходники Remix и совместимые
библиотеки USD. Проект рассчитан на закреплённую Windows-сборку USD 22.11
с Python 3.10 и её именами import libraries. Зависимости не устанавливаются
автоматически. Включённая библиотека Tracy должна соответствовать сборке
renderer; проверенный вариант собран без активной трассировки Tracy.

## Сборка

Примените [четыре патча экспортёра в указанном порядке](../README.md)
и соберите renderer.
В окружении MSVC x64 создайте отдельный каталог проверки:

```powershell
meson setup <new-build> tools/WinxRemix/renderer-usd-fix/offline-test --buildtype release -Drenderer_source=<renderer-source> -Drenderer_build=<renderer-_Comp64Release> -Dusd_directory=<usd-distribution>
meson compile -C <new-build> -j 1 test_usd_exporter test_usd_dynamic_buffers test_usd_variable_skin test_usd_signed_adapter
```

Проверка использует `liblssUsd.a`, `libutil.a` и `libtracy.a` из указанной
сборки. Для изоляции изменения одного файла можно передать
`-Dexporter_object=<compiled-game_exporter.cpp.obj>`: он заменяет `liblssUsd.a`
в этой проверке. Объект, заголовки и USD должны иметь согласованный ABI.
Остальные библиотеки renderer и шейдеры тест не компилирует.

## Запуск и результат

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/renderer-usd-fix/Test-UsdExporter.ps1 -Executable <new-build>/test_usd_exporter.exe -Python <compatible-python.exe> -UsdDirectory <usd-distribution> -Output <new-output-directory>
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/renderer-usd-fix/Test-UsdExporter.ps1 -Executable <new-build>/test_usd_exporter.exe -Python <compatible-python.exe> -UsdDirectory <usd-distribution> -Output <new-default-joints-directory> -Scenario DefaultJoints
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/renderer-usd-fix/Test-UsdExporter.ps1 -Executable <new-build>/test_usd_dynamic_buffers.exe -Python <compatible-python.exe> -UsdDirectory <usd-distribution> -Output <new-dynamic-output-directory> -Scenario Dynamic
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/renderer-usd-fix/Test-UsdExporter.ps1 -Executable <new-build>/test_usd_variable_skin.exe -Python <compatible-python.exe> -UsdDirectory <usd-distribution> -Output <new-variable-skin-directory> -Scenario VariableSkin
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/renderer-usd-fix/Test-UsdExporter.ps1 -Executable <new-build>/test_usd_variable_skin.exe -Python <compatible-python.exe> -UsdDirectory <usd-distribution> -Output <new-variable-default-directory> -Scenario VariableSkinDefaultJoints
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/renderer-usd-fix/Test-UsdExporter.ps1 -Executable <new-build>/test_usd_signed_adapter.exe -Python <compatible-python.exe> -UsdDirectory <usd-distribution> -Output <new-signed-adapter-directory> -Scenario SignedAdapter
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/renderer-usd-fix/Test-UsdExporter.ps1 -Executable <new-build>/test_usd_signed_adapter.exe -Python <compatible-python.exe> -UsdDirectory <usd-distribution> -Output <new-signed-static-directory> -Scenario SignedAdapterStatic
```

`Output` должен быть новым каталогом. Запускатель сохраняет собственные
исходники проверки, хеш исполняемого файла, журналы, расход памяти, код выхода
и результат ожидания процессов. Каталоги поиска USD задаются только на время
проверки; окружение восстанавливается после завершения.

Перед запуском нужны 512 МиБ свободной физической памяти и 2 ГиБ запаса commit.
По умолчанию каждая фаза ограничена 90 секундами и 1 ГиБ суммарной рабочей
памяти и private bytes процесса с запускателем. Резерв системе составляет
256 МиБ физической памяти и 512 МиБ commit. Доступны параметры
`MaximumSeconds` и `MaximumWorkingSetMiB`. Принудительная остановка или
исчерпание резерва дают `FAIL`; сохранённое состояние не возобновляется.

`result.json` объединяет завершение экспорта и чтения. `verdict.json` содержит
проверки состава USD, временных значений, материалов и независимого расчёта
деформации. При ошибке сохраняются частичные сцены и диагностические журналы.
Наличие USD-файла само по себе не означает успешный результат.
Числовые сравнения отвергают `NaN` и бесконечность в любой компоненте массива,
включая эталонные значения; они не могут дать успешный результат из-за
особенностей вычисления максимальной ошибки.

## Покрытие

- Четыре кости палитры и 1–4 влияния на вершину, включая отрицательные веса
  и суммы, отличные от единицы.
- Явные индексы костей (`Skin`) и стандартные индексы D3D9 без исходного
  индексного буфера (`DefaultJoints`); оба сценария используют один executable.
- Три кадра, вращения на 45° и их сочетание с масштабом по разным осям.
- Скрытые прототипы, экземпляры на всём интервале и экземпляры только
  на среднем кадре; отдельная трансформация каждого экземпляра.
- Удаление неиспользуемых вершин с сохранением топологии, UV, цветов,
  нормалей, весов и индексов костей.
- Типы и значения 33 констант Opaque, схема привязки материала,
  разрешение 12 относительных ссылок и содержимое собственных DDS.

Входные матрицы и ожидаемые вершины рассчитываются независимо в C++ и Python.
Проверка использует настоящий `UsdSkel`, включая составные ссылки на скелет
и источник анимации. Сравнение выполняется на захваченных временных отсчётах.

Тест проверяет CPU-часть экспорта. Capturer, передача по bridge, GPU-выгрузка
текстур, игровые сцены и повторный рендер USD остаются отдельными сценариями.
Проверка нормалей здесь относится к сохранению массива, а не к совпадению
его деформации с Fixed.rfx. При масштабе стандартный USD может давать другое
[направление нормали](../../docs/usd-capture.md#нормали-после-деформации).

Сценарий `Dynamic` сравнивает 16 сцен с запросом сокращения и без него:
постоянный и меняющийся набор используемых вершин, увеличение и уменьшение
длины массивов, постоянный цвет сетки и изменение числа треугольников.
При постоянной длине позиции заданы на отсчётах 0, 1 и 3, индексы и остальные
атрибуты — на 0 и 2. При изменении длины массивов все атрибуты заданы на 0 и 2.

Проверка сравнивает конечные атрибуты треугольников на отсчётах
0, 0,5, 1, 1,5, 2 и 3. Она различает линейную интерполяцию и удержание массива,
проверяет синхронность двух массивов топологии и длину атрибутов с учётом
`constant` или `vertex`. Смена режима интерполяции цвета между отсчётами
в этот сценарий не входит.

`VariableSkin` и `VariableSkinDefaultJoints` используют отдельный executable.
В сценах с равномерным масштабом палитры массивы растут с 6 до 8 вершин,
с неравномерным — уменьшаются с 8 до 6. Позиции, цвета, веса и индексы заданы
на отсчётах 0 и 2, позы костей — на 0, 1 и 2. Проверка учитывает удержание
исходных массивов на среднем кадре и независимо вычисляет деформацию
с изменяющимися весами, включая отрицательные значения. Проверяются все атрибуты, видимость,
1–4 влияния и сохранение полных массивов при запросе сокращения.
Интерполяция между записанными позами костей этим тестом не подтверждается.

`SignedAdapter` вызывает общий адаптер WinxRemix, затем реальный экспортёр.
Модули весов, положительная и отрицательная палитры и дополнительная нулевая
кость проходят запись и повторное чтение. Исходные 1–4 влияния становятся 2–5;
порядок девяти костей, неизменность весов и индексов между позами и результат
деформации проверяются отдельно. Эталон позиций использует исходные знаковые
веса. Для этого сценария нужен дополнительный `zero-scale-v1.patch`.
Запускатель сохраняет также исходники общего адаптера и его зависимостей.

`SignedAdapterStatic` использует тот же executable и сохраняет только среднюю
позу. Интервал сцены — один кадр; камера, экземпляры и анимация записываются
значениями USD по умолчанию, без временных отсчётов. Проверка отдельно требует
этого способа записи и сравнивает деформацию с поворотами и масштабом, включая
положительную, отрицательную и нулевую части палитры.
