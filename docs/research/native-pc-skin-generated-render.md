# PC прочитанный Skin → generating shader miss → draw (CP69)

2026-09-07, pristine WinxClub.exe SHA-256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Продолжение [CP65 read/render](native-pc-skin-loaded-render.md) и
[CP58 generating draw](native-pc-renderer-generated-draw.md).

Сохранённый Skin/Node/bone arrays из полного491170 передаётся в46A240 при
пустом shader cache. Внутри одного завершённого original render:

```text
palette/world → DXMesh4BC670 → material/pass4BC4A0 → automatic4BC290
→ manager4C8980 miss → shader factory4C9F10 → template4CFFE0
→ external SDK request/result → parameter append4AF940
→ device creation4CA030 → cache insert4C87A0 → constants4AE930 → draw4BE210
```

Template и один pass подготовлены actual constructors4D0960/4D5120,
строки entry Main, target vs_2_0. RFX/XML acquisition здесь не исполняется.
Native request — HLSL source `#define USE_TEXCOORD0\n`, flags0, active1.
Код тела пуст: внешний SDK fixture возвращает **условные** пять bytes
1020304050 и reflection BlendMatrices0/3, MatDiffuse3/1. Это не компиляция
валидного GPU shader. Проверяется логика engine вокруг SDK: owned copy всех
bytes, четыре строки параметров, создание/освобождение device handle.

Shader handle обозначен7 при сравнении; guest и host адреса не приравниваются.
Normal и failed-device дают по одному create/cache entry и release.
Отрицательные device HRESULT, включая create с ненулевым output, не меняют
исходный успешный результат Skin. Все palette bytes/device events совпали.

## Перенос и проверки

`SubmitUnlitGeometryForAnalysis` получил optional generation context и
использует существующий `DrawAutomaticForAnalysis`. Его передаёт
`SkinRenderContextForAnalysis::shaderGeneration`. Без контекста сохраняется
прежний cache-only контракт. Соединены прежние template/compiler/cache/
material/palette/draw части, второго алгоритма не добавлено.

Source harness читает те же FAT/Skin bytes, создаёт такой же prepared
template, вызывает весь Skin entry и проверяет SDK request, palette/device
trace, handle lifetime. **2 exact captures,56 native assertions** (по19
linked/generation и9 render). Read50702, whole render42407 instructions,
52 owner generations полностью освобождены. Source build и comparison успешны.

## Память и уточнение атрибуции

Ни один cap не увеличен:100000 instructions/2s/call,30s/child,64KiB arena,
32KiB engine allocation request. Пик65488 bytes. Внешний allocator выбирает
наименьший подходящий свободный диапазон; большое caller-owned renderer
backing storage располагается у верхней границы. Живые engine объекты не
двигаются. COM device/vtable и SDK data используют уже объявленные4KiB
interface pages; engine fields/allocations остаются в arena. Это политика
стенда, не доказанная политика оригинального malloc.

После чтения466760/4228A0 очищают FAT/serializer entries. Повторный статический
просмотр уточнил: это **clear methods**, не полные destructors FAT/manager.
Раннее описание CP65 исправлено с явной оговоркой. Skin serializer и
PCErrorManager действительно уничтожаются исходными deleting destructors.
В CP69 после clear стенд снимает свой prepared serializer-manager global и
освобождает собственные raw backing inputs. Native heap frees не заменены.

Открыты whole SMO mesh/material acquisition, simultaneous SAN/frame,
lit/custom/fog/texture branches, библиотека SDK и живой GPU. Geometry buffers,
fallback material и renderer backing по-прежнему явные inputs.

```powershell
python research/native_workbench.py run pc-skin-generated-render --deadline-utc 2026-09-07T16:00:00Z
```
