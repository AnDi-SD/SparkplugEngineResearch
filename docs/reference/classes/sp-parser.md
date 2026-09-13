# spParser

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spBaseObject](../../../Sparkplug/Code/SparkBase/spBaseObject.h), [spParser](../../../Sparkplug/Code/Sparkplug/spParser.h).

## Verified behavior

- Factory `4D0DA0` resolves to `13DAAB0` and requests `0x538` bytes.
  Constructor `4D0CA0` resolves to `13C8070`; text ownership byte `10` is false,
  fields `14/18/1C/20`, vector `128/12C/130`, word `134` and `400` bytes from
  `138` are zero. Bytes `123..127` remain allocator data.
- The `255`-byte lookup table at `24` marks decimal codes
  `9,10,13,32,33,40,41,42,43,44,45,47,58,59,60,61,62,63,91,93,94,123,125,126`.
  This delimiter table is not identical to the normalization dispatch table.
- Virtual text method `4D0BF0` sets input begin/end/length, sets word `20` to
  one and calls `4D0AA0`. The latter allocates exactly the supplied length,
  scans through the inclusive end, appends NUL, owns the new text and stores
  its size including NUL. **It leaves field `18` pointing to the old input end.**
- Tab and space collapse to a single space unless previous or next character
  is marked as a delimiter. Leading/trailing spaces can remain. CR and LF are
  discarded by the control-character branch, not converted to spaces.
- `//` skips to LF; `/*...*/` is deleted without an inserted separator.
  Therefore `a/**/b` yields `ab`. Quoted text preserves spaces, comment markers
  and line breaks. Backslash does not escape a quote in this method.
- Clone `4D0E00` makes an empty parser and uses inherited no-op copy `40ECE0`.
  Original destructors `4D0D80/4D0C30` release owned normalized storage and the
  vector at `128`. All tracked allocations are released in the bounded cases.

Factory `4D54B0` additionally confirms `spPCRFXFileLoader`: size `728`, vtable `6F3DA4`, same parser prefix, zero `538/53C/540/544/724`, embedded structure at `548` containing empty strings/vectors. Construction reaches `13C8FB0`, `4D5120` and `4D3800`; destruction uses `4C9790`.
