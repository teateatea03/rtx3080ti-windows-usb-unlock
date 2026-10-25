"""Check public text inventory, provenance hashes, evidence math and local doc links.
This is a bounded publication check, not a replacement for Gitleaks or review.
"""
from pathlib import Path
import ast
import hashlib
import json
import re
import statistics
import sys

ROOT = Path(__file__).resolve().parents[1]
SKIP = {".git", ".local", "build", "bundle", "__pycache__"}
BINARY = {".bin", ".rom", ".efi", ".exe", ".dll", ".obj", ".zip", ".xz", ".zst"}
PRIVATE = re.compile(r"GPU-[0-9a-f]{8}-[0-9a-f-]{27,}|[A-Za-z]:[/\\]Users[/\\][A-Za-z0-9_.-]+|[A-Za-z]:[/\\]CODEX[/\\]|USBSTOR[/\\]DISK&", re.I)

def public_files():
    return sorted(p for p in ROOT.rglob("*") if p.is_file()
                  and not SKIP.intersection(p.relative_to(ROOT).parts))

def text_hash(path):
    return hashlib.sha256(path.read_bytes().replace(b"\r\n", b"\n")).hexdigest()

def main():
    files = public_files()
    for path in files:
        relative = path.relative_to(ROOT)
        assert path.suffix.lower() not in BINARY, f"Binary/archive excluded: {relative}"
        data = path.read_bytes()
        assert len(data) <= 2 * 1024 * 1024 and b"\0" not in data, f"Not public text: {relative}"
        content = data.decode("utf-8-sig")
        assert not PRIVATE.search(content), f"Possible private identifier/path: {relative}"
        if path.suffix == ".json":
            json.loads(content)
        if path.suffix == ".py":
            ast.parse(content, filename=str(relative))
    manifest = json.loads((ROOT / "IMPORT-MANIFEST.json").read_text())
    for entry in manifest["files"]:
        assert text_hash(ROOT / entry["path"]) == entry["public_sha256_lf"], entry["path"]
    for path in [*ROOT.glob("README*.md"), *ROOT.glob("THIRD_PARTY_NOTICES.md"), *ROOT.glob("CONTRIBUTING.md"), *(ROOT / "docs").glob("*.md")]:
        for target in re.findall(r"\[[^\]]*\]\(([^)]+)\)", path.read_text(encoding="utf-8")):
            if target.startswith(("https://", "http://", "#")):
                continue
            assert (path.parent / target.split("#")[0]).exists(), f"Broken link: {path.name} -> {target}"
    before, after = [json.loads((ROOT / f"evidence/benchmark-{label}.json").read_text()) for label in ("before", "after")]
    for key in ("Gpu", "Bus", "CuBlasVersion", "CuBlasSha256", "BenchmarkScriptSha256"):
        assert before[key] == after[key], key
    for b, a in zip(before["Results"], after["Results"]):
        for result in (b, a):
            assert result["N"] == 4096 and result["Output"] == result["Accumulation"] == "FP32"
            assert (result["ComputeType"], result["MathMode"], result["Algorithm"]) == (68, 16, 99)
            assert result["CheckedOutputElements"] == 4096**2 and result["ExactExpectedOutput"] == 4096
            assert statistics.median(result["SamplesTflops"]) == result["MedianTflops"]
        print(f'{b["Input"]}: {a["MedianTflops"]/b["MedianTflops"]:.6f}x')
    dynamic = json.loads((ROOT / "evidence/benchmark-run04.json").read_text())
    for key in ("Gpu", "Bus", "CuBlasVersion", "CuBlasSha256", "BenchmarkScriptSha256"):
        assert dynamic[key] == before[key], key
    for b, d in zip(before["Results"], dynamic["Results"]):
        for key in ("Input", "N", "Output", "Accumulation", "ComputeType", "MathMode",
                    "Algorithm", "IterationsPerSample", "CheckedOutputElements", "ExactExpectedOutput"):
            assert d[key] == b[key], key
        assert statistics.median(d["SamplesTflops"]) == d["MedianTflops"]
        print(f'{d["Input"]} RUN04: {d["MedianTflops"]/b["MedianTflops"]:.6f}x')
    report = (ROOT / "evidence/unlock-run04.txt").read_text()
    assert "TRIAL DYNAMIC SS0:" in report
    assert report.count("PNY dynamic preflight: SS0=0x24321117 SS1=0x7 FWSEC[8241e0]=0x3 strap[824148]=0x1") == 4
    assert "transaction result=0 containment=0 started=1 quiesced=1 fault=0 retained=0 maps=0 FLR=1" in report
    assert "SS0=88888888 SS1=00000007" in report
    assert report.rstrip().endswith("FINAL status=0 stage=transaction attempted=1 driver_release=0 clean=1")
    for run in (1, 2, 3):
        refused = (ROOT / f"evidence/rejected-run0{run}.txt").read_text()
        assert "stage=diagnostic-cold-state attempted=0 driver_release=0 clean=1" in refused
    sys.path.insert(0, str(ROOT / "tools/firmware"))
    import generate_v67
    assert hashlib.sha256(generate_v67.build_payload()).hexdigest() == generate_v67.PAYLOAD_SHA256
    print(f"PASS {len(files)} public text files, import hashes, Python/JSON, doc links, measured math and V67 hash. No hardware or firmware execution.")

if __name__ == "__main__":
    main()
