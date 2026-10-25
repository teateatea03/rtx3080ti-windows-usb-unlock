"""Windows host extraction of pinned assets; no installer or firmware execution."""
from pathlib import Path
import argparse, hashlib, json, tarfile
import zstandard
import extract_boot_assets
import generate_v67

root = Path(__file__).parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--inputs", type=Path, required=True)
parser.add_argument("--work", type=Path, required=True, help="New local output directory")
args = parser.parse_args()
if args.work.exists():
    parser.error("Preserve existing work directory; choose a new --work")
args.work.mkdir(parents=True)
manifest = json.loads((root / "artifacts-610.43.03.json").read_text())
source, _, package = manifest["source_artifacts"]
def digest(p):
    with p.open("rb") as f:
        return hashlib.file_digest(f, "sha256").hexdigest()
for entry in (source, package):
    if digest(args.inputs / entry["filename"]) != entry["sha256"]:
        raise ValueError("input digest mismatch")
names = set(extract_boot_assets.PINNED)
source_root = args.work / "selected-source"
target = source_root / "src/nvidia/generated"
target.mkdir(parents=True, exist_ok=True)
found = set()
with tarfile.open(args.inputs / source["filename"], "r:xz") as archive:
    for member in archive:
        name = Path(member.name).name
        if name not in names:
            continue
        if name in found or not member.isfile() or not 0 < member.size <= 16*1024*1024:
            raise ValueError("invalid/duplicate generated source")
        data = archive.extractfile(member).read()
        if hashlib.sha256(data).hexdigest() != extract_boot_assets.PINNED[name]:
            raise ValueError("generated source digest mismatch")
        (target / name).write_bytes(data)
        found.add(name)
if found != names:
    raise ValueError("missing generated source")
out = args.work / "assets"
extract_boot_assets.extract_assets(source_root, out)
wanted = {
    ".PKGINFO": None,
    generate_v67.FIRMWARE_PREFIX + "gsp_ga10x.bin": generate_v67.GSP_SHA256,
    package["notice"]["path"]: package["notice"]["sha256"],
}
files = {}
with (args.inputs / package["filename"]).open("rb") as raw:
    with zstandard.ZstdDecompressor().stream_reader(raw) as decoded:
        with tarfile.open(fileobj=decoded, mode="r|") as archive:
            for member in archive:
                if member.name not in wanted:
                    continue
                if member.name in files or not member.isfile() or not 0 < member.size <= 128*1024*1024:
                    raise ValueError("invalid/duplicate package member")
                data = archive.extractfile(member).read()
                expected = wanted[member.name]
                if expected and hashlib.sha256(data).hexdigest() != expected:
                    raise ValueError("package member digest mismatch")
                files[member.name] = data
if set(files) != set(wanted):
    raise ValueError("missing package member")
if "pkgver = 610.43.03-1" not in files[".PKGINFO"].decode().splitlines():
    raise ValueError("wrong package version")
(out / "gsp_ga10x.bin").write_bytes(files[generate_v67.FIRMWARE_PREFIX+"gsp_ga10x.bin"])
payload = generate_v67.build_payload()
if hashlib.sha256(payload).hexdigest() != generate_v67.PAYLOAD_SHA256:
    raise ValueError("V67 output digest mismatch")
(out / "v67-ga102-610.43.03.bin").write_bytes(payload)
(out / "NVIDIA-LICENSE").write_bytes(files[package["notice"]["path"]])
result = {
    "upstream_revision": "0ebdb1c6308b6a2072be5bf04513472942541956",
    "mode": "Windows Python 3.12 + zstandard 0.25.0; archive member reads only",
    "downloaded_code_executed": False,
    "firmware_executed": False,
    "inputs": [{"name": e["filename"], "sha256": e["sha256"]} for e in (source,package)],
    "assets": [{"name": p.name, "bytes": p.stat().st_size, "sha256": digest(p)} for p in sorted(out.iterdir()) if p.is_file()],
}
(args.work / "preparation-manifest.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
print(json.dumps(result, indent=2))
