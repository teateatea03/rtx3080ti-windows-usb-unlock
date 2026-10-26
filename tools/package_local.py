"""Build a LOCAL experimental USB file tree; never uploads, formats or boots it."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil

ROOT = Path(__file__).resolve().parents[1]
FIXED_CANDIDATE_EFI = "4fd4143b57acbdbd68e57a3b792bcebcf73f5b48612ea788b47347338c9b1167"
DYNAMIC_CANDIDATE_EFI = "b04ce9ff5b484c4dd069778ed8102199fbced2db977bef745041793632990844"
PROM = "991cbe4878a4c6f0456b6412ffff3dab5e7f60ec317ba4539254eb52b692f140"

def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assets", type=Path, required=True)
    parser.add_argument("--diagnostic-summary", type=Path, required=True,
                        help="Local v4 Read-Run summary; not a substitute for physical cold boot")
    parser.add_argument("--output", type=Path, required=True, help="New directory under .local")
    parser.add_argument('--dynamic-entry-ss0', action='store_true', help='Explicit dynamic entry latch; all exact-card and service-boundary guards retained')
    a = parser.parse_args()
    out = a.output.resolve()
    if out.exists() or not out.is_relative_to(ROOT / ".local"):
        parser.error("Use a NEW output directory beneath this repository's .local")
    summary = json.loads(a.diagnostic_summary.read_text(encoding="utf-8-sig"))
    if not (summary["Version"] == 4 and summary["Problems"] == 0
            and summary["ProbeCaptureComplete"] and summary["RestoreVerified"]
            and summary["ConfigUnchanged"] and summary["PciAttributesValid"]
            and summary["PromSha256"].lower() == PROM):
        parser.error("Diagnostic evidence does not match the measured PNY profile")
    regs = {r["Offset"]: r["Value"] for r in summary["Registers"]}
    ss0 = regs.get(0x82381c)
    candidate = isinstance(ss0, int) and 0 <= ss0 <= 0xffffffff and ss0 not in (0xffffffff, 0x88888888) and (ss0 & 0xffff0000) != 0xbadf0000
    selector_ok = candidate if a.dynamic_entry_ss0 else ss0 in (0x24321116, 0x24321156)
    if not (regs.get(0x8241e0) == 3 and regs.get(0x824148) == 1 and regs.get(0x823820) == 7 and selector_ok):
        parser.error("Wrong FWSEC or stock selector evidence")
    mode = "experimental-dynamic-ss0-preserve-ss1" if a.dynamic_entry_ss0 else "experimental-preserve-ss1"
    candidate_efi = DYNAMIC_CANDIDATE_EFI if a.dynamic_entry_ss0 else FIXED_CANDIDATE_EFI
    build = ROOT / "src/build/pny3080ti" / mode
    efi = build / "PNY3080TI.EFI"
    manifest = json.loads((build / "build-manifest.json").read_text(encoding="utf-8-sig"))
    if (digest(efi) != candidate_efi or manifest["EfiSha256"].lower() != candidate_efi
            or bool(manifest.get("DynamicEntrySs0")) != a.dynamic_entry_ss0
            or manifest["Mode"] != mode or manifest.get("HardwareUnlockTested") is not False):
        parser.error("Only the reviewed, hardware-untested hardening candidate hash is packaged")
    for source in manifest["Sources"]:
        if digest(ROOT / "src" / source["Name"]) != source["Sha256"].lower():
            parser.error("Source changed since build")
    tests = ROOT / "src/build/tests" / ("pny3080ti-dynamic-ss0" if a.dynamic_entry_ss0 else "pny3080ti")
    result = json.loads((tests / "pny-host-results.json").read_text(encoding="utf-8-sig"))
    logs = [p for p in tests.glob("pny-host-*.log") if digest(p) == result["LogSha256"].lower()]
    expected_count = 225 if a.dynamic_entry_ss0 else 126
    if not result["AllPassed"] or bool(result.get("DynamicEntrySs0")) != a.dynamic_entry_ss0 or digest(tests / "pny-tests.exe") != result["TestExeSha256"].lower() or not any(f"{expected_count} PNY actual app/core scenarios passed" in p.read_text(encoding="utf-16" if p.read_bytes().startswith(b"\xff\xfe") else "utf-8-sig") for p in logs):
        parser.error("Run the full fixture tests first")
    assets = json.loads((ROOT / "src/assets.json").read_text())
    for asset in assets:
        path = a.assets / asset["Name"]
        if path.stat().st_size != asset["Bytes"] or digest(path) != asset["Sha256"].lower():
            parser.error("Wrong local asset: " + asset["Name"])
    license_path = a.assets / "NVIDIA-LICENSE"
    if digest(license_path) != "c692fb14ad4f499c6aee1b9e144e6905148c420e6225b17cb2937366c1e507ad":
        parser.error("Wrong NVIDIA license")
    parent = out / "PNY3080TI-UNLOCK"
    (out / "EFI/BOOT").mkdir(parents=True)
    (parent / "ASSETS").mkdir(parents=True)
    shutil.copy2(efi, out / "EFI/BOOT/BOOTX64.EFI")
    for asset in assets:
        shutil.copy2(a.assets / asset["Name"], parent / "ASSETS" / asset["Name"])
    for src, name in [(ROOT / "LICENSE", "GPL-LICENSE.txt"),
                      (ROOT / "THIRD_PARTY_NOTICES.md", "THIRD_PARTY_NOTICES.md"),
                      (ROOT / "docs/USB-GUIDE.zh-TW.md", "README.zh-TW.md"),
                      (ROOT / "docs/USB-GUIDE.md", "README.md"),
                      (license_path, "NVIDIA-LICENSE.txt")]:
        shutil.copy2(src, parent / name)
    files = sorted(p for p in out.rglob("*") if p.is_file())
    (out / "SHA256SUMS.txt").write_text("".join(
        f"{digest(p)}  {p.relative_to(out).as_posix()}\n" for p in files), encoding="ascii")
    print("Local hardening-candidate bundle verified; hardware execution remains untested. Contains separately licensed firmware; DO NOT upload this directory.")

if __name__ == "__main__":
    main()
