# SmoImporter 0.1.1

- запуск из SmoViewer с автоматически загруженным исходным SMO;
- импорт целой OBJ/GLB-сцены и детерминированная нарезка по существующим mesh slots;
- выбор rigid bone palette, подгонка transform и 3D preview;
- embedded GLB base-color и внешний PNG/JPEG;
- безопасная fixed-size RGB замена atlas с сохранением Alpha и структуры SMO;
- исходный SMO никогда не перезаписывается, результат повторно проверяется strict parser.

Изменение разрешения atlas, texture repack, импорт новой скелетной деформации и анимаций не поддерживаются.
