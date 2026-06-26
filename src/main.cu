/*
 * main.cu - Application Entry Point
 * Single Responsibility: Wire dependencies and launch the application
 *
 * Part of SHA256 CUDA Hasher Phase 1
 *
 * Build Instructions:
 *   nvcc -o sha256_hasher main.cu app_controller.cu gpu_device_info.cu power_metrics.cu
 *        SHA256.cu input_handler.cpp -lnvml -std=c++17
 *
 * Or with Visual Studio:
 *   Add all .cu, .cpp, .h files to the CUDA project
 *   Link against: cudart.lib, nvml.lib
 */

#include "config.h"
#include "gpu_device_info.h"
#include "power_metrics.h"
#include "input_handler.h"
#include "cmd_ui.h"
#include "app_controller.h"

#include <cstdio>
#include <fstream>
#include <string>

void log_debug(const std::string& msg) {
    std::ofstream f("C:\\Anudeep\\Repo\\IITJ_MTech\\SHA256\\debug_log.txt", std::ios::app);
    if (f.is_open()) {
        f << msg << std::endl;
    }
}

int main(int argc, char* argv[])
{
    // Clear previous log on startup
    {
        std::ofstream f("C:\\Anudeep\\Repo\\IITJ_MTech\\SHA256\\debug_log.txt", std::ios::trunc);
    }

    log_debug("--- Starting SHA256 program ---");

    // --- Step 1: Query GPU Device ---
    log_debug("Step 1: Querying GPU device...");
    GPUDeviceInfo gpuInfo = queryGPUDevice();
    if (!gpuInfo.valid) {
        log_debug("GPU not valid. Error message: " + gpuInfo.errorMessage);
        printf("WARNING: %s\n", gpuInfo.errorMessage.c_str());
        printf("The application will continue but GPU hashing will not be available.\n");
        printf("Press any key to continue...\n");
        getchar();
    } else {
        log_debug("GPU valid: " + gpuInfo.deviceName);
    }

    // --- Step 2: Initialize Power Metrics (NVML) ---
    log_debug("Step 2: Initializing power metrics...");
    PowerMetrics powerMetrics;
    bool nvmlOk = powerMetrics.init();
    log_debug("NVML init status: " + std::to_string(nvmlOk));

    // --- Step 3: Create Input Handler ---
    log_debug("Step 3: Creating input handler...");
    InputHandler inputHandler;

    // --- Step 4: Create UI ---
    log_debug("Step 4: Initializing UI...");
    CmdUI ui;

    // --- Step 5: Create Controller & Run ---
    log_debug("Step 5: Running controller...");
    AppController controller(ui, inputHandler, gpuInfo, powerMetrics);
    controller.run();

    log_debug("Step 6: Shutdown...");
    powerMetrics.shutdown();

    log_debug("--- Program completed normally ---");
    return 0;
}
