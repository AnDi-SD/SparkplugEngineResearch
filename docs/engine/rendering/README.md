# Отрисовка

Renderer получает преобразования, геометрию, материалы и состояние сцены. Описания PC-путей не означают тождественной реализации PS2 или нашего OpenGL backend.

<!-- catalog:start -->

## Статьи

- [CPU-подготовка кадра: данные, порядок и владение](cpu-preparation.md).
- [PC automatic draw: создание шейдера и повторное использование](renderer-generated-draw.md).
- [PC cached automatic weighted draw](renderer-cached-draw.md).
- [PC Clear, Present и жизненный цикл устройства](renderer-present-clear.md).
- [PC complete cached weighted submission](renderer-weighted-submit.md).
- [PC complete geometry submission](renderer-submit.md).
- [PC material UV → renderer matrix submission](uv-renderer.md).
- [PC preselected-shader draw and shader stacks](renderer-draw.md).
- [PC Renderable callbacks и очереди renderer](renderer-protocol.md).
- [PC renderer matrix cache](renderer-matrices.md).
- [PC renderer matrix setters/raw getters](renderer-matrix-inputs.md).
- [PC scene boundaries and real material payload](renderer-scene.md).
- [PC: модель, render-узел и точные правила копирования](model-render-world.md).
- [PC: целый SceneRender, Shadow manager и DX state cache](scene-render-runtime.md).
- [PS2: scalar-подготовка параметров pipeline](ps2-cpu-preparation.md).
- [PS2: нормализация vertices и сборка mesh packet](ps2-mesh-preparation.md).
- [Граница CPU-подготовки и команд графическому устройству](renderer-boundary.md).
- [Исходники рендеринга: реализации и границы](source-map.md).
- [Проекции: геометрия, FX и разные интерфейсы PC/PS2](projection-family.md).

<!-- catalog:end -->
