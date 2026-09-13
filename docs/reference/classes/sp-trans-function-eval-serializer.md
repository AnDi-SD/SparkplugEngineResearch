# spTransFunctionEvalSerializer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spTransFunctionEvalSerializer](../../../Sparkplug/Code/Sparkplug/spTransFunctionEvalSerializer.h), [spUVControllerSerializer](../../../Sparkplug/Code/Sparkplug/spUVControllerSerializer.h).

Статус: самостоятельная RTTI-запись, lifecycle, раздельный PC/PS2 ABI и
структурная grammar подтверждены. Класс отделён от `spUVControllerSerializer`:
UV-wrapper создаёт обычный временный экземпляр этого serializer-а и передаёт ему
embedded target, а не вызывает безымянный локальный helper.

## Field grammar и target offsets

Writer создаёт единственный field 0 wire-типа 5. Внутри него строго последовательно
обрабатываются семь functional evaluator-ов:

| Роль | Offset |
| --- | ---: |
| translation X/Y/Z | `+0x10/+0x48/+0x80` |
| scale X/Y/Z | `+0xB8/+0xF0/+0x128` |
| rotation | `+0x178` |

## Неизвестное

- оригинальные header/source paths и исходные имена методов;
- полный `spTransFunctionEval` layout за пределами подтверждённых offsets;
- field names из original enum и публичный interface getter-ов;
- `FunctionType` enum и точные runtime-формулы;
- default/NaN/time/wrap semantics и обработка ошибок;
- контролируемый in-game mutation test.

Следующий маленький кандидат — serializer отдельного functional evaluator-а,
который вызывается семь раз из этого класса; его identity и границы нужно сначала
найти независимо, не предполагая имя по роли call target-а.
