# Общие readers функций UV и цвета

Пятый блок цикла до19:00 МСК9 сентября. Удалены две самостоятельные C#
грамматики из SmoUvControllerDecoder и SmoMaterialColorControllerDecoder.
Инспекторы и corpus consumers используют общие сериализаторы.

## Реализация

UV field body передаётся настоящим spTransFunctionEvalSerializer и
spTransFunctionEval. Вложенные семь FunctionEval и два вектора читает исходник.
Для цвета существующий цикл четырёх ColorFunc и одного Function вынесен в
spMatColorControllerSerializer::ReadEvaluatorsForAnalysis; whole-controller
reader вызывает тот же метод. ABI использует настоящие leaf evaluators без
создания контроллера или регистрации в AnimationManager. Никаких ссылок на
материал, игровых заменителей и выполнения формул при чтении нет.

ABI2 возвращает192 байта UV или152 байта цвета, включая raw function type,
frequency/amplitude/offsets/pitch, packed color endpoints и UV pivot/axis.
C# только переносит значения в прежние DTO. Невосстановленный protected
конструктор MaterialColorController не используется и этим блоком не закрыт.

Reader сохраняет исходные повторы, skip неизвестных полей, значения NaN и
signed zero, а также допустимую ширину заголовка независимо от предпочтения
writer. Масштаб пустого TransFunctionEval получает исходный yOffset1;
старый C# helper назначал всем пропущенным yOffset0. Это удаление ошибочного
повторения defaults в приложении, а не изменение восстановленного класса.
Ограничение16 МиБ и проверка границ входа принадлежат host adapter.

## Доказательства и проверки

Семь свежих micro original-PC probes читают целые контроллеры и наблюдают
конечные скалярные поля памяти; полученные192/152 байта совпали с ABI точно.
Четыре UV случая используют настоящий controller factory и reader440BE0.
Три color случая используют reader4412E0 на явно объявленном backing с defaults
настоящих leaf factories. Ранее capped41A580/clone запрещены до входа;
это доказательство reader, не whole-factory/whole-loader. Fixtures:
default, authored values, repeat/unknown, UInt32 envelope, NaN/signed zero.
Четыре bounded/truncated refusals прошли.

C++ UVFunction386, ColorFunction118, MaterialColor225, FullLoader213 прошли.
Проверки C# дополнены широким UV заголовком, исходными defaults, повторами и
raw IEEE bits; на выбранных ресурсах проверяется успешный decode именно этих
двух полей. Итоговая выборка и хеши находятся в seal блока.

Первый uv-values fixture превысил предел255 байт вспомогательного генератора
коротких полей и завершился до original reader. Fixture получил допустимый
UInt32 envelope; ошибка сохранена. Лимиты эмулятора не повышались, capped
участки не повторялись. Первый C# compile выявил неверную сигнатуру BuildField
в новом тесте ширины; тест записывает явный wire fixture вместо этой перегрузки.

Остаются часы/вычисление контроллеров в составе настоящего material runtime,
разрешение material/texture references и старые preview эвристики. Чтение
параметров не объявляется готовым воспроизведением этих эффектов.

Evidence: `research/tools-core-material-functions-block-2026-09-09.json`.
