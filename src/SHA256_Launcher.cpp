#include <windows.h>
#include <string>
#include <vector>
#include <cwchar>
#include <shellapi.h>

// CUDA Driver API typedefs and constants
typedef int(*PFN_cuInit)(unsigned int Flags);
typedef int(*PFN_cuDeviceGetCount)(int* count);
typedef int(*PFN_cuDeviceGet)(int* device, int ordinal);
typedef int(*PFN_cuDeviceGetAttribute)(int* pi, int attrib, int dev);

#define CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR 75
#define CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR 76

// Checks if we should run the Modern version (CUDA 13.3, CC >= 7.5)
// Returns true for Modern, false for Legacy
bool DetectGpuCapability(bool debugMode) {
    HMODULE hCUDA = LoadLibraryA("nvcuda.dll");
    if (!hCUDA) {
        if (debugMode) {
            fwprintf(stderr, L"[Launcher] nvcuda.dll not found. Falling back to Legacy.\n");
        }
        return false;
    }

    PFN_cuInit cuInit = (PFN_cuInit)GetProcAddress(hCUDA, "cuInit");
    PFN_cuDeviceGetCount cuDeviceGetCount = (PFN_cuDeviceGetCount)GetProcAddress(hCUDA, "cuDeviceGetCount");
    PFN_cuDeviceGet cuDeviceGet = (PFN_cuDeviceGet)GetProcAddress(hCUDA, "cuDeviceGet");
    PFN_cuDeviceGetAttribute cuDeviceGetAttribute = (PFN_cuDeviceGetAttribute)GetProcAddress(hCUDA, "cuDeviceGetAttribute");

    if (!cuInit || !cuDeviceGetCount || !cuDeviceGet || !cuDeviceGetAttribute) {
        if (debugMode) {
            fwprintf(stderr, L"[Launcher] Failed to resolve nvcuda.dll symbols. Falling back to Legacy.\n");
        }
        FreeLibrary(hCUDA);
        return false;
    }

    if (cuInit(0) != 0) {
        if (debugMode) {
            fwprintf(stderr, L"[Launcher] cuInit failed. Falling back to Legacy.\n");
        }
        FreeLibrary(hCUDA);
        return false;
    }

    int deviceCount = 0;
    if (cuDeviceGetCount(&deviceCount) != 0 || deviceCount <= 0) {
        if (debugMode) {
            fwprintf(stderr, L"[Launcher] No CUDA-capable devices found. Falling back to Legacy.\n");
        }
        FreeLibrary(hCUDA);
        return false;
    }

    int maxMajor = 0;
    int maxMinor = 0;

    for (int i = 0; i < deviceCount; ++i) {
        int device = 0;
        if (cuDeviceGet(&device, i) == 0) {
            int major = 0;
            int minor = 0;
            if (cuDeviceGetAttribute(&major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, device) == 0 &&
                cuDeviceGetAttribute(&minor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, device) == 0) {
                if (debugMode) {
                    fwprintf(stderr, L"[Launcher] Device %d: compute capability %d.%d\n", i, major, minor);
                }
                if (major > maxMajor || (major == maxMajor && minor > maxMinor)) {
                    maxMajor = major;
                    maxMinor = minor;
                }
            }
        }
    }

    FreeLibrary(hCUDA);

    if (debugMode) {
        fwprintf(stderr, L"[Launcher] Max GPU Compute Capability detected: %d.%d\n", maxMajor, maxMinor);
    }

    // Modern requires compute capability >= 7.5 (Turing)
    if (maxMajor > 7 || (maxMajor == 7 && maxMinor >= 5)) {
        return true;
    }

    return false;
}

int wmain(int argc, wchar_t* argv[]) {
    // Check if launcher debug environment variable is set
    bool debugMode = false;
    wchar_t envBuf[32] = {0};
    if (GetEnvironmentVariableW(L"SHA256_LAUNCHER_DEBUG", envBuf, 32) > 0) {
        if (wcscmp(envBuf, L"1") == 0) {
            debugMode = true;
        }
    }

    bool runModern = DetectGpuCapability(debugMode);
    std::wstring selectedExe = runModern ? L"SHA256_Modern.exe" : L"SHA256_Legacy.exe";

    if (debugMode) {
        fwprintf(stderr, L"[Launcher] Selected: %s\n", selectedExe.c_str());
    }

    // Reconstruct the command line with the new executable name as the first argument
    std::wstring cmdLine = L"\"" + selectedExe + L"\"";
    for (int i = 1; i < argc; i++) {
        cmdLine += L" \"";
        std::wstring arg = argv[i];
        size_t pos = 0;
        while ((pos = arg.find(L"\"", pos)) != std::wstring::npos) {
            arg.replace(pos, 1, L"\\\"");
            pos += 2;
        }
        cmdLine += arg;
        cmdLine += L"\"";
    }

    if (debugMode) {
        fwprintf(stderr, L"[Launcher] Reconstructed CommandLine: %s\n", cmdLine.c_str());
    }

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (CreateProcessW(
        NULL,
        &cmdLine[0],
        NULL,
        NULL,
        FALSE,
        0,
        NULL,
        NULL,
        &si,
        &pi
    )) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return (int)exitCode;
    } else {
        fwprintf(stderr, L"Error: Failed to launch %s. Make sure it is in the same directory as the launcher.\n", selectedExe.c_str());
        return 1;
    }
}
