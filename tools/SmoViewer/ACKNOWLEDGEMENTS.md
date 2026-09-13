# Благодарности и происхождение вкладов

## Создатель проекта

AnDi~SD aka @andisd — автор SmoViewer и исследовательского проекта
SparkplugEngineResearch:

- [SmoViewer — основной репозиторий](https://github.com/AnDi-SD/SmoViewer);
- [SparkplugEngineResearch — исследовательский репозиторий](https://github.com/AnDi-SD/SparkplugEngineResearch).

## Тестирование

Спасибо тестировщикам, которые помогали находить проблемы, проверять модели и
оценивать изменения в инструментах:

- Sumthini aka @SionFane;
- Butermix aka @Butermix;
- DarkExp aka @expdark;
- Ty-Ty aka @UUSTYTSTY.

Имена пользователей приведены как обычный текст и не являются ссылками.

## Сообщество

Отдельное спасибо сообществу `D:\Games\Winx` за помощь, обсуждение проекта и
многолетнюю любовь к игре:

- Telegram: [@dgameswinxTG](https://t.me/dgameswinxTG);
- VK: [vk.ru/dgameswinx](https://vk.ru/dgameswinx).

И, конечно, спасибо всем, кто ждёт новые версии, делится обратной связью и
поддерживает развитие проекта. Именно ваш интерес помогает ему становиться лучше.

## kotwys — STX GIMP plugin

Спасибо `kotwys`, автору независимого плагина STX для GIMP, за то, что он
поделился исследованием и исходным кодом, а также разрешил изучать их и
использовать полезные выводы при развитии инструментов проекта:

- [kotwys/stx-gimp-plugin](https://github.com/kotwys/stx-gimp-plugin).

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

При подготовке следующего релиза SmoViewer вклад Butermix должен сохраняться в
release notes вместе с этим файлом.
