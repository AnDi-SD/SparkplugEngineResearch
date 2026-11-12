# Contact: две и три строки с двумя supplied body

`ComputeContactTwoBodyMultirowForAnalysis` — отдельная аналитическая
value-операция над 480 байтами: 228 captured-байт Contact, два raw-буфера
по 96 байт из `6C/70` и пять 12-байтовых массивов `74/78/7C/80/84`.
Это не native class layout: pointer-слова — инертные tags, а две позиции
body передаются явно. Manager/singleton/body/граф функция не создаёт.

Профиль требует полный `40=88=0`, count `58=2` либо `58=3`, ненулевые
различные пары tags `48/4C` и `6C/70`, прочие buffer-tags ненулевые.
Scalar положителен в `[0.25,4]`; point/normal/обе bodyPoint/rawCC/raw8C
конечны и имеют модуль не более 16. Выбранная tangent-пара ненулевая;
Manager supplies точные raw `7C=3E4CCCCD` и `80=3727C5AC`. Default/отказ
сохраняют output, staging допускает before/output alias.

Primary row использует прежнюю [two-body операцию](contact-two-body-row.md):
J2 displacement X проходит F32 spill, Y/Z остаются extended; angular X/Y
проходят F32 перед FCHS, Z — FCHS extended перед F32. Base loop задаёт
каждой выбранной строке parameter=Manager80, lower/upper=`-FLT_MAX/FLT_MAX`
и index=`FFFFFFFF`; reciprocal/Manager7C/8 пишутся в raw60/64/68.

Прежний [tangent helper](contact-tangents.md) вычисляет stored F32 T1/T2
по signed `normal.z > raw3F3504F3`, без FABS и без F32 spill перед FSQRT.
Для дополнительных J1 строк используются T1, затем T2 и F32 displacement1.
Обе дополнительные J2 linear-строки получают **positive T2**. Они не
заменяются conventional `-T1/-T2`. Angular J2 использует T2 и все три
перечитанных F32 displacement2; X остаётся extended через FCHS, Y/Z
сначала проходят F32 spill, затем FCHS. Это отличается от primary row.

Stride каждой Jacobian-строки 32 байта: linear DWORD0..2, angular4..6,
padding3/7 сохраняется. При count2 третьи строки обоих Jacobian и третьи
элементы всех дополнительных массивов сохраняются. RHS1/2 при полном88=0
не изменяются. Дополнительные lower/upper получают FCHS(raw8C)/raw8C,
включая отрицательные значения и signedzero. Все остальные raw-байты
переносятся без интерпретации. Используются прежние nearest64 kernels.

API задаёт только описанный finite-профиль. Другие ветви метода, включая
переворот normal и ненулевые flags88, не входят в этот контракт. Имена API
аналитические; физическая база, native return type, оба carrier owner, lifetime/production
reachability/fullsolver/gameplay остаются открытыми. Новый API не добавляет
зарегистрированный класс и не закрывает целиком `spContactConstraint`.
