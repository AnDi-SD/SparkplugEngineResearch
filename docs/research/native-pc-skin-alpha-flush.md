# PC alpha flush → actual RenderNode → owning decoded Skin refusal (CP84)

2026-09-07; тот же pristine PC EXE, что [CP83](native-pc-skin-alpha-queue.md).
Whole491170 читает Skin/material/bone. Actual469ED0 добавляет тот же Skin
двум actual RenderNode (intrusive ref2); actual DXMesh/setter даёт sphere.
Три whole46A240 enqueue создают records с support A/A/B и priorities30/20/10.
Затем original454850 вызывает actual4248D0→4BBB60→COM SetTransform и original
Skin46A240/423FD0. Явный пользовательский pre callback возвращает0.

**6 exact captures /209 native assertions**,237 default source assertions:
three/failed-device/preserve-lights/append-during-pre/already-active/empty.
Whole read70998; flush26..487 instructions; peak65024-byte fixed arena;
38 engine owner generations freed. Original RenderNode destructors освобождают
оба owning refs Skin, затем Skin/material/mesh и helper owners. Queue refs
не увеличивают. COM HRESULT<0 по-прежнему не отменяет SetWorld успех.

454850 вызывает external qsort даже для count0 и делает это до установки44.
В этом fixture qsort — явно объявленный no-op только на уже упорядоченных
различных priorities; проверяются count/record24/original comparator454800.
CRT algorithm/tie ordering не заявлены. После sort ставится44=1; adjacent
support cache вызывает setup лишь для A и B, false Skin results игнорируются.
Callback в одном режиме копирует borrowed record B в четвёртый слот и меняет
count3→4: исходный loop читает count заново и посещает четвёртую запись без
новой сортировки. Final count/44=0 даже при изначальном44=1; record bytes
остаются stale. Второй empty flush снова вызывает qsort.

Actual4248D0 публикует C190 light-cache pointer до SetWorld, если C9C4=0.
Опция preserve сохраняет явный прежний pointer. После SetWorld копирует
world sphere из support24..30 в C9C8..D4; COM наблюдает прежнюю sphere и уже
новый light pointer. Dirty/current matrix также сверены побитно.

Source spRenderer::FlushAlphaForAnalysis переносит loop/count/flags/borrowed
storage и явную внешнюю сортировку. Callbacks dispatch здесь вызывают реальные
source RenderNode::PrepareForRenderForAnalysis и Skin::RenderUnlitForAnalysis.
RenderNode context хранит явные renderer matrix/light/sphere inputs. Host
защищает NULL/corrupted count/reentry, не приписывая такие гарантии original.
Подготовленные camera/cache inputs и external COM по-прежнему явны; geometry
draw останавливается по настоящему pre callback, не заменой engine тела.
Все прежние arena/request/instruction/time limits сохранены.

```powershell
python research/native_workbench.py run pc-skin-alpha-flush --deadline-utc 2026-09-07T16:00:00Z
```
