# Анимация и скелеты

Именованные кривые связываются с узлами; actor и контроллеры управляют воспроизведением, skin применяет палитру костей. Дисковые кривые описаны в [SAN](../../formats/san.md).

<!-- catalog:start -->

## Статьи

- [Decoded light first-generation bounded candidate](skin-decoded-light-generated-boundary.md).
- [GUIObject: поиск GUICollision и история состояния контроллеров](gui-controller-binding.md).
- [MovieTextureController и конкретные VideoStream](movie-controller-runtime.md).
- [PC `spActor`: playback, события и граница менеджера](actor-playback.md).
- [PC `spAnimation`, `spTrack`, `spAnimTrack`: object lifetime](animation-lifecycle.md).
- [PC actor: capacity, queries и fade-stop](actor-controls.md).
- [PC actor: discovery, input insertion и запуск](actor-binding.md).
- [PC actor: full-capacity Start и fade-stop до реального Tick](actor-control-pipeline.md).
- [PC actor: перенос owned runtime и сквозная проверка](actor-owned-runtime.md).
- [PC actual selected light cache through complete Skin draw](skin-selected-light.md).
- [PC alpha flush → actual RenderNode → owning decoded Skin refusal](skin-alpha-flush.md).
- [PC animation runtime: SAN → node → skin palette](animation-runtime.md).
- [PC FunctionEval: scalar runtime and shared codec (checkpoint21)](function-eval.md).
- [PC queued Skin first-generation: bounded failure record](skin-queued-generated-boundary.md).
- [PC real SAN → retained Skin → first shader generation](skin-san-generated-render.md).
- [PC real SAN/scene world + decoded mesh → Skin draw](skin-scene-mesh-render.md).
- [PC SAN actor → прочитанная кость → Skin palette/draw](skin-san-render.md).
- [PC SAN → scene world → Skin shader generation/draw](skin-scene-generated-render.md).
- [PC SAN: payload → descriptors → preparation → sampling](animation-keys.md).
- [PC Skin-owned Fog/material/bone → SAN/scene/mesh draw](skin-fog-render.md).
- [PC Skin: actual light world to shader constants](skin-light-constants.md).
- [PC Skin: complete light submission in decoded draw](skin-lights-render.md).
- [PC Skin: palette -> mesh -> shader constants -> draw](skin-render.md).
- [PC Skin: сохранённый граф read → render](skin-loaded-render.md).
- [PC unchanged SMO light through complete lit Skin draw](skin-decoded-light.md).
- [PC прочитанный Skin → generating shader miss → draw](skin-generated-render.md).
- [PC: queued Skin draw and mesh bounds](skin-queued-mesh-render.md).

<!-- catalog:end -->
