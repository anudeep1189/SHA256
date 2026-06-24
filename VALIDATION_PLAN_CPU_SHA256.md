# CPU SHA-256 Implementation Validation Plan

## Summary
This document outlines how to validate the new CPU SHA-256 batch hasher implementation alongside the existing GPU path.

## Build & Run Instructions

### 1. Close Debugger
Close the Visual Studio debugger window (SHA256.exe must not be running) so the linker can create the new executable.

### 2. Clean Rebuild
In Visual Studio:
- Build > Clean Solution
- Build > Rebuild Solution

Or from command line:
```powershell
cd C:\Anudeep\Repo\IITJ_MTech\SHA256
msbuild SHA256.vcxproj /t:Clean,Rebuild /p:Configuration=Debug /p:Platform=x64
```

### 3. Launch Application
Run the rebuilt executable: `x64/Debug/SHA256.exe`

## UI Validation

### Expected Changes
1. **Button Layout**: Two buttons should now appear side-by-side:
   - `[ > RUN HASH GPU ]`  (blue)
   - `[ > RUN HASH CPU ]`   (blue)
   - `[ CLEAR ]`             (red)

2. **Results Title**: Dynamic display based on button clicked:
   - Clicking GPU button: "RESULTS – GPU"
   - Clicking CPU button: "RESULTS – CPU"

### Test Case 1: GPU Path (Unchanged)
- Input: "the quick brown fox"
- Batch Size: 1000
- Click: `[ > RUN HASH GPU ]`
- Expected:
  - Results panel displays with "RESULTS – GPU" title
  - Shows timing metrics for GPU execution
  - Displays power consumption (NVML data)
  - Original hash matches standard SHA-256 for the input

### Test Case 2: CPU Path (New)
- Input: "the quick brown fox"
- Batch Size: 1000
- Click: `[ > RUN HASH CPU ]`
- Expected:
  - Results panel displays with "RESULTS – CPU" title
  - Shows timing metrics for CPU execution
  - Power metrics show 0.0 (not measured for CPU)
  - Original hash should match GPU and standard SHA-256

### Test Case 3: Cross-Validation (Key Test)
- Run both GPU and CPU with same input and batch size
- Compare "Original:" hash from both results
- **Expected: GPU[0] hash == CPU[0] hash**
  - If hashes match: ✅ Implementation is correct
  - If hashes differ: ❌ Investigate nonce logic and bit order

## Reference SHA-256 Test Vectors

Use these standard inputs to verify correctness:

### Vector 1: Empty String
- Input: "" (empty)
- Expected SHA-256: `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`

### Vector 2: "abc"
- Input: "abc"
- Expected SHA-256: `ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad`

### Vector 3: "The quick brown fox jumps over the lazy dog"
- Input: "the quick brown fox jumps over the lazy dog"
- Expected SHA-256: `d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592`

### How to Test:
1. Enter one of the above strings in the Text Input field
2. Set Batch Size: 1
3. Click GPU button, note the "Original:" hash
4. Click CPU button, note the "Original:" hash
5. Compare both to the expected value
6. If all three match: ✅ SHA-256 is correct

## Detailed Validation Steps

### Step 1: Verify UI Changes
- [ ] Two separate buttons visible: GPU and CPU
- [ ] Results title changes based on button clicked
- [ ] Status messages clear on new runs
- [ ] Clear button still works

### Step 2: Single-Threaded CPU Test
- [ ] Input: "test"
- [ ] Batch Size: 1
- [ ] Click CPU button
- [ ] Verify:
  - No error message
  - Hash displays in hex format (64 characters)
  - Timing shows non-zero value
  - Throughput calculated correctly

### Step 3: Batch CPU Processing
- [ ] Input: "hello world"
- [ ] Batch Size: 100000
- [ ] Click CPU button
- [ ] Verify:
  - Processes without error
  - Shows "Hashes Computed: 100000"
  - Multiple sample hashes displayed (at indices 0, 25000, 50000, 75000, 99999)
  - Each hash is different (nonce variation)
  - Timing is reasonable (should be slower than GPU for large batches)

### Step 4: CPU vs GPU Comparison
- [ ] Input: "benchmark test"
- [ ] Batch Size: 50000
- [ ] Run GPU:
  - Note: Single hash time, Batch time, Throughput
- [ ] Run CPU with same input/batch:
  - Note: Single hash time, Batch time, Throughput
- [ ] Verify:
  - Hash[0] GPU == Hash[0] CPU ✅ (cross-validation passes)
  - GPU throughput > CPU throughput (expected—GPU should be faster)
  - CPU timing is deterministic (repeatable)

### Step 5: Hash Uniqueness (Nonce Logic)
- [ ] Input: "same input"
- [ ] Batch Size: 5
- [ ] Run CPU
- [ ] Check displayed hashes:
  - Original (Hash -1): base hash
  - Hash #0: should equal Original (thread 0, no nonce)
  - Hash #1: different (has nonce for i=1)
  - Hash #2: different (has nonce for i=2)
  - Hash #3: different (has nonce for i=3)
  - Hash #4: different (has nonce for i=4)
- [ ] Verify: All hashes #1-#4 are unique and differ from Original

## Performance Expectations

### Single Hash (batch=1)
- GPU: ~1-5 ms (kernel launch overhead)
- CPU: <1 ms (single hash, no parallelism)

### Large Batch (batch=100000)
- GPU: ~100-200 ms (highly parallel)
- CPU: ~5-10 seconds (single-threaded, slow)
- Ratio: GPU ~50-100x faster (depending on CPU/GPU capability)

## Troubleshooting

### Issue: "Cannot open exe for writing"
- **Solution**: Close all SHA256.exe processes and the Visual Studio debugger, then rebuild.

### Issue: CPU button not responding
- **Check**: Verify UIEvent::RUN_CLICKED_CPU is handled in onUIEvent()
- **Action**: Review app_controller.cu line ~60 for event routing

### Issue: CPU and GPU hashes don't match for index 0
- **Likely cause**: Byte order mismatch in nonce or hash finalization
- **Debug**: Check sha256_cpu.cpp lines 180-195 (nonce byte order) and lines 155-170 (final hash byte order)
- **Verify**: Ensure both GPU and CPU use identical big-endian byte order

### Issue: Timeouts or hangs
- **For single-threaded CPU**: Expected behavior for very large batches (e.g., 10,000,000)
- **Mitigation**: Reduce batch size for testing
- **Note**: Future optimization would add std::thread parallelism

## Implementation Checklist

- [x] **sha256_cpu.h** created with public API declaration
- [x] **sha256_cpu.cpp** created with full SHA-256 implementation
  - [x] Identical constants (k[64] array)
  - [x] Identical bitwise macros (CH, MAJ, EP0, EP1, SIG0, SIG1)
  - [x] Identical transform logic (cpu_sha256_transform)
  - [x] Nonce appending for batch uniqueness (threads 1+)
  - [x] Big-endian byte ordering for hash output
- [x] **cmd_ui.h/cpp** updated with CPU button
  - [x] drawRunCPUButton() method
  - [x] REGION_RUN_BUTTON_CPU constant
  - [x] lastComputeMode tracking
  - [x] CPU button click event routing
  - [x] Dynamic results title ("RESULTS – CPU/GPU")
- [x] **app_controller.h/cu** updated with CPU handler
  - [x] onRunHashCPU() method
  - [x] RUN_CLICKED_CPU event case
  - [x] std::chrono timing for CPU
  - [x] Identical sample hash extraction logic
  - [x] Metrics building (skip power for CPU)

## SOLID Principles Compliance

✅ **Single Responsibility**: Each file has one reason to change
- sha256_cpu.*: CPU SHA-256 logic only
- cmd_ui.*: UI rendering and input handling
- app_controller.*: Orchestration between UI and compute backends

✅ **Open/Closed**: New CPU path added without modifying GPU path or existing logic

✅ **Liskov Substitution**: CPU and GPU can be called identically via parallel methods

✅ **Interface Segregation**: Clean public API (mcm_cpu_sha256_hash_batch signature matches GPU version)

✅ **Dependency Inversion**: AppController depends on abstractions (UI events), not concrete GPU/CPU implementations

## Next Steps (Optional Enhancements)

1. **Multithreaded CPU**:
   - Replace single-threaded loop with `std::thread` or `std::execution::par`
   - Expected speedup: 4-8x on modern CPUs

2. **CPU Power Measurement** (Windows-specific):
   - Query WMI or Intel PowerGadget API for CPU socket power
   - Display alongside GPU metrics

3. **Comparative Dashboard**:
   - Run both GPU and CPU automatically
   - Show side-by-side results with speedup ratio
   - Useful for benchmarking and teaching

4. **Test Suite**:
   - Unit tests for sha256_cpu functions
   - Reference vector validation (empty, "abc", known strings)
   - Cross-GPU-CPU hash validation

