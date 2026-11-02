# PCI hardening update, 2026-11-02

This backport takes two reviewed changes from the Windows upstream, now hosted at
[windows-GPU-UNLOCK/Windows-GPU-UNLOCK](https://github.com/windows-GPU-UNLOCK/Windows-GPU-UNLOCK).
It retains the exact PNY/B550/5080 profile and manual F11 USB → U once → firmware
menu → Windows Boot Manager workflow.

## Changes and source

- [d3bb696f1f59](https://github.com/windows-GPU-UNLOCK/Windows-GPU-UNLOCK/commit/d3bb696f1f592760416f1a83fa7124eccb495083): require complete successful PCI identity reads, pin the boot-services resolver/handle/methods across callbacks, recheck BDF and reject reentry. A failed refresh leaves the target unavailable. This shared refresh path is used after firmware driver release and during its containment; the PNY profile continues to forbid that release path.
- [96924633c201](https://github.com/windows-GPU-UNLOCK/Windows-GPU-UNLOCK/commit/96924633c2017c8becda9701fd3267121d6330f7): accept aligned PCIe v1 starts through `0xec` and v2 through `0xc4`, reject unknown versions before device access. The measured card's v2/`0x78` remains accepted.

The upstream identity and layout tests are retained under GPL-2.0-only. Identity
tests use literal segment zero because this port has no variable-segment profile.
The layout mock now follows the parsed capability offsets instead of assuming
one fixed layout. All four suites run in `Test.ps1`, using generic and PNY identities.

## Candidate builds

| Mode | Bytes | SHA-256 | Hardware execution |
| --- | ---: | --- | --- |
| Fixed SS0 | 67,584 | `b48e0ba917f3004895bcff540bfd9859d8d1ab713d94473fc05a360a52e4586d` | Untested |
| Dynamic SS0 | 68,096 | `1e220ca0ad40fdada9ee3aca9c229a137208ada2328f0d9e2e38ad4463db5c83` | Untested |
| Diagnostic-only | 56,832 | `6b239f42b7ec665380f5d6bbb557c114c56eb7b0cca35e953bbc3f841249fb70` | Untested |

Clang/LLD 20.1.8, freestanding x64 UEFI, zero PE timestamp, no imports or
writable/executable sections. Build manifests continue to record
`HardwareUnlockTested=false`. Packaging/copy tools and CI pin these new candidates.

Local validation passes 13 fixture-independent groups, 32 compile-policy cases,
126 fixed and 225 dynamic full app/core scenarios plus default-mode refusal.
Each of generic/PNY has 557 identity assertions and 71 layout/FLR/restore cases.
See [validation receipt](../evidence/publication-pci-hardening.json) and
[selected test output](../evidence/pci-hardening-tests.txt). Every device and
firmware operation in these tests is simulated.

## Deployment and evidence boundary

The physical USB remains on the earlier RUN05 dynamic EFI
`b04ce9ff5b484c4dd069778ed8102199fbced2db977bef745041793632990844`.
The previous local build directory was backed up before rebuilding. Existing
RUN00–RUN05 reports remain preserved. This update does not install a boot entry,
create a Release or perform a hardware attempt. A later physical test must record
the new hash and a new report before claiming successful unlock or Windows health.

Windows upstream main was reviewed at
[`fd8bbea5b1db`](https://github.com/windows-GPU-UNLOCK/Windows-GPU-UNLOCK/commit/fd8bbea5b1db991805ffcd7ba84e73fa7867bd8a).
Its [3080 Ti research PR #139](https://github.com/windows-GPU-UNLOCK/Windows-GPU-UNLOCK/pull/139)
binds another exact GPU/parent identity and is not a deployable PNY profile.
Open [DMA test PR #193](https://github.com/windows-GPU-UNLOCK/Windows-GPU-UNLOCK/pull/193)
and [experimental universal-engine PR #153](https://github.com/windows-GPU-UNLOCK/Windows-GPU-UNLOCK/pull/153)
are not included. Zagitta's Linux 2.0 recipe-driven refactor at
[`7eb51bccae0a`](https://github.com/Zagitta/gpu-unlock/commit/7eb51bccae0a0b7e599e084d971170e9a1e82d0c)
is not imported into this UEFI port.

## 繁體中文

此次只移植 PCI 身分重新解析與 PCIe 設定保存邊界兩項防護。新增測試已納入
原有入口，固定／動態版完整模擬皆通過。新版三種 EFI 尚未實機執行；RUN05
的成功與效能紀錄僅屬於舊雜湊。USB 保留舊版，待另次實機測試才切換。

踩坑注意：重新解析身分時，裝置可能正處於 MEM 關閉或 BME 尚待清除狀態；
這時若套用「正常執行前」的 Command 條件，反而會擋住必要的清理存取。
因此仍檢查精確身分與完整回傳，允許清理所需的 Command 狀態。PCIe 範圍
則必須隨版本分開檢查，不能以單一偏移上限代替。
