# Общий PC producer света: 13 сентября 2026

Арифметика `spDXLight::RefreshDevicePayloadForAnalysis` вынесена в общий inline helper **`RefreshPCLightPayloadForAnalysis`** без смены подтверждённых правил. Класс делегирует ему; собственный native-light shim вызывает тот же helper, не содержит второй формулы PC payload. После extraction существующие проверки **pc-dx-light 8/8 и pc-light-world 6/6 PASS** сравнили результат восстановленного класса с original pristine instructions. Дополнительно выполнена read-only квалификация используемого debug EXE: **13/13 участков побайтно совпали**. Новые профили, игра и GPU автором этого checkpoint не запускались.

[Манифест](../../research/winx-remix-shared-light-producer-2026-09-13.json) содержит SHA исходников, EXE тестов до/после, записанных результатов, скриптов, двоичных участков и disassembly. Исторические CP44/CP90 документы сохранены: [spDXLight](native-class-sp-dx-light.md), [decoded light world](native-pc-light-world.md).

| Что сравнивалось | Pristine и debug |
| --- | --- |
| Pristine WinxClub.exe | `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` |
| Фактический WinxClubDebug.exe | `C27EA9DB4228781A12A90AE808807D4AF1397A7E40DD8F5FFF28F3C87CC62CDB` |
| Весь producer 4B53C0..4B58AF | 1264 байта, совпали; thiscall(this), без stack arguments |
| World callback 4B58D0..4B590A | 59 байт, совпали; thiscall(this, inheritedFlags), `ret 4` |
| Primary 6F0C88, secondary 6F0C84, thunk 4B5960 | 15 primary slots, secondary pointer и 11-байтовый thunk совпали; primary slot 12 → 4B58D0, secondary вычитает B4 из this |
| Base Light/Node world regions | 428C30, 384 байта; 421420, 544 байта — совпали |
| ARGB helper 424700 и пять float constants | 91 байт и значения 0, 1, 0.3F, 0.7F, float32(1/255) совпали |

Проверка использует уже имеющийся file-backed PE reader и Capstone из `local-data/research-cache/python`. Прочитаны только байты файлов; EXE не загружались для исполнения. Значения primary slots подтверждены, но тела всех 15 целей отдельно не квалифицировались. Проверенные base world regions не являются аудитом всей цепочки SceneManager/renderer. Все 14 записанных original-профилей исполняли **pristine**, а не debug instructions: перенос их значения на debug ограничен перечисленными совпавшими участками.

Producer читает kind `C0`, RGBA `C4..D0`, attenuation byte `D4`, intensity `D8`, range `E0`, hotspot/falloff `E4/E8`; world position `74..7C` и direction `A4..AC`. Записываемый payload занимает 26 words `F0..157`. Default vector `7600E0` и ambient ARGB `73FE98` передаются helper явно. Их текущие значения, начальное состояние и свежесть world cache не выводятся из совпадения кода.

World callback сначала сохраняет `stored B0 | inherited`; bit 1 добавляет refresh bit 8. Затем вызывает исходный 428C30 и только после него читает enabled byte `ED`: producer вызывается при enabled и ранее сохранённом bit 8. Поэтому disabled light всё ещё проходит base world/dirty propagation; raw intensity change без dirty marker может оставлять старый payload. Helper не заменяет callback, не выполняет update Node, не очищает flags и не регистрирует источник света.

Семантика extraction сохранена: range и falloff=1 записываются до type dispatch; ambient/unknown оставляют остальные words нетронутыми. Directional умножает RGBA на intensity, ограничивает RGB только сверху единицей, не ограничивает alpha; theta/phi остаются прежними. Point/spot копируют цвет без умножения на intensity. Point fallback по-прежнему использует 1/intensity даже при отключённой attenuation, включая +Inf при нуле; spot fallback — 1/0/0. Линейная attenuation сохраняет прежнее расположение double-промежуточных вычислений с исходными float32 0.7F/0.3F. Optional/unwritten words не превращены в нули. Finite-input guard — существующая граница analysis-кода, а не доказанное поведение native для произвольных NaN/Inf.

| Записанный профиль | Exact comparison groups |
| --- | --- |
| `20260913T042312296128Z-pc-dx-light.json` | factory, copy, clone, directional, point, spot, unknown, world — 8/8 |
| `20260913T042312296184Z-pc-light-world.json` | directional, point, spot, disabled, ambient, parent — 6/6 |

Скрипты сравнения остались прежними; их SHA совпали с reports. Они проверяют равенство source capture и original capture, включая raw payload words. World-профиль проходит deserialization и последовательные world/raw-change/dirty/inherited/disabled-position операции; parent использует настоящий original owning Node в ограниченном guest. Reports хранят exit results и script hashes, а не самостоятельный полный дамп всех captures; из них не выводится class coverage или полнота бинарника.

Общие test EXE собраны для x64 host с PC 2100 вариантом исходников в 04:22:40 UTC; профили начали работу в 04:23:12 UTC, все 14 children завершились успешно под прежним 30-секундным пределом. Первый `build.log` сохранил ошибку launcher: cmd неверно разобрал путь с прямыми слешами; `build-v2.log` содержит успешную сборку обоих targets. Старые EXE и исходный метод сохранены для provenance; отдельный повторный запуск before-EXE здесь не выполнялся.

`spPCLightPayload.h` SHA: `CBEBA279A3126A6C943A3C059F89FC9EB21DE1C3CBDD73D7E920CBBD4B01B0E1`. Новый helper — общий CPU расчёт подтверждённого PC payload, а не добавление DirectX 9 backend в приложения tools. **PS2 исходники этим extraction не изменены, PS2 проверки и дополнительное покрытие не заявляются.** Готовность native-light consumer, фактическая свежесть полей/ownership, scene selection, физическая модель света и GPU-результат относятся к отдельному игровому этапу.