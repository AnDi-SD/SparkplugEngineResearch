# PC Skin: сохранённый граф read → render

После целиком выполненных FAT `466B90` и Skin reader `491170` тот же Skin,
его Node и обе выделенные таблицы используются полным `46A240` до загрузки
shader constants и indexed draw. Common resolver `4678B0` и Node reader
`463A70` выполняются оригинальными инструкциями. Input содержит position
`{1,2,3}`, scale `{2,3,4}` и inverse-bind translation `{5,6,7}`. Палитра
получает translation `{11,20,31}`; сравниваются все её байты и device events.
