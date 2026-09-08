# PC: целый gem.smo, две текстуры и UV-контроллер

CP114, 8 сентября 2026. Неизменённый `SFX/gem.smo` загружен оригинальным
`422B50` и общим C++ reader. Совпали 9 объектов, связи по FAT ID и 4414 байт:
802 состояния объектов, 170 material layers, 3442 mesh/declaration/mip data.
Четыре последующих UV updates совпали по дополнительным 288 байтам матриц,
двух часов контроллера и времени всех семи скалярных функций.

Файл 3878 байт, SHA-256
`4F192B68087AF09AFBBE4688CDCED49B279611E16780AEA824E7C640033F813F`.
Оригинальный PC EXE закреплён hash из [манифеста CP114](../../research/native-cycle-checkpoint-2026-09-08-cp114.json).

## Граф и сравниваемое поведение

Node ID1 владеет RenderNode ID2, тот ссылается на Model ID3. Model связан с
Material ID4, Fog ID8 и Mesh ID9. Два material layers используют DXTexture
ID5 `vcolor` (8×8) и ID6 `cryst_hl` (16×16). Material также удерживает
UVController ID7. Обратная ссылка контроллера проверена по точному номеру
материала, pass и layer; это заимствованная связь, а не дополнительное владение.

Полный capture включает enabled, applied/accumulated time, saved 3×3 matrix,
восемь float-полей, clamp byte и type каждого FunctionEval, pivot и axis.
Незаписанные padding bytes не включены. Начальное состояние сравнивается до
исполнения updates. Затем original `423190 → 434820` и восстановленный runtime
получают последовательность delta `[0, 0.25, 0.75, -0.5]`. На каждом шаге
сравниваются 9 значений матрицы holder, оба clock и 7 function times.
Это продолжение [UV runtime CP22](native-pc-uv-functions.md) на реальном целом
файле; renderer submission и display в этом capture не выполняются.

В каждой текстуре исходно один raw mip. Actual `4ABBA0 → 4AB030 → 61039A →
60FDB4` строит остальные: всего 9 уровней и 7 вызовов фильтра. Сравнены все
1704 texture bytes, включая 424 сгенерированных. Применён общий
[missing-mip алгоритм](native-pc-texture-missing-mips.md).

## Границы исполнения и проверка

Whole load: 395857 инструкций, около 2,65 с в отдельном запуске. UV update:
не более 1938 инструкций. Arena 91600 байт; все 197 actual native allocations
освобождены. Контроллер исключён из original AnimationManager list при
уничтожении графа. Texture/surface/buffer refs и locks сбалансированы.

До нового запуска явно заданы две независимые COM texture identity на одном
device, стороны 8×8/16×16, до 5 уровней на texture, до 2048 байт на surface,
padding 4 байта на строку. Device refs считаются одним общим владельцем;
texture и surface refs — отдельно. CreateTexture допускает только ещё не
созданную объявленную текстуру подходящего размера. Engine algorithms
исполняются оригиналом; COM boundary предоставляет память и интерфейс.

Сохранены file profile 1 млн инструкций / 8 с, fresh child 30 с, arena 128 КиБ,
native allocation 32 КиБ; выбран существующий mesh buffer preset 8 КиБ.
RTTI, пустые managers и отсутствующие внешние debug/registry inputs остаются
явными fixtures. Полный CRT startup не проверен этим результатом.

```powershell
python research/probe_pc_scene_file_profile.py gem --file-ids
python research/probe_pc_scene_file_profile.py gem-animated --file-ids
python research/native_workbench.py run pc-scene-file-profile --workers 4
```

Два новых случая имеют разные имена отчётов и независимые fresh guests.
Общий profile прошёл 9/9 случаев. Source scene CTest 1/1 и workbench unit suite
16/16 проверены после изменения capture. Это дополнительная целая композиция
восстановленных классов; class scores не повышены. Whole native save,
controller clone parity, произвольные внешние ресурсы и общий render frame
остаются открытыми.
