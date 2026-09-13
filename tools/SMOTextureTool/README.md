# SMO Texture Tool 2.2 — workspace build

Просмотр, извлечение и замена текстур Winx Club PC SMO. Версия 2.2 в рабочем
репозитории включает исправленный GUI writer; опубликованный релиз 2.1 остаётся
отдельной исторической сборкой.

## Возможности

- Просмотр встроенных BGRA-текстур, RGB/Alpha и диагностической окраски модели.
- Экспорт одной текстуры или всего поддерживаемого набора в PNG.
- Замена полного RGBA и сохранение точных размеров исходного изображения,
  включая прямоугольные размеры и размеры, не являющиеся степенями двойки.
- Сохранение отдельной проверенной копии SMO; существующая копия получает backup.

Запись включена для встроенного PC Direct3D BGRA32 с одним записанным mip-уровнем
и без второй платформенной копии. Размеры реальных полей, каталог объектов,
вложенные контейнеры и зеркала размеров обновляет общий `SmoViewer.Core`, который
использует также Importer. Нетронутые данные, ID и связи объектов сохраняются.

Cross-platform/PS2, палитры, внешние источники, смешанные представления и явно
записанные mip-цепочки не записываются этим writer. Поддерживаемые BGRA-изображения
из таких ресурсов можно просматривать; неподдерживаемые ресурсы учитываются в
сводке. Ограничение конкретного слота видно рядом с ним.

## Работа

1. Откройте SMO и при необходимости извлеките PNG.
2. Выберите замену для нужного слота или папку с PNG, сохранив имена экспортированных файлов.
3. Нажмите «Сохранить копию SMO». Исходный SMO и изображения замен защищены от перезаписи.
4. Повторно откройте полученную копию для просмотра результата.

Замена переносит полный RGBA. Режимы смешивания материалов сохраняются: наличие
Alpha в PNG само по себе не включает прозрачность материала. Окраска модели
влияет на предпросмотр; экспорт и запись используют исходные пиксели.

Обычный предел — 4096 пикселей на сторону. Расширенный — 16384 на сторону и
128 МиБ несжатых пикселей на изображение. Эти пределы ограничивают инструмент;
они не гарантируют поддержку каждого размера конкретным GPU. Изображения
обрабатываются последовательно, первый кадр декодируется после проверки размеров.

## Проверка

Короткие проверки охватывают извлечение точных PNG, побайтный roundtrip, фиксированную
замену, рост/уменьшение размеров, несколько замен и сохранность skin palette.
Полученные файлы дополнительно загружаются оригинальным PC-кодом под ограниченным
эмулятором с явно заданной памятью COM: проверяются объекты, связи и все mip-уровни.
Это не равнозначно отдельной проверке большого HD-ресурса в запущенной игре.
Avalonia-проверка выполняет открытие, режимы preview, выбор замены, save/backup,
защиту исходника и повторное открытие; системные file picker диалоги не автоматизированы.

[Структура формата](docs/SMO_FORMAT.md).

## Сборка в исследовательском workspace

Общая библиотека `SmoViewer.Core` находится в соседнем каталоге `../SmoViewer`.
Оба инструмента входят в [SparkplugEngineResearch](../../README.md);
для сборки клонируйте весь репозиторий и откройте каталог `tools/SMOTextureTool`.
Операции записи находятся в общей `SmoViewer.Editing`. Текущее рабочее дерево
также использует C++-ядро через `SparkplugViewerNative.dll`: сборка требует
Windows x64, полного SparkplugEngineResearch и Visual Studio C++/CMake.
DLL собирается автоматически и должна находиться рядом с приложением.
Raw pixels и PC mip-записи читаются восстановленными texture serializers;
отдельный FFPS header reader удалён. XRGB экспорт получает BGRA-проекцию общего
codec с alpha=255, исходные байты сохраняются. Запись одного PC BGRA mip теперь
также использует исходный C++ writer, включая NPOT dimensions и исходный
prefix byte field1C. PC source dispatch и material snapshot используют общий
reader; preview берёт выбранную им representation. Повторные, пропущенные и
непроверенные для записи source-формы остаются read-only. Для legacy-common и
PS2 сохранён явный metadata-режим; PS2 native section уже читает общий inspector,
но полный source/runtime и swizzle не заявлены. Общая запись FAT/envelope и
остальные границы редактирования ещё не закрыты; всё ядро не завершено.
Рекомендуемый SDK для Avalonia 12.1 и `.slnx` — .NET 9.0.300 или новее;
приложение рассчитано на .NET 8 Runtime.

```powershell
dotnet restore SMOTextureTool.slnx
dotnet build SMOTextureTool.slnx
dotnet run --project SMOTextureTool/SMOTextureTool.csproj
# Небольшой явный набор образцов, без полного corpus scan:
dotnet run --project SMOTextureTool.FormatTests -- --focused path/to/output path/to/gem.smo path/to/Bloom_body.smo
dotnet run --project SMOTextureTool.GuiTests -- path/to/gem.smo path/to/gui-output
```

## English

The workspace 2.2 build enables a structurally verified texture replacement UI.
It shares the catalog parser, texture writer and atomic output installer with
`SmoViewer.Core`/`SmoViewer.Editing`/Importer. Supported writes are single-mip embedded PC Direct3D
BGRA32 textures without a second platform representation. Exact image dimensions
and RGBA are preserved; material blend states remain unchanged. Unsupported
slots stay explicit. Native instruction tests cover actual generated SMOs with
bounded COM storage; this is not a blanket HD/GPU compatibility claim.

The published [2.1.0 release](https://github.com/AnDi-SD/SparkplugEngineResearch/releases/tag/smotexturetool-v2.1.0)
is separate from this workspace build. Clone the complete SparkplugEngineResearch
repository, which includes both tools and their shared libraries.

## Лицензия / License

Copyright © 2026 AnDi-SD. GNU General Public License v3.0 or later.
Сохраняйте сведения об авторе, лицензии и происхождении проекта.
Полные условия: [COPYRIGHT.txt](COPYRIGHT.txt), [LICENSE.txt](LICENSE.txt).
Исходники проекта: https://github.com/AnDi-SD/SparkplugEngineResearch/tree/main/tools/SMOTextureTool
