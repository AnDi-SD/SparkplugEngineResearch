# PC real SAN → retained Skin → first shader generation (CP70)

2026-09-07. Pristine WinxClub.exe SHA-256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

Соединены [CP66 SAN/Skin](native-pc-skin-san-render.md) и
[CP69 generating Skin](native-pc-skin-generated-render.md). Одни и те же
Skin, Node и bone arrays проходят завершённые фазы:

```text
FAT/Skin Read491170 → real bbush SAN Read43ECC0
→ Actor discovery5A33F0/Start5A1E30 → Manager4535A0/Actor5A2380
→ track479290 → explicit world421420 → Skin46A240
→ mesh/material → shader cache miss → compiler/reflection/device/cache
→ bone constants4AE930 → indexed draw4BE210
```

SAN asset SHA-256
`706BD0E5C70111BBD7C9524B3A37B1B2D867EC86FDC5A7A7908FAC8BBFCA428E`.
Native и source harness используют полный bbush.san reader, настоящее
manager→actor dispatch и один retained bone. Ни decoded keys, ни sampled
PRS не пересоздаются вручную между фазами. Сверяются raw sample, 30 local/
world PRS words, flags, frame/count, все matrix bytes, compiler request,
полный device trace и единственные create/release handle.

**4 exact captures, 173 native assertions**: quarter0.25, three-quarter0.75,
loop1.25 и failed-device0.5. Первые, второй и четвёртый — по34 linked +9
render checks, loop —35+9. Read Skin50702, SAN39170, tick максимум4857,
whole render42407 instructions. Пик65440 bytes в неизменных64KiB;92
owner generations освобождены. После refactor fixture прежние CP66 четыре
и CP69 два comparisons повторены успешно. Новый алгоритм в production
не добавлялся: source соединяет уже перенесённые readers/actor/Skin/generation.

Ограничения CP66/69 сохраняются: actor и SAN owners уничтожены до renderer
storage; это последовательные фазы, не simultaneous frame. World update
задан явно. Template, geometry, fallback material и external SDK/device
outputs — объявленные inputs. Пять SDK bytes условные, это не проверка
валидности HLSL или GPU. Prepared reader inputs освобождаются стендом после
actual entry clear; allocator placement не считается original malloc.
Классам не добавлена оценка лишь за композицию уже проверенных частей.

```powershell
python research/native_workbench.py run pc-skin-san-generated-render --deadline-utc 2026-09-07T16:00:00Z
```
