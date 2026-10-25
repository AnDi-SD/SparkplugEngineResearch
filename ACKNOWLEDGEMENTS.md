# Благодарности и происхождение вкладов

Общие благодарности участникам исследования, разработки и тестирования
SparkplugEngineResearch и его инструментов.

## Создатель проекта

AnDi~SD aka @andisd — создатель SparkplugEngineResearch, автор SmoViewer
и других инструментов проекта:

- [SmoViewer — исходники](https://github.com/AnDi-SD/SparkplugEngineResearch/tree/main/tools/SmoViewer);
- [SparkplugEngineResearch — исследовательский репозиторий](https://github.com/AnDi-SD/SparkplugEngineResearch).

## Тестирование

Спасибо тестировщикам, которые помогали находить проблемы, проверять модели и
оценивать изменения в инструментах:

- Sumthini aka @SionFane;
- Butermix aka @Butermix;
- DarkExp aka @expdark;
- Ty-Ty aka @UUSTYTSTY;
- The New Moon aka @TheNewMoonNM.

## Сообщество

Отдельное спасибо сообществу `D:\Games\Winx` за помощь, обсуждение проекта и
многолетнюю любовь к игре:

- Telegram: [@dgameswinxTG](https://t.me/dgameswinxTG);
- VK: [vk.ru/dgameswinx](https://vk.ru/dgameswinx).

И, конечно, спасибо всем, кто ждёт новые версии, делится обратной связью и
поддерживает развитие проекта. Именно ваш интерес помогает ему становиться лучше.

## kotwys — STX GIMP plugin и Splendente

Спасибо `kotwys`, автору независимого плагина STX для GIMP, за то, что он
поделился исследованием и исходным кодом, а также разрешил изучать их и
использовать полезные выводы при развитии инструментов проекта:

- [kotwys/stx-gimp-plugin](https://github.com/kotwys/stx-gimp-plugin).

Также kotwys развивает [Splendente](https://github.com/kotwys/splendente) —
библиотеки и инструменты для работы с ресурсами Sparkplug, включая текстуры
Winx Club на PC и PlayStation 2. Спасибо за дальнейшее исследование форматов
и развитие независимых инструментов для игры.

## PCSX2 Dev Team — VU disassembler

Спасибо команде [PCSX2](https://github.com/PCSX2/pcsx2) за открытый
дизассемблер VU microinstructions. Отдельный инструмент ResearchWorkbench
использует его закреплённые таблицы и функции декодирования; происхождение,
лицензия GPL-3.0-or-later и локальные изменения указаны в
[манифесте](tools/ResearchWorkbench/ps2_vu_decoder_source.json).
Инструмент не связан с engine-библиотекой проекта.

## Butermix

Butermix предоставил изменённую копию раннего SmoViewer со следующими идеями и
исправлениями:

- прототип полного batch export сцены и отдельных mesh в GLB;
- исправление вызова SharpGLTF с устаревшего `ToGltfModel()` на `ToGltf2()`;
- заготовка отдельного UV Editor;
- эксперименты с командами экспорта в GUI.

Копия была проверена 2026-08-11. UV Editor в ней пока не рисует UV, экспорт
текстур отключён, а world transforms вычисляются по старой catalog-parent цепочке
без актуальной логики `esfNodeChild` и bind-world. Поэтому исходный интерфейс и
batch exporter не копировались. Их направление учтено в развитии проекта, а
production export остаётся в отдельном `SmoExporter`, построенном поверх текущего
strict decoder.
