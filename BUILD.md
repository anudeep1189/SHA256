# SHA256 CUDA Hasher — Build Instructions

GPU-accelerated SHA-256 hashing implemented in CUDA C++, with an interactive terminal UI powered by FTXUI.

---

## Table of Contents

1. [System Requirements](#1-system-requirements)
2. [Technology Stack](#2-technology-stack)
3. [Repository Structure](#3-repository-structure)
4. [Pre-Build Setup](#4-pre-build-setup)
5. [Building FTXUI (Third-Party Dependency)](#5-building-ftxui-third-party-dependency)
6. [Building the Project](#6-building-the-project)
7. [GPU Architecture Targets](#7-gpu-architecture-targets)
8. [Troubleshooting](#8-troubleshooting)

---

## 1. System Requirements

| Requirement | Minimum | Tested With |
|---|---|---|
| **OS** | Windows 10 (64-bit) | Windows 11 |
| **GPU** | NVIDIA GPU, Turing (sm_75) or newer | RTX 40xx (Ada Lovelace, sm_89) |
| **NVIDIA Driver** | ≥ 576.x (CUDA 13.x compatible) | Latest |
| **CUDA Toolkit** | 13.3 | 13.3 (V13.3.33) |
| **Visual Studio** | 2022 with MSVC v143+ | VS 2022 v18 (v145 toolset) |
| **CMake** | 3.28+ (for FTXUI rebuild only) | Bundled with VS 2022 |
| **Disk Space** | ~2 GB (Debug + Release) | — |
| **RAM** | 8 GB | 16 GB recommended |

> **Note:** CUDA 13.3 dropped support for Maxwell (sm_5x), Pascal (sm_6x), and Volta (sm_7x) architectures. GPUs older than Turing (GTX 16xx / RTX 20xx) are **not supported**.

---

## 2. Technology Stack

### Core

| Component | Technology | Version |
|---|---|---|
| **GPU Compute** | NVIDIA CUDA | 13.3 |
| **Host Compiler** | MSVC (C++17) | v145 (VS 2022) |
| **CUDA Compiler** | `nvcc` | V13.3.33 |
| **Build System** | MSBuild (`.vcxproj`) | VS 2022 |
| **Language Standard** | C++17 | `/std:c++17` |

### CUDA Runtime Libraries

| Library | Purpose |
|---|---|
| `cudart_static.lib` | CUDA runtime (statically linked) |
| `nvml.lib` | NVIDIA Management Library — GPU power & temperature metrics |

### Third-Party Libraries

| Library | Version | Purpose |
|---|---|---|
| **FTXUI** | 7.0.0 | Terminal UI framework (interactive TUI) |

#### FTXUI Sub-Libraries (all must be built)

| Library | Purpose |
|---|---|
| `ftxui-screen.lib` | Terminal screen abstraction |
| `ftxui-dom.lib` | DOM-style layout engine |
| `ftxui-component.lib` | Interactive UI components (inputs, buttons, etc.) |

### Windows System Libraries

`kernel32`, `user32`, `gdi32`, `winspool`, `comdlg32`, `advapi32`, `shell32`, `ole32`, `oleaut32`, `uuid`, `odbc32`, `odbccp32`

---

## 3. Repository Structure

```
SHA256/
├── src/                          # All source files
│   ├── main.cu                   # Application entry point
│   ├── sha256.cu / sha256.cuh    # GPU SHA-256 kernel
│   ├── sha256_cpu.cu / .h        # CPU SHA-256 reference implementation
│   ├── app_controller.cu / .h    # Top-level application controller
│   ├── cmd_ui.cpp / .h           # Terminal UI (FTXUI integration)
│   ├── input_handler.cpp / .h    # Input parsing
│   ├── gpu_device_info.cu / .h   # GPU query utilities
│   ├── power_metrics.cu / .h     # NVML power/temp monitoring
│   └── config.h                  # Global type definitions & feature flags
│
├── thirdparty/
│   └── ftxui/                    # FTXUI library source + pre-built VS project
│       ├── include/              # Headers (already present)
│       ├── build/                # VS project & Release libs (pre-built)
│       │   ├── Release/          # ftxui-*.lib (Release config)
│       │   └── Debug/            # ftxui-*.lib (Debug config — must be built)
│       └── CMakeLists.txt        # Use this if rebuilding from scratch
│
├── SHA256.vcxproj                # Main VS project file
├── SHA256.slnx                   # VS solution file
└── BUILD.md                      # This file
```

---

## 4. Pre-Build Setup

### Step 1 — Install CUDA Toolkit 13.3

1. Download from: https://developer.nvidia.com/cuda-downloads
2. Select: **Windows → x86_64 → 11/10 → exe (local)**
3. Run the installer — choose **Custom** install and include:
   - CUDA Toolkit
   - CUDA Visual Studio Integration
   - NVML (included in toolkit)
4. Verify installation:
   ```powershell
   nvcc --version
   # Expected: Cuda compilation tools, release 13.3, V13.3.33
   ```

### Step 2 — Install Visual Studio 2022

1. Download from: https://visualstudio.microsoft.com/downloads/
2. In the installer, select the **"Desktop development with C++"** workload
3. Ensure the following individual components are checked:
   - MSVC v143 (or v145) build tools
   - Windows 10/11 SDK
   - CMake tools for Windows (needed to rebuild FTXUI if required)

### Step 3 — Clone / Open the Repository

```powershell
git clone <repo-url>
cd SHA256
```

Open `SHA256.slnx` in Visual Studio 2022.

---

## 5. Building FTXUI (Third-Party Dependency)

FTXUI **Release** libs are pre-built and included in `thirdparty/ftxui/build/Release/`.

The **Debug** libs must be compiled once before your first Debug build.

### Option A — Using MSBuild (Recommended, no CMake needed)

Open a **Developer PowerShell for VS 2022** and run:

```powershell
cd C:\path\to\SHA256

# Build Debug libs
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" `
    "thirdparty\ftxui\build\ftxui.slnx" `
    /p:Configuration=Debug /p:Platform=x64 /v:minimal
```

After success, verify these three files exist:

```
thirdparty\ftxui\build\Debug\ftxui-screen.lib
thirdparty\ftxui\build\Debug\ftxui-dom.lib
thirdparty\ftxui\build\Debug\ftxui-component.lib
```

### Option B — Using CMake (Full rebuild from source)

```powershell
cd thirdparty\ftxui
cmake -S . -B build -G "Visual Studio 18 2022" -A x64
cmake --build build --config Debug
cmake --build build --config Release
```

> You only need to do this **once**. The libs are then reused for all subsequent builds of the main project.

---

## 6. Building the Project

### Using Visual Studio 2022 (GUI)

1. Open `SHA256.slnx`
2. Select the desired configuration from the toolbar:
   - **Debug | x64** — targets your local GPU (sm_89 / RTX 40xx) for fast builds
   - **Release | x64** — targets all supported CUDA 13.3 architectures
3. Press **Ctrl+Shift+B** or go to **Build → Build Solution**
4. Output executable:
   - Debug: `x64\Debug\SHA256.exe`
   - Release: `x64\Release\SHA256.exe`

### Using MSBuild (Command Line)

Open **Developer PowerShell for VS 2022**:

```powershell
$msbuild = "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe"
$proj    = "C:\path\to\SHA256\SHA256.vcxproj"

# Debug build
& $msbuild $proj /p:Configuration=Debug /p:Platform=x64 /v:minimal

# Release build
& $msbuild $proj /p:Configuration=Release /p:Platform=x64 /v:minimal
```

---

## 7. GPU Architecture Targets

The project is configured to compile native GPU binaries (cubin) for the following CUDA compute capabilities:

### Debug Configuration
Fast single-architecture build targeting the development machine:

| SM | Architecture | GPUs |
|---|---|---|
| **sm_89** | Ada Lovelace | RTX 4090, 4080, 4070, 4060, L40 |

### Release Configuration
Universal binary covering all architectures supported by CUDA 13.3:

| SM | Architecture | GPUs |
|---|---|---|
| sm_75 | Turing | RTX 2080, 2070, 2060, T4, GTX 16xx |
| sm_80 | Ampere | A100, RTX 3080, 3070 Ti |
| sm_86 | Ampere | RTX 3060, 3060 Ti, 3070 |
| sm_87 | Ampere (Jetson) | Jetson Orin |
| sm_88 | Ampere (reserved) | — |
| **sm_89** | Ada Lovelace | RTX 4090, 4080, 4070, 4060, L40 |
| sm_90 | Hopper | H100, H200 |
| sm_100 | Blackwell | B100, B200, RTX 5090 |
| sm_103 | Blackwell | — |
| sm_110 | Rubin | — |
| sm_120 | Rubin Ultra | — |
| sm_121 | Rubin Ultra | — |

> **Note:** Architectures older than sm_75 (Maxwell, Pascal, Volta) were dropped in CUDA 13.x and are **not supported**.

---

## 8. Troubleshooting

### `cannot open file 'thirdparty\ftxui\build\Debug\ftxui-screen.lib'`

The FTXUI Debug libs have not been compiled yet. Follow [Section 5](#5-building-ftxui-third-party-dependency) to build them.

---

### `nvcc fatal: Unsupported gpu architecture 'compute_50'`

Your `CodeGeneration` setting in `SHA256.vcxproj` includes architectures that CUDA 13.3 has dropped (sm_50–sm_72). The correct range starts at **sm_75**.

To verify what your local nvcc supports:
```powershell
& "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.3\bin\nvcc.exe" --list-gpu-arch
```

---

### `msbuild` not recognized

You must use a **Developer PowerShell / Developer Command Prompt for VS 2022**, or provide the full path to `MSBuild.exe`:

```powershell
"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe"
```

---

### Build succeeds but NVML / power metrics show 0

NVML requires the **NVIDIA Display Driver** to be installed (not just the CUDA Toolkit). Ensure your driver version is compatible with CUDA 13.3 (≥ 576.x).

---

### Warning: `C4244 conversion from 'unsigned __int64' to 'BYTE'`

This is a **pre-existing, non-fatal warning** in `sha256_cpu.cu` related to bit-manipulation in the SHA-256 padding step. The truncation is intentional by the SHA-256 specification. The build and output are correct.

---

*Last updated: 2026-06-30*
