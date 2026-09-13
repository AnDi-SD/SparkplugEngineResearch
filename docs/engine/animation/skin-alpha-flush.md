# PC alpha flush → actual RenderNode → owning decoded Skin refusal

Actual4248D0 публикует C190 light-cache pointer до SetWorld, если C9C4=0.
Опция preserve сохраняет явный прежний pointer. После SetWorld копирует
world sphere из support24..30 в C9C8..D4; COM наблюдает прежнюю sphere и уже
новый light pointer. Dirty/current matrix также сверены побитно.
