"""Compile only: default remains non-writing; PNY cannot use legacy/automatic modes."""
import pathlib
import subprocess
import sys
root = pathlib.Path(__file__).resolve().parents[1]
base = [sys.argv[1], "-std=c11", "-Werror", "-fsyntax-only", "-x", "c", "-",
        "-I../deps/edk2/MdePkg/Include", "-I../deps/edk2/MdePkg/Include/X64"]
pny = "-DGU_PROFILE_PNY_B550_3080TI"
exp = "-DGU_EXPERIMENTAL_3090"
keep = "-DGU_ENTRY_LATCHED_SS1_PRESERVE"
full = "-DGU_ENTRY_LATCHED_SS1_FULL"
original = "-DGU_STOCK_PROFILE_ORIGINAL_03100062"
observed = "-DGU_STOCK_PROFILE_OBSERVED_03000062"
dynamic = "-DGU_PNY_DYNAMIC_ENTRY_SS0"
cases = [
 ([], True), ([pny], True), ([pny, exp, keep], True),
 ([pny, exp], False), ([pny, keep], False), ([pny, exp, full], False),
 ([pny, exp, original], False), ([pny, exp, observed], False),
 ([pny, exp, keep, full], False),
 ([pny, "-DGU_PROFILE_B650M_3090FE"], False),
 ([pny, "-DGU_PROFILE_B660M_3070TI"], False),
 ([exp, original], True), ([exp, observed], True),
 ([exp, keep], True), ([exp, full], True),
 (["-DGU_PROFILE_B660M_3070TI", exp, full], True),
 (["-ffreestanding", "-DGU_HOST_WRITE_TEST"], False),
]
for mode in ("GU_AUTOMATIC_BOOT", "GU_INTERNAL_BOOT", "GU_HEADLESS_RESEARCH"):
    cases += [([pny, exp, keep, "-D" + mode], False), ([pny, "-D" + mode], False)]
cases += [
 ([dynamic], False), ([pny, dynamic], False), ([exp, keep, dynamic], False),
 ([pny, exp, dynamic], False), ([pny, exp, full, dynamic], False),
 ([pny, exp, keep, dynamic], True),
]
for mode in ("GU_AUTOMATIC_BOOT", "GU_INTERNAL_BOOT", "GU_HEADLESS_RESEARCH"):
    cases += [([pny, exp, keep, dynamic, "-D" + mode], False)]
for defs, expected in cases:
    source = '#include "port.h"\n'
    if not defs:
        source += '#if GU_PREFLIGHT_ONLY != 1\n#error Default can write\n#endif\n'
    result = subprocess.run(base + defs, cwd=root, input=source, text=True, capture_output=True)
    assert (result.returncode == 0) == expected, (defs, result.stderr)
print(f"PASS {len(cases)} production selection/default/legacy compile cases; no hardware access")
