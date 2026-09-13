# SMO Texture Tool 2.1

**Texture extractor and repacker for Winx Club PC `.smo` models**<br>
**Инструмент для извлечения и замены текстур в `.smo`-моделях Winx Club для ПК**

> [!WARNING]
> **The GUI writer/repacker remains disabled.** Version 2.1 fixes the parser bug
> behind the former game crash, but resized/repacked files have not completed a
> native validation pass. Use this application only to inspect and export
> textures. For the game-tested fixed-size RGB replacement path, use
> `tools/SmoImporter`.
>
> **Запись/repack в GUI остаются отключены.** Версия 2.1 исправляет ошибку parser-а,
> из-за которой прежние файлы вызывали вылет игры, но resize/repack ещё не прошёл
> отдельную нативную проверку. Используйте программу только для просмотра и
> экспорта текстур. Подтверждённая в игре fixed-size замена RGB реализована в
> `tools/SmoImporter`.

---

## 🌐 Language / Язык

- [English](#english)
- [Русский](#русский)

---

# English

## 📌 Description

**SMO Texture Tool 2.1** is a desktop utility for inspecting embedded textures in
model files from the PC version of **Winx Club**.

Its currently supported read-only workflow can:

- find and preview every supported texture in an SMO file;
- export one texture or the complete set as PNG;
- preserve the BGRA channel layout used by supported game textures;
- inspect RGB and Alpha channels separately;
- preview grayscale textures with the vertex colors of their actual model;

Texture replacement, resizing and SMO repacking are retained only as research
code. Their GUI controls are disabled and their output must not be used in-game.

Version 2.1 corrects the `0x32E3`/`0x43E3` layout: byte `+0x3C` is a required
serializer marker, while BGRA pixels start at `+0x3D`. It also fixes the
rectangular-texture height mirror and adds strict malformed-header and
byte-identical round-trip regressions. A corrected fixed-size full-BGRA control
file passed the native loader; resize/repack has not been game-validated.

Version 2.0 has been rewritten with **Avalonia UI** and no longer depends on the
Visual Studio WinForms designer.

## 📥 Download

Download the latest build from:

https://github.com/AnDi-SD/SMOTextureTool/releases

The standardized workspace release uses a framework-dependent single-file build.
If Microsoft .NET 8 Desktop Runtime (x64) is missing, its root launcher offers
to download the official signed Microsoft installer and installs it silently.

## 🚀 How to use

### 1. Open an SMO

Click **Open SMO** and select a model file. The original file is only read and
is never overwritten automatically.

### 2. Inspect and export textures

Use **Save PNG** for one texture or **Extract all** for the complete set.

The preview selector can show:

- RGBA on a checkerboard;
- raw RGBA;
- RGB without transparency;
- the Alpha channel;
- **Model coloring**, which multiplies grayscale texture data by the diffuse
  vertex colors of its linked mesh.

### 3. Edit textures externally

Edit the exported PNG files in an image editor. Keep their filenames if you
want to load a complete replacement folder.

Grayscale model-colored textures should normally remain grayscale. Their pink,
yellow, or other final colors are stored in the model vertices and applied by
the game with `D3DTOP_MODULATE`.

### 4. Replacement (disabled)

The former replacement controls remain disabled while the corrected resize and
repack path awaits a separate native validation pass.

Replacement images may be PNG, BMP, or JPEG. Both dimensions must be powers of
two.

### 5. Save a new SMO (disabled)

Do not use the old repacker for game files. Use `SmoImporter`'s fixed-size RGB
writer, which preserves the original alpha bytes, headers, offsets and file size.

## 🖼 HD textures (historical, not validated)

- Safe mode allows textures up to `4096×4096`.
- Experimental mode allows up to `16384×16384`.
- Earlier claims about successful `1024×1024` and `2048×2048` replacements are
  superseded and require independent revalidation.
- Texture data is stored uncompressed at four bytes per pixel, so very large
  replacements substantially increase both file size and video-memory usage.

## ⚠️ Important notes

- Always save to a new SMO and keep the original file.
- The colored UV preview is diagnostic. Overlapping UV surfaces can use the same
  texture pixels with different vertex colors, so a single flat preview cannot
  reproduce every 3D surface simultaneously.
- Model coloring changes preview only. Export and repacking always use the raw
  texture, preventing colors from being applied twice.
- Only the texture layouts documented in
  [`docs/SMO_FORMAT.md`](docs/SMO_FORMAT.md) are modified.

## 🛠 Development

Requirements:

- .NET 8 SDK or newer;
- Windows, Linux, or macOS for development.

```powershell
dotnet restore SMOTextureTool.slnx
dotnet build SMOTextureTool.slnx
dotnet run --project SMOTextureTool/SMOTextureTool.csproj
dotnet run --project SMOTextureTool.FormatTests/SMOTextureTool.FormatTests.csproj
```

Windows x64 releases are built from the workspace root with the shared packager:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
  ./release/Build-Releases.ps1 -Product SMOTextureTool
```

Project structure:

- `SMOTextureTool` — Avalonia desktop interface;
- `SMOTextureTool.Core` — SMO parsing, texture decoding, repacking, material
  graph, and vertex-color preview;
- `SMOTextureTool.FormatTests` — regression tests using local sample models;
- `docs/SMO_FORMAT.md` — reverse-engineered SMO format notes.

## 📜 License

Copyright © 2026 AnDi-SD.

Original project: https://github.com/AnDi-SD/SMOTextureTool

Licensed under the GNU General Public License v3.0 or later. You may use,
redistribute, and modify the program under its terms. When distributing copies
or modified versions, preserve the copyright, license, and origin notices.
See `COPYRIGHT.txt` and `LICENSE.txt`.

---

# Русский

## 📌 Описание

**SMO Texture Tool 2.1** — read-only программа для исследования встроенных
текстур в файлах моделей ПК-версии **Winx Club**. Запись файлов отключена:
результаты прежнего repack могли проходить проверку программы, но вызывать
вылет оригинальной игры.

Возможности:

- поиск и просмотр всех поддерживаемых текстур внутри SMO;
- сохранение отдельной текстуры или всего набора в PNG;
- сохранение BGRA-раскладки каналов поддерживаемых игровых текстур;
- отдельный просмотр RGB и Alpha;
- просмотр серых текстур с цветами вершин их настоящей модели;

Код замены и repack сохранён только для исследования формата. Для подтверждённой
игрой fixed-size замены RGB используйте `tools/SmoImporter`.

В версии 2.1 уточнена раскладка `0x32E3`/`0x43E3`: байт `+0x3C` является
обязательным marker-ом сериализатора, а BGRA-пиксели начинаются с `+0x3D`.
Исправлено зеркало высоты прямоугольной текстуры и добавлены строгие проверки
повреждённого заголовка и побайтного round-trip. Исправленный fixed-size
full-BGRA контрольный файл прошёл нативный загрузчик; resize/repack игрой ещё не
проверялся.

Версия 2.0 полностью переписана на **Avalonia UI** и больше не зависит от
WinForms Designer в Visual Studio.

## 📥 Скачать

Последняя версия:

https://github.com/AnDi-SD/SMOTextureTool/releases

Корневой загрузчик Windows x64 сам проверяет Microsoft .NET 8 Desktop Runtime.
Если компонента нет, он предлагает скачать подписанный установщик Microsoft и
выполняет тихую установку; Windows может показать стандартное подтверждение UAC.

## 🚀 Использование

### 1. Открыть SMO

Нажмите **Открыть SMO** и выберите файл модели. Исходный файл только читается и
никогда не перезаписывается автоматически.

### 2. Посмотреть и извлечь текстуры

Кнопка **Сохранить PNG** сохраняет одну текстуру, **Извлечь все** — полный
набор.

Переключатель предпросмотра поддерживает:

- RGBA на шахматном фоне;
- исходный RGBA;
- RGB без прозрачности;
- Alpha-канал;
- **Окраску модели** — умножение серой основы на diffuse-цвета вершин
  действительно связанного меша.

### 3. Отредактировать изображения

Измените PNG в любом графическом редакторе. Не переименовывайте файлы, если
планируете загружать сразу всю папку замен.

Окрашиваемые серые текстуры обычно следует оставлять серыми. Розовый, жёлтый и
другие итоговые цвета находятся в вершинах модели и накладываются игрой через
`D3DTOP_MODULATE`.

### 4. Замена текстур — отключена

Элементы выбора замен остаются отключены до отдельной нативной проверки
исправленного resize/repack-пути.

### 5. Сохранение нового SMO — отключено

Не используйте прежний repacker для игровых файлов. `SmoImporter` сохраняет
исходные Alpha, заголовки, смещения и размер файла, меняя только RGB существующего
pixel buffer.

## 🖼 HD-текстуры — историческая гипотеза

- Прежние пределы `4096×4096` и `16384×16384` относятся только к возможностям
  parser/repacker и не означают совместимость с игрой.
- Заявления о проверке замен `1024×1024` и `2048×2048` считаются недействительными
  до независимого повторного подтверждения.
- Текстуры хранятся без сжатия по четыре байта на пиксель, поэтому очень большие
  изображения заметно увеличивают файл и расход видеопамяти.

## ⚠️ Важно

- Используйте программу только для просмотра и извлечения; writer отключён.
- Цветная UV-развёртка является диагностическим предпросмотром. Пересекающиеся
  поверхности могут использовать одни пиксели с разными цветами вершин, поэтому
  одна плоская картинка не способна одновременно показать все стороны модели.
- Окрашивание влияет только на просмотр. Экспорт и упаковка используют исходную
  текстуру, поэтому цвет не накладывается дважды.
- Программа изменяет только раскладки, описанные в
  [`docs/SMO_FORMAT.md`](docs/SMO_FORMAT.md).

## 🛠 Разработка

Требуется .NET 8 SDK или новее.

```powershell
dotnet restore SMOTextureTool.slnx
dotnet build SMOTextureTool.slnx
dotnet run --project SMOTextureTool/SMOTextureTool.csproj
dotnet run --project SMOTextureTool.FormatTests/SMOTextureTool.FormatTests.csproj
```

Структура:

- `SMOTextureTool` — интерфейс Avalonia;
- `SMOTextureTool.Core` — разбор SMO, декодирование, пересборка, граф материалов
  и предпросмотр vertex color;
- `SMOTextureTool.FormatTests` — регрессионные тесты на локальных образцах;
- `docs/SMO_FORMAT.md` — результаты исследования формата.

## 📜 Лицензия

Copyright © 2026 AnDi-SD.

Оригинальный проект: https://github.com/AnDi-SD/SMOTextureTool

Проект распространяется по лицензии GNU General Public License v3.0 или более
поздней версии. Программу можно использовать, распространять и изменять на
условиях GPL. При распространении копий и изменённых версий необходимо
сохранять сведения об авторе, лицензии и происхождении проекта. Полные условия
находятся в `COPYRIGHT.txt` и `LICENSE.txt`.
