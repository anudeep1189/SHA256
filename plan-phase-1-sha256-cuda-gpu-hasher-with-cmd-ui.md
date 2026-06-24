# 🎯 Phase 1 — SHA256 CUDA GPU Hasher with CMD UI

## Context & Decisions

- **Workspace**: `C:\Users\as01\Downloads\cuda-hashing-algos-master\cuda-hashing-algos-master\`
- **Existing files used as-is**:
  - `config.h` — type defs (`BYTE`, `WORD`, `LONG`), feature flags
  - `SHA256.cuh` — declares `mcm_cuda_sha256_hash_batch(BYTE* in, WORD inlen, BYTE* out, WORD n_batch)`
  - `SHA256.cu` — full CUDA kernel implementation (batch hashing, 256 threads/block, each thread hashes one item)
- **SOLID Principles**: Each new file has single responsibility, dependencies injected
- **NVML**: Included for real power measurement
- **Thread Input**: User specifies `n_batch` (total parallel hash operations). Internally the existing kernel uses 256 threads/block and calculates blocks as `(n_batch + 255) / 256`
- **Phase 1 scope**: GPU execution only

## Existing SHA256 API Understanding

```cpp
// SHA256.cuh
void mcm_cuda_sha256_hash_batch(BYTE* in, WORD inlen, BYTE* out, WORD n_batch);
```

- `in` — input buffer of size `inlen * n_batch` (same data replicated for each batch item)
- `inlen` — length of each input item in bytes
- `out` — output buffer of size `32 * n_batch` (32 bytes SHA256 digest per item)
- `n_batch` — number of parallel hash operations (maps to user's "Total Threads")

For single-input hashing: replicate the input `n_batch` times into the input buffer, run kernel, read first 32 bytes of output as the hash result. All outputs will be identical (correctness check).

## File Structure (SOLID — No new SHA256 kernel)

```
cuda-hashing-algos-master/
├── config.h                  (existing — no changes)
├── SHA256.cuh                (existing — used directly)
├── SHA256.cu                 (existing — used directly)
├── gpu_device_info.h         (NEW — GPU discovery interface)
├── gpu_device_info.cu        (NEW — GPU discovery implementation)
├── power_metrics.h           (NEW — NVML + timing interface)
├── power_metrics.cu          (NEW — NVML + timing implementation)
├── input_handler.h           (NEW — text/file reading interface)
├── input_handler.cpp         (NEW — text/file reading implementation)
├── cmd_ui.h                  (NEW — console UI interface)
├── cmd_ui.cpp                (NEW — console UI implementation)
├── app_controller.h          (NEW — orchestrator interface)
├── app_controller.cu         (NEW — orchestrator calling SHA256 kernel + metrics)
├── main.cu                   (NEW — entry point, wires dependencies)
```

## UI Layout

```
╔══════════════════════════════════════════════════════╗
║   SHA256 CUDA HASHER v1.0                           ║
╠══════════════════════════════════════════════════════╣
║ Input Mode:  [●] Text    [○] File                   ║
║                                                     ║
║ Text Input:  [________________________________]     ║
║ File Path:   [________________________________] [..]║
║                                                     ║
║ GPU: NVIDIA GeForce RTX 3080                        ║
║ Available CUDA Cores: 8704                          ║
║ Total Threads: [1024          ]                     ║
║                                                     ║
║           [ ▶ RUN HASH GPU ]                        ║
╠══════════════════════════════════════════════════════╣
║ RESULTS                                             ║
║ ──────────────────────────────────────────────────  ║
║ SHA256:           a7f3c8...d92b1e                   ║
║ Execution Time:   0.00342 s                         ║
║ Avg Power Draw:   85.2 W                            ║
║ Total Energy:     0.291 J                           ║
║ Throughput:       1245.6 MB/s                       ║
╚══════════════════════════════════════════════════════╝
```

## Key Technical Details

### GPU Device Info (`gpu_device_info.h/.cu`)
- `GPUDeviceInfo` struct: `deviceName`, `cudaCoreCount`, `maxThreadsPerBlock`, `maxTotalThreads`, `totalMemory`
- `queryGPUDevice()` — calls `cudaGetDeviceProperties`, computes CUDA cores from `multiProcessorCount × coresPerSM` (architecture-based lookup table for compute capability)
- `validateBatchCount(int n_batch)` — ensures n_batch ≤ reasonable limit based on device memory (each batch item needs `inlen + 32` bytes of GPU memory)

### Power Metrics (`power_metrics.h/.cu`)
- `PowerMetrics` class:
  - `init()` / `shutdown()` — wraps `nvmlInit()` / `nvmlShutdown()`
  - `startSampling()` — spawns `std::thread` polling `nvmlDeviceGetPowerUsage()` every 100ms, stores samples in vector
  - `stopSampling()` — joins thread, computes average
  - `startTimer()` / `stopTimer()` — wraps `cudaEventCreate`, `cudaEventRecord`, `cudaEventSynchronize`, `cudaEventElapsedTime`
  - `getElapsedTime()` → seconds
  - `getAveragePower()` → watts
  - `getEnergy()` → joules (avgPower × time)
  - `getThroughput(size_t dataSize)` → MB/s (dataSize / time)

### Input Handler (`input_handler.h/.cpp`)
- `InputHandler` class:
  - `readFromText(const std::string& text)` → `std::vector<BYTE>`
  - `readFromFile(const std::string& path)` → `std::vector<BYTE>`
  - `isValidFilePath(const std::string&)` → bool
  - `getErrorMessage()` → string (last validation error)

### CMD UI (`cmd_ui.h/.cpp`)
- Win32 Console API based (`windows.h`)
- `CmdUI` class:
  - `initialize(int width=80, int height=30)` — sets console buffer size, enables `ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT`
  - `drawFrame()` — Unicode box-drawing borders
  - `drawRadioButtons(InputMode current)` — Text/File toggle, clickable regions
  - `drawTextbox(int row, string label, string value, bool active)` — active = cyan, inactive = grey
  - `drawGPUInfo(GPUDeviceInfo&)` — device name, core count display
  - `drawThreadInput(int value)` — editable number field
  - `drawButton(string label, int row, int col)` — clickable RUN button
  - `drawResults(GPUMetrics&)` — formatted output panel, scrollable if overflow
  - `runEventLoop(AppController&)` — reads `INPUT_RECORD`, dispatches `MOUSE_EVENT` (click detection via region bounds) and `KEY_EVENT` (textbox input)
- Enforcement: `InputMode` enum controls which textbox is active; inactive one drawn in dark grey and ignores keystrokes
- Scrolling: results panel uses virtual buffer, scroll wheel events shift viewport

### App Controller (`app_controller.h/.cu`)
- `AppController` class — orchestrates all components:
  - Holds references to `CmdUI`, `InputHandler`, `GPUDeviceInfo`, `PowerMetrics`
  - `onRunHashGPU()`:
    1. Get input bytes from `InputHandler` (text or file based on mode)
    2. Get `n_batch` from UI thread count field
    3. Validate via `GPUDeviceInfo::validateBatchCount()`
    4. Prepare input buffer: replicate input data `n_batch` times
    5. Allocate output buffer: `32 * n_batch` bytes
    6. `PowerMetrics::startSampling()` + `PowerMetrics::startTimer()`
    7. Call `mcm_cuda_sha256_hash_batch(inBuf, inlen, outBuf, n_batch)`
    8. `PowerMetrics::stopTimer()` + `PowerMetrics::stopSampling()`
    9. Extract hash from first 32 bytes of output, convert to hex string
    10. Build `GPUMetrics` struct (time, power, energy, throughput)
    11. Pass to `CmdUI::drawResults()`
  - `run()` — initializes all components, draws initial UI, enters event loop

### Main Entry (`main.cu`)
- Instantiates: `GPUDeviceInfo`, `PowerMetrics`, `InputHandler`, `CmdUI`, `AppController`
- Calls `queryGPUDevice()` at startup
- Passes GPU info to UI for display
- Calls `appController.run()` to start

### Libraries Required
- CUDA Runtime (`cuda_runtime.h`) — already in project
- NVML (`nvml.h`, link `nvml.lib`) — from CUDA Toolkit
- Windows API (`windows.h`) — for console UI
- C++17 Standard: `<string>`, `<vector>`, `<thread>`, `<mutex>`, `<fstream>`, `<iomanip>`, `<sstream>`

### Build Notes
- Compile `.cu` files with `nvcc`
- Compile `.cpp` files with MSVC
- Link: `cudart.lib`, `nvml.lib`
- The existing `SHA256.cu` uses `extern "C"` wrapper — compatible with linking from other `.cu` files

**Progress**: 100% [██████████]

**Last Updated**: 2026-06-23 08:11:01

## 📝 Plan Steps
- ✅ **Create `gpu_device_info.h` and `gpu_device_info.cu` — implement `GPUDeviceInfo` struct (deviceName, cudaCoreCount, maxTotalThreads, totalMemory) and `queryGPUDevice()` function using `cudaGetDeviceProperties`; include `validateBatchCount(int)` that checks memory constraints (`n_batch * (inlen + 32)` ≤ available memory); include architecture lookup table for cores-per-SM based on compute capability**
- ✅ **Create `power_metrics.h` and `power_metrics.cu` — implement `PowerMetrics` class with NVML lifecycle (`init`/`shutdown`), background power sampling thread (polls `nvmlDeviceGetPowerUsage` at 100ms intervals into a vector), CUDA event timing (`startTimer`/`stopTimer`/`getElapsedTime`), and computed metrics (`getAveragePower`, `getEnergy`, `getThroughput(size_t dataSize)`)**
- ✅ **Create `input_handler.h` and `input_handler.cpp` — implement `InputHandler` class with `readFromText(const std::string&)` returning `std::vector<BYTE>`, `readFromFile(const std::string& path)` reading binary file into `std::vector<BYTE>`, plus validation (`isValidFilePath`, `isInputEmpty`, `getErrorMessage`)**
- ✅ **Create `cmd_ui.h` and `cmd_ui.cpp` — implement `CmdUI` class using Win32 Console API; include `initialize()` (set buffer size, enable mouse input), `drawFrame()` (box borders), `drawRadioButtons()` (Text/File mode with click regions), `drawTextbox()` (active/greyed state per InputMode), `drawGPUInfo()`, `drawThreadInput()`, `drawButton()` (RUN HASH GPU with click region), `drawResults(GPUMetrics)` (formatted output panel with scroll support), and `runEventLoop()` dispatching MOUSE_EVENT clicks to registered regions and KEY_EVENT to active textbox**
- ✅ **Create `app_controller.h` and `app_controller.cu` — implement `AppController` class that holds references to all components; implement `onRunHashGPU()` which: reads input, validates, replicates data into `n_batch`-sized buffer, calls `mcm_cuda_sha256_hash_batch()` from existing `SHA256.cuh`, wraps call with power metrics timing, extracts 32-byte hash as hex string, computes throughput as `(inlen * n_batch) / time`, builds GPUMetrics struct, sends to UI; implement `run()` as the main application loop**
- ✅ **Create `main.cu` — entry point that instantiates `GPUDeviceInfo`, `PowerMetrics`, `InputHandler`, `CmdUI`, `AppController`; calls `queryGPUDevice()` at startup, initializes NVML, passes GPU info to UI, and starts `appController.run()`**

