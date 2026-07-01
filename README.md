# SHA256 CUDA & CPU Hasher

GPU-accelerated SHA-256 hashing benchmark program implemented in CUDA C++ and C++17, with an interactive terminal UI powered by FTXUI.

---

## 💻 Tools & System Requirements

To build and run this project, your development system must have:

1. **NVIDIA GPU**:
   - For Legacy build: Maxwell or Pascal architecture or newer (GTX 9xx, 10xx, MX150/MX250 series, or newer).
   - For Modern build: Turing architecture or newer (RTX 20-series, 30-series, 40-series, 50-series, GTX 1660/1660 Ti, or newer).
2. **NVIDIA Display Driver**: Version compatible with CUDA 13.3 (Driver version $\ge$ 576.x recommended).
3. **NVIDIA CUDA Toolkit**:
   - **CUDA 13.3**: For compiling the modern targets.
   - **CUDA 12.9**: For compiling the legacy targets.
4. **Visual Studio 2026**: Installed with the **"Desktop development with C++"** workload (provides the MSVC host compiler, MSBuild, and Windows SDK) along with the **MSVC v143** and **MSVC v145** compiler toolsets.

---

## 🛠️ Quick Start: Build Instructions

Follow these steps in order to build the project:

### 1. Build the FTXUI Library Dependencies (First-Time Only)
The project uses FTXUI for its terminal user interface. To build the required **Debug** binaries once, open **Developer PowerShell for VS 2026** in the repository root and run:

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" `
    "thirdparty\ftxui\build\ftxui.slnx" `
    /p:Configuration=Debug /p:Platform=x64 /v:minimal
```

### 2. Build the Main Project

#### Option A: Run the Automated Packaging Build (Recommended)
You can run the build script to compile the Legacy build, Modern build, Launcher, and package them all automatically:
```powershell
powershell -ExecutionPolicy Bypass -File .\build_all.ps1
```

#### Option B: Using MSBuild (Command Line Manual Builds)
Open **Developer PowerShell for VS 2026** and run:
```powershell
# For Legacy Release Build (CUDA 12.9, CC 5.0 to 9.0)
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" `
    "SHA256.vcxproj" /p:Configuration=Release /p:Platform=x64 /p:CudaVersion=12.6 /p:CudaToolkitCustomDir="C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.9" /p:VCToolsVersion=14.44.35207 /t:Rebuild /v:minimal

# For Modern Release Build (CUDA 13.3, CC 7.5 to 12.1)
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" `
    "SHA256.vcxproj" /p:Configuration=Release /p:Platform=x64 /p:CudaVersion=13.3 /t:Rebuild /v:minimal
```

#### Option C: Using the VS 2026 IDE
1. Open the solution file `SHA256.slnx` in Visual Studio 2026.
2. Select the configuration (**Debug** or **Release**) and platform (**x64**).
3. Build the solution (**Ctrl+Shift+B**). *Note: By default, the IDE builds with CUDA 13.3.*

---

## 📦 Creating the Portable Build

To create a clean, portable distribution package that end-users can run without installing developer tools or CUDA runtimes, simply execute:

```powershell
powershell -ExecutionPolicy Bypass -File .\build_all.ps1
```

This script will automatically generate the [Release_Package](file:///c:/Anudeep/Repo/IITJ_MTech/SHA256/Release_Package) folder containing:
1. `SHA256.exe` - Standalone GPU-detecting launcher.
2. `SHA256_Modern.exe` - High-performance CUDA 13.3 build (CC >= 7.5).
3. `SHA256_Legacy.exe` - Maximum compatibility CUDA 12.x build (CC < 7.5).
4. `README.txt` - User guide detailing requirements and launcher options.

---
## ScreenShot
<img width="1908" height="970" alt="image" src="https://github.com/user-attachments/assets/88b6dbf4-d4a6-497a-8cc7-2950a70e61f6" />


## 📚 Additional Documentation
For detailed troubleshooting, deep-dives into the technology stack, and GPU architecture compatibility listings, see **[BUILD.md](BUILD.md)**.

