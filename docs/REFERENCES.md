# Credits and pinned references

Integration and machine-specific port; original research/loader lineage belong to upstream authors.

| Source | Pin | Use |
| --- | --- | --- |
| [impacifist/Windows-GPU-UNLOCK](https://github.com/impacifist/Windows-GPU-UNLOCK/tree/e3f03cd9389195adcabda4743d8fc2922b32498c/internal-boot/unlock) | `e3f03cd9389195adcabda4743d8fc2922b32498c` | Direct Windows UEFI base |
| [rough2515-hub/rtx3080-unlock-public](https://github.com/rough2515-hub/rtx3080-unlock-public/tree/a08254d269af30ace9e57237abd3d8e6c261e8f5) | `a08254d269af30ace9e57237abd3d8e6c261e8f5` | Earlier Windows lineage |
| [Zagitta/gpu-unlock](https://github.com/Zagitta/gpu-unlock/tree/0ebdb1c6308b6a2072be5bf04513472942541956) | `0ebdb1c6308b6a2072be5bf04513472942541956` | Core unlock/firmware contracts |
| [WildFlash1st/cmp90hx-unlock](https://github.com/WildFlash1st/cmp90hx-unlock/blob/76940ae583281daa2b883518a2e89b56c11d3a46/driver/patches/cmp90-rejoin15/0014-6104303-cmp90hx-stockflow-rejoin14-multigpu-state.patch) | `76940ae583281daa2b883518a2e89b56c11d3a46` | _kgspCmp90FillV67Signature; GPL2, not MIT |
| [EDK2](https://github.com/tianocore/edk2/tree/c1415771ac592ad8509351c46cd3cda4f38d7d52) | `c1415771ac592ad8509351c46cd3cda4f38d7d52` | UEFI headers, BSD-2-Clause-Patent |
| [Older Windows pin](https://github.com/impacifist/Windows-GPU-UNLOCK/tree/9352abd27480203ea146f766e7c96957912d148a) | `9352abd27480203ea146f766e7c96957912d148a` | Diagnostic minimal-header provenance |

NVIDIA 610.43.03 URLs/hashes: [input manifest](../tools/firmware/artifacts-610.43.03.json), [assets](../src/assets.json), [notice](../tools/firmware/NOTICE.md).

- Source SHA-256: `7e118923c7a23edc36114d63273a46e3e04e9af98695a42203e7ac2dfe9fc1dc`.
- Package SHA-256: `e17e7c92eeffa178b0fe1f40eb17a47095f8fc3ce4a77bf115cf5bd5a4df86e9`.

[LLVM official 20.1.8](https://github.com/llvm/llvm-project/releases/tag/llvmorg-20.1.8): LLVM-20.1.8-win64.exe SHA-256 `3197846a2b19063687dd56e93e34cd941e3548d907f23a6131571321bdf9fe7b`. Local toolchain: Clang/LLD20.1.8, Python3.12.10, MSVC14.44.35207, SDK10.0.26100.0, zstandard0.25.0.

Discovery context: [LCZ topic 2488/2](https://lcz.me/topic/2488/2), subsequent RTX40/Windows UEFI news. These are context, not evidence this port supports AD10x, single-GPU operation or the news performance figures. No AD103 payload bundled.

[Original Korean notices](../UPSTREAM-NOTICES.ko.md) · [Licensing](../THIRD_PARTY_NOTICES.md)
