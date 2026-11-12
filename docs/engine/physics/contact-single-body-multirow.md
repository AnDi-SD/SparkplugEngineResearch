# Contact: две и три строки для одного supplied body

`ComputeContactMultirowForAnalysis` — отдельная аналитическая value-операция
над 384 байтами: 228 raw-байт Contact, 96 байт буфера из поля `6C` и пять
12-байтовых буферов из `74/78/7C/80/84`. Это не native layout нового класса.
Числовые pointer-слова в raw Contact остаются инертными role tags: функция
их не разыменовывает и не создаёт Manager, body или runtime-граф.

Профиль требует полные DWORD `40=0`, `88=0`, `4C=0`, `70=0` и `58=2` либо
`58=3`. Поля `48/6C/74/78/7C/80/84` должны содержать ненулевые tags. Скаляр
положителен и лежит в `[0.25,4]`; point `B4..BC`, normal `C0..C8`, raw `CC`,
raw `8C` и явно supplied bodyPoint конечны и имеют модуль не более 16.
Выбранная tangent-пара ненулевая. Manager supplies exact raw `7C=3E4CCCCD`
и `80=3727C5AC`; singleton не читается. Default и отказ сохраняют output,
а staging допускает передачу одного value как input и output.

Warm base записывает reciprocal в `60`, Manager `7C` в `64`, константу 8
в `68`. Цикл по полному count инициализирует каждую выбранную строку:
parameter=Manager `80`, lower=`-FLT_MAX`, upper=`FLT_MAX`, index=`FFFFFFFF`.
Первая строка использует прежние [one-body операции](contact-single-body-row.md)
с теми же nearest64 kernels и явными F32 spills displacement/reciprocal.

Затем прежняя [tangent операция](contact-tangents.md) выбирает ветвь по
**signed** `normal.z > raw 3F3504F3`, без FABS. Сумма квадратов до FSQRT
остаётся extended. Оба tangent-вектора записываются в F32 и перечитываются
из этих stored words перед angular cross с F32 displacement. Эти векторы
образуют вторую и, при count=3, третью строки. Шаг raw6C равен 32 байтам:
linear занимает первые три DWORD, angular — DWORD 4..6; padding 3 и 7
сохраняется. При count=2 третья строка целиком сохраняется.

Для дополнительных строк lower получает FCHS от raw `8C`, upper сохраняет
raw `8C`, включая знаки нулей и отрицательные коэффициенты. При полном
`88=0` raw RHS второй и третьей строк остаются прежними, даже если там
нечисловые биты. Не выбранные поля и padding переносятся побайтно.

Имя API и структуры — наши аналитические обозначения. Native C++ return
type, физическая база, carrier owner, производство и lifetime графа,
полный solver, другие ветви и gameplay остаются открытыми. Этот prototype
опирается на static predictions; native pilot/matrix и сравнение текущего
C++ требуют отдельной квалификации. Он не добавляет зарегистрированный
класс и не закрывает целиком `spContactConstraint`.
