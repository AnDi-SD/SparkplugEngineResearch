# PC: общие raw pixels → DXTexture

## Resize и округление

Восстановлен `619219` для wrapped triangle coefficients. Для каждого source
coordinate проходят две половины линейной функции, интегрируют её на отрезках
destination pixels, объединяют соседние одинаковые wrapped indices и отбрасывают
веса≤float32 `1e-5`. Ratio, границы и накопления сохраняют float32 stores.
Consumer `61C44F` суммирует в порядке source y, source x, destination y
contribution, destination x contribution.

`60FDB4` заменяет DEFAULT `FFFFFFFF` на `00080004`: triangle плюс ordered
dither. Missing-mip path передаёт4, без dither. Это объяснило следующий
результат16/29 при совпадающих коэффициентах. Readonly traces на входе actual
`61F153` подтвердили отсутствие gamma, premultiply и error diffusion в этих
случаях и таблицу bias/32:

```text
31 15 27 11
 7 23  3 19
25  9 29 13
 1 17  5 21
```

Нечётные строки проходят справа налево; индекс bias следует порядку прохода.
Channel умножается на255 с float32 store toward zero, затем прибавляется bias
с таким же store, затем integer truncation. Dither одинаков для четырёх
каналов. Последующие mips используют прежнее округление+.5. C++ regression
сохраняет найденный1×8 random case целиком; guards проверяют отсутствие output
mutation при неправильном extent/размере.
