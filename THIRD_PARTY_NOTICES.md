# Licensing and attribution

Available without charge. Root [LICENSE](LICENSE) is GPL version2; combined unlock port and project tooling/docs are **GPL-2.0-only**, except separately licensed files below. Authors retain original copyrights/notices.

- `src/`: inherited GPL2 loader lineage and embedded notices. [References](docs/REFERENCES.md), [upstream notices](UPSTREAM-NOTICES.ko.md), [upstream changes](CHANGES.md), [PNY changes](docs/CHANGES-PNY.md).
- `diagnostics/v1`–`v4`: independent MIT diagnostics, each with LICENSE.txt; EDK2 headers retain separate notices.
- `deps/edk2` and diagnostic `vendor/edk2`: EDK2 BSD-2-Clause-Patent and per-file notices; [license](LICENSES/EDK2-BSD-2-Clause-Patent.txt).
- NVIDIA source contracts retain MIT notices where applicable; [notice](LICENSES/NVIDIA-MIT.txt). Open kernel source licensing does not relicense firmware.
- V67 constants derive from WildFlash1st GPL2 patch. Previous local MIT attribution corrected.
- [CMP reference MIT notice](LICENSES/CMP40HX-MIT.txt) retained; CMP behavior does not establish 3080 Ti support.
- NVIDIA firmware is **not shipped** in public source/releases. Local extraction retains exact package license as NVIDIA-LICENSE; [reference agreement](LICENSES/NVIDIA-Driver.txt). Local bundles contain separately licensed assets. No public redistribution permission for the generated bundle asserted.
- Measured benchmark/config/log facts included; excluded ROM/PROM/private captures are not licensed for redistribution.

EFI redistribution requires corresponding complete source and notices under GPL. Free access does not restrict commercial uses allowed by GPL and does not grant additional NVIDIA firmware rights.
