# Binary template header

The fixed header consumed by `spTemplateSerializer` is `0x4C` bytes on PC
and PS2, with little-endian integer words. It precedes the descriptor records;
the following record grammar remains separate from this header.

| Offset | Size | Field |
| ---: | ---: | --- |
| `0x00` | `4` | magic `0xDAB33F00` |
| `0x04` | `0x40` | null-terminated template name |
| `0x44` | `4` | unsigned descriptor count |
| `0x48` | `4` | word copied to `spTemplate +0x20`; meaning unknown |

The reader zeroes all `0x4C` bytes before calling the stream once with that
length. When the stream returns false, it returns count zero without changing
the template. If the magic is wrong, it emits a diagnostic and returns zero.
For valid magic it first changes the template's word, then its inherited name,
then returns the complete descriptor count. A count of zero therefore does
not imply that the template was left untouched.

The outer binary reader treats a zero count as failure and reports it after
those header writes. For a nonzero count it decodes and appends that many
descriptors in sequence; the available header proof does not establish record
rollback, limits, types or resource-loading semantics.

The original trusts the stream's return value and has no actual-length check.
A stream that returns true after copying fewer than `0x4C` bytes exposes the
unwritten fields as their initial zeros. This behavior differs from a stream
that reports the same short read as false and is preserved by the portable
header operation. The supported host domain requires a name terminator inside
the 64-byte name field.

Implementation and platform entry points are described in
[`spTemplateSerializer`](../reference/classes/sp-template-serializer.md).
