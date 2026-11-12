# Contact: строки с заданным tangent-вектором

ComputeContactProvidedTangentForAnalysis представляет отдельную аналитическую
value-операцию над Contact228, raw6C96 и пятью массивами по12байт: всего384B.
Это проекция данных, а не native layout нового класса. Pointer-слова инертны,
не разыменовываются и не создают singleton, body или runtime-граф.

Профиль требует полный DWORD40=0 либо1, 4C=70=0, count58=2 либо3. Полный88
ненулевой и содержит только биты000007E5. Теги48/6C/74/78/7C/80/84 ненулевые.
Manager supplies raw7C=3E4CCCCD и80=3727C5AC. Скаляр положителен в[0.25,4];
point/normal/bodyPoint/CC/8C/90/providedD8..E0 конечны с модулем<=16.
Нулевые normal/tangent допустимы: здесь нет FSQRT и tangent helper.
Default и отказ сохраняют output; staging допускает before/output alias.

Warm base и первая строка используют прежние
[one-body операции](contact-single-body-row.md). При40=1 normal проходит
FCHS с F32 spill, включая signedzero. Первый tangent копируется изD8/DC/E0.
Второй равен effectiveNormal cross providedTangent: Z сохраняется какF32,
затем X/Y. Все storedF32 компоненты перечитываются перед angular cross
с F32 displacement. Используются прежние целочисленные nearest64 kernels.

Base инициализирует выбранные строки: parameter=Manager80, lower=-FLT_MAX,
upper=FLT_MAX,index=FFFFFFFF. Jacobian stride32B; linear DWORD0..2,
angular DWORD4..6; padding3/7 сохраняется. Count2 сохраняет третью строку
целиком, включая все пять дополнительных массивов.

| Бит88 | Операция |
| --- | --- |
| 004 | rawA0 копируется в parameter0 |
| 020/040 | rawA4/A8 копируется в RHS1/2 |
| 080/100 | rawAC/B0 копируется в parameter1/2 |
| 200/400 | index1/2 получает0 |
| 001 | Третья строка использует raw90 вместоraw8C |

Коэффициент второй строки raw8C; lower получает FCHS, upper исходное слово,
включая отрицательные значения и signedzero. Без соответствующего override
RHS дополнительной строки остаётся исходным. A0/A4/A8/AC/B0 — непрозрачные
DWORD: NaN,Inf и произвольные payload копируются как биты без float-проверки.

Имена API аналитические. Native return type, физическая база, carrier owner,
lifetime, bit8 velocity/restitution, body2, полный solver и gameplay открыты.
Этот профиль отделён от [генерируемых tangent](contact-tangents.md) и не
означает полноты spContactConstraint.
