# spMatColorControllerSerializer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spMatColorControllerSerializer](../../../Sparkplug/Code/Sparkplug/spMatColorControllerSerializer.h).

Статус: восстановлены идентичность, RTTI/lifetime, раздельный PC/PS2 ABI и
подтверждённая структура пяти evaluator-секций. Portable-класс намеренно не
реализует формулы evaluator-ов и реальный stream codec до реконструкции самих
`spColorFuncEval`/`spFunctionEval`.

## Идентичность

Точный исходный `.cpp` path не найден. PS2 factory выделяет `0x14` байт, а PC
destructor/vtables подтверждают такой же observed extent без derived storage.

## Состав сериализации

Собственная секция имеет один внешний field ID 0, названный в диагностике
`esfMaterialColorController`. Внутри reader и writer вызывают специализированные
helper-сериализаторы в фиксированном порядке:

| Порядок | Роль | Helper | Target offset PC/PS2 |
| ---: | --- | --- | ---: |
| 0 | ambient | color-functional evaluator | `+0x68` |
| 1 | diffuse | color-functional evaluator | `+0xB8` |
| 2 | specular | color-functional evaluator | `+0x108` |
| 3 | emissive | color-functional evaluator | `+0x158` |
| 4 | alpha | functional evaluator | `+0x1A8` |

Эти пять смещений и разделение 4+1 независимо видны в x86 и MIPS-коде. Reader
обеих платформ требует ненулевой target, обрабатывает field 0 и передаёт
неизвестные ID общему skip-пути. Writer всегда открывает внешнюю секцию и пишет
все пять evaluator-ов; уже сами helper-ы подавляют значения по умолчанию.

Portable `EvaluatorPlan` фиксирует только доказанные роли, helper-kind и offsets.
Он не объявляет layout evaluator-ов частью layout serializer-а. Полная
read-only статистика полей helper-ов находится в
[`smo-class-sp-material-color-controller.md`](sp-material-color-controller.md).

## Неизвестное

Следующий класс по текущему serializer-ряду — `spLightControllerSerializer`.
