# Общая версия CRT sort для geometry и Alpha

Реализовано [предложение](tool-shared-sort-dependency-proposal-2026-09-10.md):
одна portable зависимость `msvcr71_7_10_7031_4::Sort`, перенесённая по CFG
`7C382650/7C3825E0`. DLL не включается в приложения. Версия 7.10.7031.4 выбрана
как явная host policy по разрешению пользователя 11 сентября; историческая
версия DLL из поставки игры по-прежнему не установлена.

`LegacySortPolicy` передаёт общий sorter двум существующим callbacks:
`GeometryHelper4604F0` и `spRenderer::AlphaDispatchForAnalysis`. Сохранены
max-to-high shortsort, граница 8 элементов, pivot/swap order, порядок всех
comparisons и целые записи. Не добавлены stable ties или замена comparator.
Alpha comparator оригинала не возвращает ноль даже для равных ключей; выбранная
зависимость принимает этот контракт без требования strict weak ordering.

Один свежий guest выполнил десять малых случаев настоящими инструкциями DLL и
исходными PC comparators `4607F0/454800`: 0/1, 8/9 элементов, duplicates,
all-equal, unsigned priorities, particles, повторные pointers, NaN и signed zero.
Portable результат совпал побайтно; совпали **все 222 пары аргументов comparator**
в исходном порядке. Общий weld сохранил duplicate5 representative ID4, конечные
IB/VB и allocation192; Alpha flush проверен с целыми host records и прежним
adjacent support reuse. Это не новый full original Alpha frame capture.

Guest пакет занял 2,839 s; лимиты 100k instructions/2 s на вызов, 30 s на процесс,
один worker сохранены. Native suites GeometryHelper/Msvcr71Sort прошли 2/2.
Sources, binaries и captures: [evidence](../../research/tools-core-sort-policy-2026-09-11.json).
На уровне приложений policy уже подключена к [Occlusion reader](tool-occlusion-core-2026-09-11.md).
Полный renderer должен явно использовать тот же Alpha callback при подключении
очереди. EXE score за выбор host dependency не увеличивается.
