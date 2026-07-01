SHA256 CUDA & CPU Hasher - Portable Build
=========================================

This release folder contains a dual-executable GPU benchmarking deployment designed to achieve maximum performance and hardware compatibility.

Included Files
--------------
- SHA256.exe        : Smart Launcher that automatically detects your GPU's compute capability and runs the correct version.
- SHA256_Modern.exe : Modern version compiled with CUDA 13.3. Optimized for newer GPUs.
- SHA256_Legacy.exe : Legacy version compiled with CUDA 12.x. Provided for older GPUs (e.g. Pascal, Maxwell architectures).

---

System Requirements
-------------------
1. OS: Windows 10 or Windows 11 (64-bit).
2. GPU: NVIDIA GPU.
   - For SHA256_Modern.exe: Turing architecture or newer (RTX 20xx, 30xx, 40xx, 50xx series, GTX 1660/1660 Ti/1650 Super, or newer).
   - For SHA256_Legacy.exe: Maxwell or Pascal architectures (GTX 9xx, 10xx series, MX150/MX250 series, or newer).
3. GPU Driver: NVIDIA Display Driver installed.

---

How to Run
----------
1. Extract the contents of this folder to any directory.
2. Run `SHA256.exe` (or double-click it).
   The launcher will automatically select and run either `SHA256_Modern.exe` or `SHA256_Legacy.exe` depending on your GPU.
3. (Optional) You can directly run `SHA256_Modern.exe` or `SHA256_Legacy.exe` from your terminal if you want to bypass the launcher.

---

Troubleshooting & Advanced Settings
-----------------------------------
* Launcher Diagnostics:
  To see details about which GPU was detected and which executable was launched, set the environment variable `SHA256_LAUNCHER_DEBUG=1` in your terminal before running `SHA256.exe`.
  Example (PowerShell):
    $env:SHA256_LAUNCHER_DEBUG=1
    .\SHA256.exe

* Error: "VCRUNTIME140.dll" or "MSVCP140.dll" was not found:
  This app requires the Microsoft Visual C++ 2015-2022 Redistributable to be installed on your system. 
  You can download and install it directly from Microsoft using the link below:
  https://aka.ms/vs/17/release/vc_redist.x64.exe

---
Created: 2026-07-01
