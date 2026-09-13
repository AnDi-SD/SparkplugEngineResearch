# External headers

No third-party implementation is vendored here. Run `../Prepare-Dependencies.ps1`
from PowerShell, or `python tools/WinxRemix/prepare_dependencies.py` from the
repository root. Downloads and generated headers stay in ignored `local-data/`.

- Remix: https://github.com/NVIDIAGameWorks/dxvk-remix.git, pinned in
  [dependencies.json](../dependencies.json). The light conversion generator
  extracts the original functions with their MIT notice, applies our explicit
  CPU adaptation, and verifies the result against the previously tested header.
- xxHash: https://github.com/Cyan4973/xxHash, v0.8.3. The unmodified header and
  its embedded BSD 2-Clause license are downloaded and SHA-256 checked.

Our patches and generator are project code. Generated upstream functions are
Remix policy, not reconstructed Winx/Sparkplug logic. Compilation preserves the
existing `third-party/...` include names via the external include directory;
CPU fixtures freeze those dependencies into their local source snapshots.
