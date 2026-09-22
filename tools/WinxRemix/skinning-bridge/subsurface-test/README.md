# Передача материала Subsurface

[subsurface-wire-v1.patch](../subsurface-wire-v1.patch) дополняет передачу
`MaterialInfoOpaqueSubsurfaceEXT`: профиль диффузии, RGB-радиус, масштаб радиуса,
предельный радиус выборки и путь текстуры радиуса. До исправления эти поля
не входили в wire payload, хотя присутствовали в публичной структуре API.

Декодер обнуляет начальное состояние перед выделением памяти и освобождает
все четыре принадлежащих ему пути через `delete[]`. Указатель `pNext` остаётся
локальной связью цепочки; его значение между процессами не передаётся.

## Подключение

Pin, зависимости патча и SHA256 исходников с окончаниями LF указаны в
[subsurface-manifest.json](../subsurface-manifest.json). В отдельной копии
закреплённого upstream сначала применяют перечисленные там патчи Skin и
остановки bridge, затем новый патч:

```powershell
git -C <fresh-source> -c core.autocrlf=false apply --check <absolute-path-to-subsurface-wire-v1.patch>
git -C <fresh-source> -c core.autocrlf=false apply <absolute-path-to-subsurface-wire-v1.patch>
```

Нужно собрать и заменить **оба** компонента моста: x86 client и x64 server.
Версия протокола становится `remix-main-skinwire-v1+material-v1`; полный тег
сборки дополнительно содержит pin. Подмена только одного компонента недопустима.
Сборочные переопределения версии должны сохранять новый тег на обеих сторонах.
Renderer и его патчи экспортёра собираются отдельно.

## CPU-проверка

[Стенд](../test_subsurface_wire.cpp) компилируется с настоящим
`util_remixapi.cpp`. Ему нужны Windows, MSVC, Meson и две неизменяемые копии
исходников: до и после нового патча. Проверочные сборки помещают в новые
каталоги. В окружении MSVC x86 выполняют:

```powershell
meson setup <fresh-test-x86> tools/WinxRemix/skinning-bridge/subsurface-test --wrap-mode nodownload -Dbaseline_source=<absolute-baseline> -Dcandidate_source=<absolute-candidate>
meson compile -C <fresh-test-x86> -j 1
```

В отдельном окружении MSVC x64 повторяют команды с новым `<fresh-test-x64>`.
Используется `vc++17`, как у закреплённого upstream: режим строгого поиска
имён в шаблонах меняет совместимость его serializer headers.

Собранные программы принимают режим и путь нового файла:

| Исполняемый файл | Режим | Файл |
| --- | --- | --- |
| x86 `material_baseline.exe` | `--baseline` | `baseline-x86.bin` |
| x64 `material_baseline.exe` | `--baseline` | `baseline-x64.bin` |
| x86 `material_candidate.exe` | `--write` | `wire-x86.bin` |
| x64 `material_candidate.exe` | `--read` | `wire-x86.bin` |
| x64 `material_candidate.exe` | `--write` | `wire-x64.bin` |
| x86 `material_candidate.exe` | `--read` | `wire-x64.bin` |

Каждый процесс должен завершиться с кодом 0; в его JSON должны быть нулевые
`errors`, `remainingAllocations` и `wrongDeleteForms`. SHA256 двух baseline-файлов
должны совпасть между собой, как и SHA256 двух candidate-файлов. Вывод и файлы
сохраняют отдельно; программа записи не защищает существующий файл от замены.

Проверка охватывает все комбинации наличия четырёх путей, отличие пустой строки
от отсутствующего пути, UTF-16, числовые значения, независимый байтовый эталон
и каждый отказ выделения памяти при декодировании. Baseline подтверждает
отсутствие новых полей в старом wire. Его неполный декодер не вызывается.

Это проверка обмена файлами между архитектурами. Она не подтверждает IPC,
загрузку текстур GPU или USD-захват; эти проверки остаются отдельным этапом.
Обработка повреждённых или обрезанных material payload этим патчем не добавлена.
