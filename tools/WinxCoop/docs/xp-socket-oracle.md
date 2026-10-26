# XP32 socket oracle

`tests/XpSocketPeer.c` is an independent **test-only** wire peer for a small
Windows XP x86 guest. It is not an XP port of the production WinxCoop library,
launcher or game adapter. It does not link the C++ runtime or the production
packet serializer; its fixed wire fixtures independently check interoperability.

It connects to one explicit IPv4 endpoint, checks Probe/Info/Ready/GameStart
with token `1234` hex, exchanges and echoes payloads of 0/1/256 bytes, checks
one Hello echo, performs `shutdown(SD_SEND)` and waits for EOF. Fixed buffers,
packet/attempt limits, a 5-second connect deadline, 5-second socket timeouts
and a 20-second overall deadline bound the run. A current blocking call can
take up to its socket timeout beyond the overall deadline. No heap is used.

Build from an **x86** Visual Studio developer command prompt. Choose a new
output directory first; commands below assume it already exists:

```bat
cl /nologo /TC /O1 /GS- /W4 /Zl /c tools\WinxCoop\tests\XpSocketPeer.c /FoX:\fresh-output\XpSocketPeer.obj
link /nologo /NODEFAULTLIB /ENTRY:ProbeEntry /SUBSYSTEM:CONSOLE,5.01 /MACHINE:X86 /INCREMENTAL:NO /OUT:X:\fresh-output\XpSocketPeer.exe X:\fresh-output\XpSocketPeer.obj kernel32.lib ws2_32.lib
dumpbin /headers /imports X:\fresh-output\XpSocketPeer.exe
```

For a guest that starts the peer through Explorer, use
`/SUBSYSTEM:WINDOWS,5.01` instead of `/SUBSYSTEM:CONSOLE,5.01` with the same
entry point. This version creates no console window and writes the same report.

Inspect the PE machine/subsystem version and imports. Only `KERNEL32.dll`
and `WS2_32.dll` are expected; a successful modern-host build does not establish
XP compatibility. Actual XP execution and the reported OS version are separate
acceptance requirements. Microsoft documents the x86 subsystem minimum in
[/SUBSYSTEM](https://learn.microsoft.com/en-us/cpp/build/reference/subsystem).

Inside the isolated guest:

```bat
XpSocketPeer.exe --server 192.168.56.1:24001 --report "C:\Coop Test\peer-result.json"
```

For double-click launch without arguments, place ASCII text without a BOM in
`XpSocketPeer.exe.args` beside the executable (the actual executable filename
plus `.args`). Its contents are the same options, without the executable name:

```text
--server 192.168.56.1:24001
--report "C:\Coop Test\peer-result.json"
```

Spaces, tabs and CR/LF separate tokens; quoted report paths may contain spaces.
The sidecar is bounded to 1024 bytes, opened for reading only and resolved from
the executable path, independently of Explorer's current directory. Explicit
command-line options take precedence; a partial command line is an error, not a
request to merge options with the sidecar. `sidecarUsed` records which input ran.

The server must enable the diagnostic 0/1/256-byte echo corpus and authorize
token `1234` hex. The report path must be ASCII, parent directory must exist,
and the report must not exist: `CREATE_NEW` prevents replacement. Reports
contain actual OS version, process/endpoints, exchange/Hello/EOF observations,
byte/call counts and Winsock/socket cleanup counts. Exit code 0 requires the
complete exchange and balanced cleanup; 3 means failure. `passed` alone does
not assert the process ran in XP: check `osVersion`/`isWindowsXP` and the VM's
independent identity as well.
