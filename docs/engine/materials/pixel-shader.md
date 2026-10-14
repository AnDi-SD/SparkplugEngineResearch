# PC pixel shader classes

`spDXPixelShader` (`0x2B5D52DF`) is the abstract pixel shader interface above
`spDXShader`. `spPCPixelShader` (`0x77F8325C`) supplies PC device creation and
owns the returned device shader. The physical constructor chain and primary
tables agree with this inheritance; this conclusion uses more than the RTTI
registration bases. These are PC classes. No PS2 implementation is claimed.

The shared sources are
[spDXPixelShader.h](../../../Sparkplug/Code/SparkplugDX/spDXPixelShader.h) and
[spPCPixelShader.h](../../../Sparkplug/Code/SparkplugPC/spPCPixelShader.h), with
their adjacent `.cpp` files. Original source and header paths and original
method names remain unknown. The namespace, callback interface and
`ForAnalysis` names are portable reconstruction interfaces, not recovered
declarations or a reproduction of the native x86 ABI.

## Native storage and construction

PC factory `004C9D90` creates a `0x54` byte object. It runs the DX pixel
constructor `004D6120` and the shared shader constructor `004AF980`, leaving
the PC primary table at `006F2E98`. Construction does not call the device or
take a device reference.

| Native offset | Contents and factory behavior |
| --- | --- |
| `00` | PC primary table pointer |
| `04..0F` | Inherited base storage; types are defined by the base classes |
| `10` | Inherited name pointer, NULL |
| `14..34` | Nine shader words, zero; descriptor row-count sum is at `34` |
| `38` | Untouched by this constructor; field semantics remain unknown |
| `3C/40/44` | Owned descriptor vector begin/end/capacity pointers, zero |
| `48/4C` | Generated-code byte size and owned byte pointer, zero |
| `50` | Owned device pixel shader pointer, NULL |

A descriptor is `44` bytes: `32` bytes for its name, then type, start register
and register count. Descriptor resolution and constant generation are shared
with [spDXShader](shader-constants.md); the pixel subclasses do not introduce
another parameter table or another constant-building algorithm.

## Primary virtual tables

Each primary table contains eight 32-bit entries. The immediately following
bytes are a class-name string, not additional methods. Portable inherited
method aliases follow the repository's PS2 slot names; the offsets below are
the actual PC table offsets.

| PC offset | DX pixel table `006F3EC4` | PC pixel table `006F2E98` | Operation |
| --- | --- | --- | --- |
| `00` | `004D6160` | `004C9EE0` | Scalar deleting destructor |
| `04` | `005B7A00` | `005B7A00` | Inherited notification/context no-op, pops one word |
| `08` | `004A1BF0` | `004C9E00` | DX NULL clone stub; PC concrete clone |
| `0C` | `00413120` | `00413120` | Inherited named-object copy |
| `10` | `004D6140` | `004C9D80` | Class record getter |
| `14` | `00408350` | `00408350` | Exact identity query |
| `18` | `00408370` | `00408370` | Ancestry query |
| `1C` | `0060DB76` | `004C9EB0` | Purecall trampoline; PC device shader creation |

The DX record has no factory. Its device operation is abstract, and its clone
slot is the inherited NULL-return implementation. The PC record has a factory.

## Device creation and ownership

`004C9EB0` takes one caller-supplied bytecode pointer and pops four argument
bytes. It obtains the renderer from `0075DB68`, the device at renderer `C9E8`,
then calls device table `+1A8` with `(device, code, &this->field50)`.
Unlike PC vertex shader creation, it has no unused second argument.

This is the external `CreatePixelShader` boundary. The class does not assemble
or compile source. The existing [template compiler](shader-compile.md) accepts
the shared `spDXShader` output carrier for either shader stage; generated code
and descriptors belong to that shared carrier. The reconstructed pixel class
passes the exact supplied bytecode pointer to its caller-provided device
callback. It does not substitute a host graphics implementation.

The native operation always returns `AL=1`, ignoring HRESULT. The device writes
directly into the existing `50` field: failure may leave it NULL, supply a
handle, or leave a previously stored handle unchanged. The engine does not
release the previous handle before creation. On repeated successful writes,
the overwritten handle therefore receives no engine release.

Destructor `004C9E50` releases the final non-NULL `50` through its interface
table `+8`, clears the field, then calls `004D6150`. The shared shader
destructor `004AF8E0` frees code `4C`, frees the descriptor allocation `3C`,
clears the descriptor vector pointers and destroys the named base. Device
release occurs while code and descriptors are still owned. The scalar deleting
destructors additionally free the object when deletion flag bit zero is set.

The portable source uses C++ owners for descriptors, code and name. Its release
callback and callback context are our lifetime adapter; they are absent from
the native object. They must remain valid through destruction and describe the
same external provider across repeated creation. Missing callbacks cause a
source-side refusal before mutation, which is a host guard rather than an
original validation rule.

## Clone and integration boundaries

PC clone `004C9E00` creates a fresh PC pixel shader, records the clone mapping
and calls inherited named copy `00413120`. It copies the name only. Shader
words, descriptors, generated code and device handle remain at their new-object
defaults; no device handle is shared or duplicated. The portable source retains
this behavior through the existing common clone manager.

Pixel binding and constant uploads are renderer operations, not further pixel
shader virtuals. The renderer's source already has separate pixel stack,
identity cache and pixel constant submission. This class reconstruction
supplies the concrete object and device-handle boundary; it does not establish
which original callers enable a pixel shader or which effects use it in game.

The class-specific factory, getters, clone, device creation and destruction
have source implementations. Shared shader algorithms retain their documented
limits, including finite inputs and refusal of unwritten constant scratch
words. Original allocation failure, exception unwinding, every nonvirtual
caller, every template failure path, live device reset and rendered pixel
equivalence are not established by the class-operation qualification.
