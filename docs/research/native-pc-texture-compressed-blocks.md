# CP111: PC DXT block decode и original missing-mip путь

8 сентября 2026. Восстановлены три декодера сжатых блоков: 438 native/source
случаев совпали по 28 032 binary32 словам. Проверены оба порядка цветовых и
альфа-концов, все индексы палитр, крайние значения и 384 seeded random блока.
Это самостоятельный слой C++ в
[spTextureBlockCodec.h](../../Sparkplug/Analysis/PC/spTextureBlockCodec.h);
генерация сжатых mips в C++ ещё требует original encoder.

## Выполненные оригинальные функции

PC EXE SHA-256:
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

| Формат | Decode block | Encode block, пока не перенесён |
|---|---|---|
| DXT1 | `64C493` | `64C798` |
| DXT3 | `64C5D5` | `64C8BC` |
| DXT5 | `64C65A` | `64C9EB` |

Таблицы actual codec получены read-only hook в `61C647`: row reader `627873`,
row writer `627452`, destructor `627B1B`, block callbacks в +8C/+90.
`compare_pc_texture_block_decode.py` затем вызывает сами block entries на
явных 8/16-byte inputs и 256-byte output. Ни одной seam, arena 288 байт,
224 212 инструкций суммарно, 9,78 с на весь batch. Каждый вызов сохраняет
micro cap 100k/2 с, внешний процесс ограничен 30 с. Output canary не изменён.

`64B094` раскрывает RGB565 в float R,G,B,A через binary32 константы
`3D042108` (1/31) и `3C820821` (1/63). Промежуточные цвета интерполируются
через `3EAAAAAB`/`3F2AAAAB` или 0,5 с original x87 store boundaries.
Для четвёртого цвета G/B differences предварительно сохранены в binary32,
а R остаётся в x87; эта разница включена в исходник и точные сравнения.

DXT3 и DXT5 действительно вызывают **тот же `64C493`**, затем заменяют альфу.
При firstColor <= secondColor и colorIndex 3 RGB остаётся чёрным и для этих
форматов. DXT3 раскрывает nibbles через `3D888889`; DXT5 использует две
альфа-ветви и original float constants 1/255, 1/7, 1/5. Имена RGBA относятся
к float block layout; исходный raw upload имеет свой порядок байтов.

Host guard принимает ровно один блок известного формата; неверный размер,
нулевой указатель и неизвестный format не изменяют output. Это политика
ограниченного host API, не приписанная оригиналу проверка входа.

## Сжатая цепочка в оригинальном reader

Новый `probe_pc_texture_compressed_missing.py` исполняет полный путь
`42C640 → 4ABBA0 → 4AB030 → 61039A → 60FDB4 → 61C44F` с одним
заданным 4×4 блоком, создавая 2×2 и 1×1. Первый DXT1 red case завершился за
34 342 инструкции / 0,334 с; все 32 allocations освобождены, arena 57 056 байт.
Ещё шесть случаев DXT1/3/5 indices/transparent также завершились с сохранением
первого уровня, padding и баланса COM/native ownership.

Поверхности — явно заданное COM storage с padding, не live GPU. До пяти уровней,
до 2048 байт на поверхность, file profile 1m/8 с, arena 128 КиБ, allocation
request <=32 КиБ. External absent module/registry и CRT inputs наследуются
от [CP108](native-pc-texture-missing-mips.md); internal decoders, filters и
encoders не подменяются.

```powershell
python research/compare_pc_texture_block_decode.py
python research/native_workbench.py run pc-texture-compressed-blocks --workers 4 --deadline-utc 2026-09-08T04:00:00Z
```

Source Texture CTest прошёл 1/1; последняя полная C++ регрессия — CP108.
Первый compile attempt остановлен после 120 с; проверено и завершено только
его дерево процессов. Исходник приведён к принятому в проекте C++17,
следующая сборка завершилась успешно. Проверочный запуск со старым EXE
не засчитан; приведённый batch использует новый EXE с fingerprint в manifest.

Далее: `64BB40` color encoder, optimizer `64B5C0`, DXT5 alpha optimizer
`64B299`, округления и padding малых блоков. Полный compressed conversion,
DX cross upload, ошибки и live backend пока открыты. Class scores не менялись.
[Manifest CP111](../../research/native-cycle-checkpoint-2026-09-08-cp111.json).
