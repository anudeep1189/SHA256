SHA256 CUDA & CPU Hasher - Portable Build
=========================================

This is a portable command-line benchmark application that measures SHA-256 hashing performance on both GPU (CUDA) and CPU.

---

System Requirements
-------------------
1. OS: Windows 10 or Windows 11 (64-bit).
2. GPU: NVIDIA GPU with Turing architecture or newer (RTX 20-series, 30-series, 40-series, 50-series, GTX 1660/1660 Ti/1650 Super, or newer).
3. GPU Driver: NVIDIA Display Driver installed (handles NVML power metrics and GPU compute).

---

How to Run
----------
1. Extract the contents of this folder to any directory.
2. Double-click `SHA256.exe` to run the application in your terminal.

---

Troubleshooting
---------------
* Error: "VCRUNTIME140.dll" or "MSVCP140.dll" was not found:
  This app requires the Microsoft Visual C++ 2015-2022 Redistributable to be installed on your system. 
  You can download and install it directly from Microsoft using the link below:
  https://aka.ms/vs/17/release/vc_redist.x64.exe

* Error: "Unsupported GPU architecture" or failure to initialize GPU:
  Make sure you are running a supported NVIDIA GPU with the latest graphics drivers installed.

---
Created: 2026-06-30
