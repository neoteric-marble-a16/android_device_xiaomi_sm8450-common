/*
 * Copyright (C) 2024 LibreMobileOS Foundation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "CameraProviderExtension.h"

#include <fstream>
#include <string>
#include <vector>

#define TORCH_BRIGHTNESS "brightness"
#define TORCH_MAX_BRIGHTNESS "max_brightness"
// TOGGLE_SWITCH path retained from file (7)
#define TOGGLE_SWITCH "/sys/devices/platform/soc/c42d000.qcom,spmi/spmi-0/0-02/c42d000.qcom,spmi:qcom,pm8350c@2:qcom,flash_led@ee00/leds/led:switch_0/brightness"

// Base paths for all four torch LEDs, derived from your device's configuration
static const std::vector<std::string> kTorchLedPaths = {
    "/sys/devices/platform/soc/c42d000.qcom,spmi/spmi-0/0-02/c42d000.qcom,spmi:qcom,pm8350c@2:qcom,flash_led@ee00/leds/led:torch_0",
    "/sys/devices/platform/soc/c42d000.qcom,spmi/spmi-0/0-02/c42d000.qcom,spmi:qcom,pm8350c@2:qcom,flash_led@ee00/leds/led:torch_1",
    "/sys/devices/platform/soc/c42d000.qcom,spmi/spmi-0/0-02/c42d000.qcom,spmi:qcom,pm8350c@2:qcom,flash_led@ee00/leds/led:torch_2",
    "/sys/devices/platform/soc/c42d000.qcom,spmi/spmi-0/0-02/c42d000.qcom,spmi:qcom,pm8350c@2:qcom,flash_led@ee00/leds/led:torch_3",
};

/**
 * Write value to path and close file.
 */
template <typename T>
static void set(const std::string& path, const T& value) {
    std::ofstream file(path);
    if (file.is_open()) {
        file << value;
    }
}

/**
 * Read value from the path and close file.
 */
template <typename T>
static T get(const std::string& path, const T& def) {
    std::ifstream file(path);
    T result;
    if (file.is_open()) {
        file >> result;
        return file.fail() ? def : result;
    }
    return def;
}

bool supportsTorchStrengthControlExt() {
    return true;
}

bool supportsSetTorchModeExt() {
    return false;
}

int32_t getTorchDefaultStrengthLevelExt() {
    return 65;
}

int32_t getTorchMaxStrengthLevelExt() {
    return 300;
}

int32_t getTorchStrengthLevelExt() {
    // Read from the brightness file of the first LED.
    if (kTorchLedPaths.empty()) return 0;
    
    // Append the 'brightness' file name to the path
    auto node = kTorchLedPaths[0] + "/" + TORCH_BRIGHTNESS;
    return get(node, 0);
}

void setTorchStrengthLevelExt(int32_t torchStrength, bool enabled) {
    // 1. Temporarily turn off the master switch to allow brightness changes (from file 7 logic)
    set(TOGGLE_SWITCH, 0);

    // 2. Determine the final strength. If 'enabled' is false, force strength to 0.
    int32_t finalStrength = enabled ? torchStrength : 0;
    
    // 3. Write the strength value to the 'brightness' node for ALL four LEDs.
    for (const auto& path : kTorchLedPaths) {
        auto node = path + "/" + TORCH_BRIGHTNESS; 
        set(node, finalStrength);
    }
    
    // 4. If enabled (and strength > 0, implicitly), turn the master switch back on.
    if (enabled && finalStrength > 0)
        // Note: File 7 used 255 to toggle it ON, we keep that value.
        set(TOGGLE_SWITCH, 255); 
}

void setTorchModeExt(bool enabled) {
    // This is called by CameraProviderManager::setTorchMode(id, enabled).
    // The implementation relies on setTorchStrengthLevelExt to handle both strength and toggle.
    int32_t strength = getTorchDefaultStrengthLevelExt();
    setTorchStrengthLevelExt(enabled ? strength : 0, enabled);
}
