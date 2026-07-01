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
| **GPU** | NVIDIA GPU: Maxwell (sm_50) or newer (Legacy); Turing (sm_75) or newer (Modern) | RTX 40xx (Ada Lovelace, sm_89) |
| **NVIDIA Driver** | ≥ 528.x (CUDA 12.x compatible) | Latest |
| **CUDA Toolkit** | 12.9 (Legacy) & 13.3 (Modern) | 12.9 & 13.3 |
| **Visual Studio** | 2026 with MSVC v143+ | VS 2026 v18 (v145/v146 toolsets) |
| **CMake** | 3.28+ (for FTXUI rebuild only) | Bundled with VS 2026 |
| **Disk Space** | ~2 GB (Debug + Release) | — |
| **RAM** | 8 GB | 16 GB recommended |

> **Note:** While CUDA 13.3 dropped support for Maxwell (sm_5x), Pascal (sm_6x), and Volta (sm_7x) architectures, the Legacy configuration compiled with CUDA 12.9 preserves compatibility for these older GPUs (e.g. MX150/MX250).

---

## 2. Technology Stack

### Core

| Component | Technology | Version |
|---|---|---|
| **GPU Compute** | NVIDIA CUDA | 12.9 (Legacy) & 13.3 (Modern) |
| **Host Compiler** | MSVC (C++17) | v143 (`14.44` for Legacy) / v145 (`14.51` for Modern) |
| **CUDA Compiler** | `nvcc` | V12.9 / V13.3.33 |
| **Build System** | MSBuild (`.vcxproj`) | VS 2026 |
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

## 2b. Runtime Distribution Requirements

> This section answers: **"What does a user need installed to run SHA256.exe?"**

### What is already baked into the exe (no install needed)

| Component | How | Notes |
|---|---|---|
| CUDA runtime | `cudart_static.lib` — statically linked | No CUDA Toolkit install required |
| FTXUI (screen/dom/component) | Static `.lib` files | No separate install |
| GPU kernels | Compiled into `.nv_fatb` section | Covers sm_50 → sm_90 (Legacy) / sm_75 → sm_121 (Modern) |
| Windows system DLLs | Part of OS | Always present on Win10/11 |

### What the user MUST have installed

| Dependency | Supplied by | Required? |
|---|---|---|
| **NVIDIA GPU Driver** | NVIDIA (auto-installed with GPU) | ✅ Required — provides `nvml.dll` |
| **Visual C++ 2022 Redistributable** | Microsoft | ✅ Required — provides `MSVCP140.dll`, `VCRUNTIME140.dll` |

> **Note:** The VC++ Redistributable is already present on most Windows machines (it is installed by VS 2026, many games, and countless apps). If missing, Windows will show a *"VCRUNTIME140.dll not found"* error at launch.

### How to include this information when creating a release

**Option 1 — Bundle the Redistributable installer (Recommended)**

Download and include in your release package:
```
https://aka.ms/vs/17/release/vc_redist.x64.exe   (~25 MB)
```
Add a `README.txt` in the release folder:
```
SHA256 CUDA Hasher - Release Package
=====================================
Requirements:
  1. NVIDIA GPU (Maxwell/Pascal or newer)
  2. NVIDIA GPU Driver (latest recommended)
  3. Visual C++ 2022 Redistributable:
     Run vc_redist.x64.exe if the app fails to start.

Files:
  SHA256.exe         - Smart Launcher
  SHA256_Modern.exe  - Modern CUDA 13.3 binary
  SHA256_Legacy.exe  - Legacy CUDA 12.9 binary
  vc_redist.x64.exe  - VC++ Runtime (install if needed)
```

**Option 2 — Add a Windows Version Resource to the exe**

A `.rc` resource file embeds dependency metadata visible in **File → Properties → Details** in Explorer. Example `SHA256.rc`:
```rc
VS_VERSION_INFO VERSIONINFO
  FILEVERSION    1,0,0,0
  PRODUCTVERSION 1,0,0,0
  FILEFLAGSMASK  0x3fL
  FILEFLAGS      0x0L
  FILEOS         VOS_NT_WINDOWS32
  FILETYPE       VFT_APP
BEGIN
  BLOCK "StringFileInfo"
  BEGIN
    BLOCK "040904b0"
    BEGIN
      VALUE "CompanyName",      "IITJ MTech"
      VALUE "FileDescription",  "SHA256 CUDA & CPU Hasher"
      VALUE "FileVersion",      "1.0.0.0"
      VALUE "ProductName",      "SHA256 CUDA Hasher"
      VALUE "ProductVersion",   "1.0.0.0"
      VALUE "Comments",         "Requires: NVIDIA GPU Driver + VC++ 2022 Redistributable"
      VALUE "LegalCopyright",   "Copyright 2026"
    END
  END
  BLOCK "VarFileInfo"
  BEGIN
    VALUE "Translation", 0x409, 1200
  END
END
```
Add `SHA256.rc` to the project via **Add → Resource → Version**. This shows up in Windows Explorer under the Details tab.

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

### Step 2 — Install Visual Studio 2026

1. Download from: https://visualstudio.microsoft.com/downloads/
2. In the installer, select the **"Desktop development with C++"** workload
3. Ensure the following individual components are checked:
   - MSVC v143 (or v145/v146) build tools
   - Windows 10/11 SDK
   - CMake tools for Windows (needed to rebuild FTXUI if required)

### Step 3 — Clone / Open the Repository

```powershell
git clone <repo-url>
cd SHA256
```

Open `SHA256.slnx` in Visual Studio 2026.

---

## 5. Building FTXUI (Third-Party Dependency)

FTXUI **Release** libs are pre-built and included in `thirdparty/ftxui/build/Release/`.

The **Debug** libs must be compiled once before your first Debug build.

### Option A — Using MSBuild (Recommended, no CMake needed)

Open a **Developer PowerShell for VS 2026** and run:

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
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Debug
cmake --build build --config Release
```

> You only need to do this **once**. The libs are then reused for all subsequent builds of the main project.

---

## 6. Building the Project

### Using Visual Studio 2026 (GUI)

1. Open `SHA256.slnx`
2. Select the desired configuration from the toolbar:
   - **Debug | x64** — targets your local GPU (sm_89 / RTX 40xx) for fast builds
   - **Release | x64** — targets all supported CUDA 13.3 architectures
3. Press **Ctrl+Shift+B** or go to **Build → Build Solution**
4. Output executable:
   - Debug: `x64\Debug\SHA256.exe`
   - Release: `x64\Release\SHA256.exe`

### Using MSBuild (Command Line)

Open **Developer PowerShell for VS 2026**:

```powershell
$msbuild = "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe"
$proj    = "C:\path\to\SHA256\SHA256.vcxproj"

# Debug build (Modern CUDA 13.3)
& $msbuild $proj /p:Configuration=Debug /p:Platform=x64 /v:minimal

# Modern Release build (CUDA 13.3, CC 7.5 to 12.1)
& $msbuild $proj /p:Configuration=Release /p:Platform=x64 /p:CudaVersion=13.3 /t:Rebuild /v:minimal

# Legacy Release build (CUDA 12.9, CC 5.0 to 9.0)
& $msbuild $proj /p:Configuration=Release /p:Platform=x64 /p:CudaVersion=12.6 /p:CudaToolkitCustomDir="C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.9" /p:VCToolsVersion=14.44.35207 /t:Rebuild /v:minimal
```

### Automated Packaging (Recommended)
To automatically compile both builds (Legacy and Modern), compile the launcher, and prepare the release package:
```powershell
powershell -ExecutionPolicy Bypass -File .\build_all.ps1
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

You must use a **Developer PowerShell / Developer Command Prompt for VS 2026**, or provide the full path to `MSBuild.exe`:

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

*Last updated: 2026-07-01*

---

## 9. Multi-Target Build & Launcher Build (Legacy/Modern)

To support both legacy GPUs (e.g., Pascal/Maxwell architectures using CUDA 12.x/12.9) and modern GPUs (using CUDA 13.3), the project has a multi-target build script that builds both versions and bundles them with a dynamic launcher.

### Prerequisites

To execute the multi-target build, your system must have:
1. **CUDA Toolkit 12.9** and **CUDA Toolkit 13.3** installed in their default locations.
2. **Visual Studio 2026** (or similar) with both the **MSVC v143** and **MSVC v145** toolsets installed (needed to avoid compiler front-end parser crashes on legacy targets).

### Multi-Target Build Procedure

1. Open a PowerShell window.
2. Navigate to the repository root directory.
3. Run the automated packaging script:
   ```powershell
   powershell -ExecutionPolicy Bypass -File .\build_all.ps1
   ```

### Output & Distribution Folder

After a successful run of `build_all.ps1`, the `Release_Package/` folder is populated with:
- `SHA256.exe` (The dynamic launcher: queries the system GPU's compute capability and runs the correct binary).
- `SHA256_Modern.exe` (Optimized CUDA 13.3 build, CC >= 7.5).
- `SHA256_Legacy.exe` (Maximum compatibility CUDA 12.x build, CC < 7.5).
- `README.txt` (User installation and running instruction guide).

