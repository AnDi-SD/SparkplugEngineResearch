# RTX Remix project moved

Our integration source, build scripts, launchers and tests now live in
[tools/WinxRemix](../../tools/WinxRemix/README.md).

This directory remains only as a relocation notice. Historical reports and
evidence manifests keep their original paths and hashes; use the corresponding
Git commit or preserved local source snapshot to reproduce an old experiment.
Current commands use `tools/WinxRemix/` instead of `research/rtx-remix/`.

Original Remix source is fetched from NVIDIA's repository into ignored
`local-data/rtx-remix/upstream/dxvk-remix`. Only our adapter, tests, preparation
scripts and patches are tracked in the tool project.
