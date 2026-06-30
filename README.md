# SHA256 CUDA & CPU Hasher

GPU-accelerated SHA-256 hashing benchmark program implemented in CUDA C++ and C++17, with an interactive terminal UI powered by FTXUI.

---

##  Tools & System Requirements

To build and run this project, your development system must have:

1. **NVIDIA GPU**: Turing architecture or newer (RTX 20-series, 30-series, 40-series, 50-series, GTX 1660/1660 Ti, or newer).
2. **NVIDIA Display Driver**: Version compatible with CUDA 13.3 (Driver version $\ge$ 576.x recommended).
3. **NVIDIA CUDA Toolkit 13.3**: For compiling the CUDA kernels via `nvcc`.
4. **Visual Studio 2026**: Installed with the **"Desktop development with C++"** workload (provides the MSVC host compiler, MSBuild, and Windows SDK).

---

##  Quick Start: Build Instructions

Follow these steps in order to build the project:

### 1. Build the FTXUI Library Dependencies (First-Time Only)
The project uses FTXUI for its terminal user interface. To build the required **Debug** binaries once, open **Developer PowerShell for VS 2026** in the repository root and run:

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" `
    "thirdparty\ftxui\build\ftxui.slnx" `
    /p:Configuration=Debug /p:Platform=x64 /v:minimal
```

### 2. Build the Main Project

#### Option A: Using the VS 2026 IDE
1. Open the solution file `SHA256.slnx` in Visual Studio 2026.
2. Select the configuration (**Debug** for local development, or **Release** for multi-architecture distribution) and platform (**x64**).
3. Build the solution (**Ctrl+Shift+B**).

#### Option B: Using MSBuild (Command Line)
Open **Developer PowerShell for VS 2026** and run:
```powershell
# For Debug Build (sm_89 target for fast compile)
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" `
    "SHA256.vcxproj" /p:Configuration=Debug /p:Platform=x64 /v:minimal

# For Release Build (sm_75 to sm_121 multi-architecture target)
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" `
    "SHA256.vcxproj" /p:Configuration=Release /p:Platform=x64 /v:minimal
```

---

##  Creating the Portable Build

To create a clean, portable distribution package that end-users can run without installing developer tools or CUDA runtimes:

1. Create a distribution folder (e.g. `Release_Package`).
2. Copy the compiled executable to it:
   ```powershell
   Copy-Item -Path "x64\Release\SHA256.exe" -Destination "Release_Package\SHA256.exe" -Force
   ```
3. Copy/create a `README.txt` detailing usage and system requirements.
4. Download the [Visual C++ Redistributable Installer](https://aka.ms/vs/17/release/vc_redist.x64.exe) (`vc_redist.x64.exe`) and place it inside the folder so users can install it if they get a missing `.dll` error.
5. Compress the `Release_Package` folder into a ZIP file to distribute it.

---
## ScreenShot
<img width="1908" height="970" alt="image" src="https://github.com/user-attachments/assets/88b6dbf4-d4a6-497a-8cc7-2950a70e61f6" />


##  Additional Documentation
For detailed troubleshooting, deep-dives into the technology stack, and GPU architecture compatibility listings, see **[BUILD.md](BUILD.md)**.
