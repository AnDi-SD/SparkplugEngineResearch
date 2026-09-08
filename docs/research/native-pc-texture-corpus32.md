# CP119: реальные32×32 textures и whole rock.smo

8 сентября2026. Текстура из неизменённого `icebat.smo` и целый
`Levels/Gardenia/rock.smo` прошли original/source сравнение, включая достроенные
уровни16×16…1×1. Ни один engine helper для этого не заменён.

## Texture slice icebat

Исходный файл35571 байт, SHA-256
`6AEC9CA21EB50FD93E89955551C3557FF19BD21260038FF6E7CF94EEF178BF72`.
TextureData object `[1336,5496)`, class78EA082B/SBOO; reader получает исходный
payload `[1344,5496)`,4152 байта, SHA-256
`58ECE7E72E89CB88DE1B8C8E455EA8CBC54FC36BC88CDB50CE17E5E2E8693EC3`.
Source field3 содержит вложенный native payload с одним32×32 RGBA уровнем.

Original42C640 читает обе source sections и выполняет пять61039A/60FDB4/61C44F
фильтраций. Совпали5460 mip bytes, включая1364 generated, base state и cursor.
404734 инструкции, около1,07 с; arena75184 байта,60 native allocations полностью
освобождены. Padding каждой COM surface сохранился. Это texture slice,
не загрузка целой модели icebat.

## Whole rock.smo

Неизменённый файл6364 байта, SHA-256
`E5CA7A422396DBB2FB7964A248B27221B3CD9761F06611A9D05BB38A5B158034`.
Семь объектов: Node, RenderNode, Model, MaterialData, TextureData, Fog, MeshData.
Полный original422B50 материализует граф, вызывает DX mesh hook, создаёт
32×32 texture с шестью уровнями и очищает FAT. Source capture совпал по
7675 bytes:448 state,85 material-layer,7142 texture/mesh/declaration buffers.

601092 инструкции/около1,96 с whole load,65 assertions; arena97504 байта.
Все146 native allocations и COM references освобождены. Выбран существующий
8-КиБ COM-buffer fixture для mesh и одна объявленная32×32 texture.
Factory/RTTI/device inputs остаются явными: full startup/live rendering не закрыты.

## Ресурсы стенда и поиск в корпусе

`MissingMipFixture` получил отдельный явный профиль `corpus32`: сторона≤32,
шесть уровней,≤8192 bytes/surface. Default `tiny` остаётся16/5/2048; оба
варианта сохраняют arena128 КиБ, allocation32 КиБ,1M instructions/8 с и30 с
на свежий дочерний процесс. Source frontend допускает≤8 КиБ входного payload
и шесть bounded capture levels. Сам renderer/codec не изменён.

Просмотр всех TextureData field shapes PC corpus2 не нашёл внешних field4;
поэтому [CP118](native-pc-texture-external-source.md) не получает corpus credit.
Отдельный поиск compressed witnesses среди501 embedded texture rows≤16384 bytes
прочитал129 уникальных payloads. После проверки platform bit и layout
валидных DXT1/3/5 случаев в этом срезе не найдено. Первые похожие headers
принадлежали PS2 menu assets, присутствующим в PC Media; они не приняты за PC
codec evidence. Это ограниченный поиск, не доказательство отсутствия во всём корпусе.

```powershell
python research/compare_pc_texture_missing_mips.py icebat 32x32
python research/probe_pc_scene_file_profile.py rock --file-ids
```

[Манифест CP119](../../research/native-cycle-checkpoint-2026-09-08-cp119.json)
сохраняет результаты и fingerprints. Profiles теперь11 mip cases и10 whole-scene
cases. Class scores за эти композиционные проверки не повышены.
