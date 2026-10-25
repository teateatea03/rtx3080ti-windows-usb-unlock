"""Reconstruct the public, measured 256-byte conventional PCI fixture; no hardware."""
from pathlib import Path
import hashlib, json, struct
root=Path(__file__).resolve().parents[1]
data=struct.pack("<64I",*json.loads((root/"evidence/pny-pci-config.json").read_text())["dwords"])
assert hashlib.sha256(data).hexdigest()=="ec681e8f2b2f4cef6e6ec0e451b1d2814c31b6b2c32defd5bd65cc8747a5c1b2"
out=root/".local/config256.bin"
out.parent.mkdir(parents=True,exist_ok=True)
out.write_bytes(data)
print("Public PCI fixture verified")
