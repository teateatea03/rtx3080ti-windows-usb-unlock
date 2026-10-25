"""Bounded CUDA/cuBLAS benchmark only; no firmware, MMIO, settings or driver changes."""
import argparse
import ctypes as C
import hashlib
import json
import os
from pathlib import Path
import statistics
import subprocess
import time
import uuid

p = argparse.ArgumentParser()
p.add_argument("--output", type=Path, required=True)
p.add_argument("--cublas", type=Path, required=True, help="Locally installed cublas64_12.dll")
p.add_argument("--bus", required=True, help="Explicit PCI BDF, for example 0000:04:00.0")
p.add_argument("--expect-uuid", required=True, help="Local guard; never included in the output")
a = p.parse_args()
if a.output.exists():
    raise SystemExit("Preserve existing benchmark result; choose a new output path")
dll_path = a.cublas.resolve()
dll_cookie = os.add_dll_directory(str(dll_path.parent))
cuda = C.WinDLL("nvcuda.dll")
blas = C.WinDLL(str(dll_path))
ptr, u64, i32 = C.c_void_p, C.c_uint64, C.c_int
def bind(lib, name, args):
    f = getattr(lib, name)
    f.argtypes, f.restype = args, i32
    return f
def ok(f, *args):
    status = f(*args)
    if status:
        raise RuntimeError(f"{f.__name__} returned {status}")
init = bind(cuda, "cuInit", [C.c_uint])
by_bus = bind(cuda, "cuDeviceGetByPCIBusId", [C.POINTER(i32), C.c_char_p])
get_name = bind(cuda, "cuDeviceGetName", [ptr, i32, i32])
get_uuid = bind(cuda, "cuDeviceGetUuid_v2", [ptr, i32])
create = bind(cuda, "cuCtxCreate_v2", [C.POINTER(ptr), C.c_uint, i32])
destroy = bind(cuda, "cuCtxDestroy_v2", [ptr])
alloc = bind(cuda, "cuMemAlloc_v2", [C.POINTER(u64), C.c_size_t])
free = bind(cuda, "cuMemFree_v2", [u64])
fill16 = bind(cuda, "cuMemsetD16_v2", [u64, C.c_ushort, C.c_size_t])
fill32 = bind(cuda, "cuMemsetD32_v2", [u64, C.c_uint, C.c_size_t])
copy = bind(cuda, "cuMemcpyDtoH_v2", [ptr, u64, C.c_size_t])
sync = bind(cuda, "cuCtxSynchronize", [])
ev_create = bind(cuda, "cuEventCreate", [C.POINTER(ptr), C.c_uint])
ev_destroy = bind(cuda, "cuEventDestroy_v2", [ptr])
ev_record = bind(cuda, "cuEventRecord", [ptr, ptr])
ev_sync = bind(cuda, "cuEventSynchronize", [ptr])
ev_time = bind(cuda, "cuEventElapsedTime", [C.POINTER(C.c_float), ptr, ptr])
b_create = bind(blas, "cublasCreate_v2", [C.POINTER(ptr)])
b_destroy = bind(blas, "cublasDestroy_v2", [ptr])
b_version = bind(blas, "cublasGetVersion_v2", [ptr, C.POINTER(i32)])
set_math = bind(blas, "cublasSetMathMode", [ptr, i32])
gemm = bind(blas, "cublasGemmEx",
            [ptr, i32, i32, i32, i32, i32, ptr, ptr, i32, i32,
             ptr, i32, i32, ptr, ptr, i32, i32, i32, i32])
ok(init, 0)
device = i32()
ok(by_bus, C.byref(device), a.bus.encode("ascii"))
name, raw_uuid = C.create_string_buffer(128), C.create_string_buffer(16)
ok(get_name, name, 128, device)
ok(get_uuid, raw_uuid, device)
gpu_uuid = "GPU-" + str(uuid.UUID(bytes=raw_uuid.raw))
if name.value != b"NVIDIA GeForce RTX 3080 Ti" or gpu_uuid.lower() != a.expect_uuid.lower():
    raise SystemExit("Refuse different GPU identity")
context, handle = ptr(), ptr()
memory, events = [], []
results = []
snapshot = lambda: subprocess.check_output(
    ["nvidia-smi", "--query-gpu=name,pci.bus_id,driver_version,power.draw,temperature.gpu,memory.used,utilization.gpu", "--format=csv,noheader"],
    text=True).strip()
before = snapshot()
try:
    ok(create, C.byref(context), 0, device)
    ok(b_create, C.byref(handle))
    ok(set_math, handle, 16)  # DISALLOW_REDUCED_PRECISION_REDUCTION
    version = i32()
    ok(b_version, handle, C.byref(version))
    n, iterations = 4096, 20
    elements = n * n
    for size in (elements * 2, elements * 2, elements * 4):
        address = u64()
        ok(alloc, C.byref(address), size)
        memory.append(address)
    for _ in range(2):
        event = ptr()
        ok(ev_create, C.byref(event), 0)
        events.append(event)
    alpha, beta = C.c_float(1), C.c_float(0)
    for label, dtype, one_bits in (("FP16", 2, 0x3C00), ("BF16", 14, 0x3F80)):
        ok(fill16, memory[0], one_bits, elements)
        ok(fill16, memory[1], one_bits, elements)
        ok(fill32, memory[2], 0, elements)
        # CUDA_R_32F output (0), CUBLAS_COMPUTE_32F (68), DEFAULT_TENSOR_OP (99).
        def multiply():
            ok(gemm, handle, 0, 0, n, n, n, C.byref(alpha),
               ptr(memory[0].value), dtype, n, ptr(memory[1].value), dtype, n,
               C.byref(beta), ptr(memory[2].value), 0, n, 68, 99)
        for _ in range(3):
            multiply()
        ok(sync)
        samples = []
        for _ in range(3):
            ok(ev_record, events[0], None)
            for _ in range(iterations):
                multiply()
            ok(ev_record, events[1], None)
            ok(ev_sync, events[1])
            milliseconds = C.c_float()
            ok(ev_time, C.byref(milliseconds), events[0], events[1])
            samples.append(2 * n**3 * iterations / (milliseconds.value / 1000) / 1e12)
        # Check every output. all-ones input gives exactly n in float32.
        chunk_elements = 1 << 18
        expected = C.c_float(float(n))
        expected_word = C.cast(C.byref(expected), C.POINTER(C.c_uint32))[0]
        buffer = (C.c_uint32 * chunk_elements)()
        checked = 0
        for offset in range(0, elements, chunk_elements):
            count = min(chunk_elements, elements - offset)
            ok(copy, buffer, memory[2].value + offset * 4, count * 4)
            if any(word != expected_word for word in buffer[:count]):
                raise RuntimeError(f"{label} output mismatch at chunk {offset}")
            checked += count
        row = {"Input": label, "Output": "FP32", "Accumulation": "FP32",
               "ComputeType": 68, "MathMode": 16, "Algorithm": 99, "N": n,
               "IterationsPerSample": iterations, "SamplesTflops": samples,
               "MedianTflops": statistics.median(samples),
               "CheckedOutputElements": checked, "ExactExpectedOutput": float(n)}
        results.append(row)
        print(json.dumps(row), flush=True)
finally:
    if context.value:
        ok(sync)
        for event in reversed(events):
            ok(ev_destroy, event)
        for address in reversed(memory):
            ok(free, address)
        if handle.value:
            ok(b_destroy, handle)
        ok(destroy, context)
a.output.parent.mkdir(parents=True, exist_ok=True)
report = {"TimestampUnix": time.time(), "Gpu": name.value.decode(),
          "Bus": a.bus, "CuBlasVersion": version.value,
          "CuBlasDll": dll_path.name, "CuBlasSha256": hashlib.file_digest(dll_path.open("rb"), "sha256").hexdigest(),
          "Before": before, "After": snapshot(), "Results": results,
          "TotalGpuAllocationBytes": elements * 8,
          "BenchmarkScriptSha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
          "FirmwareAccess": False, "PowerOrClockChanges": False,
          "Limits": "cuBLAS matrix throughput with constant inputs; not application speed or exhaustive correctness"}
a.output.write_text(json.dumps(report, indent=2), encoding="utf-8")
print("Saved", a.output, flush=True)
