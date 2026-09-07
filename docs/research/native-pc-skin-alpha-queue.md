# PC whole Skin → alpha queue and pre-callback gate (CP83)

2026-09-07; прежний pinned pristine PC EXE, whole491170 owning DXMaterial,
двух StdLayer и Node из synthetic SMO/FAT. Material pass field3=1 в самих
входных bytes. Actual DXMesh factory/setter получает явную sphere1/2/3/1;
actual RenderNode и явный camera cache используются original454C30.

**9 exact captures /202 native assertions**,124 default source assertions
в расширенном SkinRenderTests. Modes queued/depth-only/queue-disabled/
capacity/already-flushing/alpha-disabled/object-disabled/zero-blend/priority-wrap.
Whole read70998 instructions, gate48..272, peak54224 bytes,34 engine owner
generations freed. Renderer backing тоже внутри прежней64KiB arena.
CP77 material protocol regression5/5 успешна после переноса pre guard.

Original46A240→423FD0 проверяет material first-pass blend, object alpha18,
renderer active44 и global alphaC9D8 **до callback**. Если нужна очередь,
вызывает454C30 и всегда возвращает AL0, игнорируя его результат. Disabled45
возвращает1 без чтения Skin/support/camera. Capacity2048 проверяется после
sphere getter/двух transforms/distance; record/count не изменяются при отказе.
Queue24 borrows camera/support/Skin, priority складывается с48 по uint32;
camera231 выбирает z² отдельно от serialized Is2D. Record flag20 — exact
spParticleSystem5AFA1A4F. В этой композиции actual Skin даёт false.

Skin sphere virtual46A230 возвращает mesh58+18 без NULL guard, в отличие
от Model479D20. Предварительный NULL-mesh fixture остановлен на read18;
исправленный новый fixture создаёт настоящий DXMesh и вызывает setter.
Source сохраняет защиту от NULL через существующий Model bounds facade;
это отличие host policy, не оригинальная гарантия.

Source spRenderer::EnqueueAlphaForAnalysis содержит borrowing records,
capacity/disabled gates и существующую CPU metric. Явный camera view/231
carrier не выдаётся за восстановленную stateful Camera cache API. Skin
alpha gate перенесён до callback и draw-resource guards; queued objects не
требуют уже подготовленных shaders/device/palette. Для nonqueued modes
явный pre callback возвращает0, поэтому full transparent draw/flush здесь
не заявлен. Неизвестный RTTI/NULL и исключительные numeric inputs остаются
guarded/открытыми; все прежние execution/allocation limits сохранены.

```powershell
python research/native_workbench.py run pc-skin-alpha-queue --deadline-utc 2026-09-07T16:00:00Z
```
