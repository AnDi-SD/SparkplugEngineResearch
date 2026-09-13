# PC cached automatic weighted draw

## Exact cache behavior

- 4BE2B0 uses4096 bytes of uninitialized stack, evaluates descriptors and
  copies shader+34 rows through renderer primary+58 /4BE210.
- 4BE210 copies rows toCBC4+16*start and setsE444=max(previous,start+count).
  Old high-water mark/tail persist when fewer rows are copied.
- Changed selected identity becomes boundE44C before COM+170; constants
  COM+178 use0,CBC4,E444. Device HRESULTs ignored.
- Epilogue4BC3EC..4BC403 clears the automatically selectedE454[current]
  after drawing, but retains boundE44C. Repeated auto draws rebuild constants.
- Explicit preselection bypasses key/constant rebuilding and survives the
  epilogue; changed input matrices/material do not update its old constants.

Source `DrawCachedAutomaticForAnalysis` composes the shared manager/key,
shader constant evaluator and resolved-draw core. The manager is injected,
not an invented process-global startup. Cache miss returns incomplete.
`BuildFullyWrittenConstantsForAnalysis` separately guards every submitted
word in the4096-byte scratch. Scalar-only writes, UV fourth lanes and empty
descriptors cannot manufacture zero values for untouched native stack bytes.
The guard is an explicit host restriction, not original validation.
