/*
 * Copyright (C) 2024 LibreMobileOS Foundation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "CameraProviderExtension.h"
#include <fstream>
#include <string>

#define TORCH_BRIGHTNESS "brightness"

// Sysfs path that controls the physical activation switch for the LED array (master toggle).
#define TOGGLE_SWITCH "/sys/devices/platform/soc/c42d000.qcom,spmi/spmi-0/0-02/c42d000.qcom,spmi:qcom,pm8350c@2:qcom,flash_led@ee00/leds/led:switch_0/brightness"

// Paths to the individual brightness control nodes for the torch LEDs.
static const std::string kTorchLedPaths[] = {
	"/sys/devices/platform/soc/c42d000.qcom,spmi/spmi-0/0-02/c42d000.qcom,spmi:qcom,pm8350c@2:qcom,flash_led@ee00/leds/led:torch_0",
	"/sys/devices/platform/soc/c42d000.qcom,spmi/spmi-0/0-02/c42d000.qcom,spmi:qcom,pm8350c@2:qcom,flash_led@ee00/leds/led:torch_1",
	"/sys/devices/platform/soc/c42d000.qcom,spmi/spmi-0/0-02/c42d000.qcom,spmi:qcom,pm8350c@2:qcom,flash_led@ee00/leds/led:torch_2",
	"/sys/devices/platform/soc/c42d000.qcom,spmi/spmi-0/0-02/c42d000.qcom,spmi:qcom,pm8350c@2:qcom,flash_led@ee00/leds/led:torch_3",
};

/**
 * @brief Write value to a sysfs path, ensuring the file is open.
 *
 * @tparam T The type of the value to write.
 * @param path The full path to the sysfs file.
 * @param value The value to write.
 */
template <typename T>
static void set(const std::string& path, const T& value) {
	std::ofstream file(path);
	if (file.is_open()) {
		file << value;
	}
}

/**
 * @brief Read value from a sysfs path, returning default on failure.
 *
 * @tparam T The type of the value to read.
 * @param path The full path to the sysfs file.
 * @param def The default value to return on failure.
 * @return The value read from the file or the default value.
 */
template <typename T>
static T get(const std::string& path, const T& def) {
	std::ifstream file(path);
	T result;
	if (!file.is_open()) {
		return def;
	}
	file >> result;
	return file.fail() ? def : result;
}

// --- Extension Implementation ---

/**
 * @brief Checks if variable torch strength control is supported.
 */
bool supportsTorchStrengthControlExt() {
	return true;
}

/**
 * @brief Checks if simple on/off torch mode control is supported.
 */
bool supportsSetTorchModeExt() {
	return false;
}

/**
 * @brief Gets the default torch brightness level.
 */
int32_t getTorchDefaultStrengthLevelExt() {
	return 65;
}

/**
 * @brief Gets the maximum supported torch brightness level.
 */
int32_t getTorchMaxStrengthLevelExt() {
	return 300;
}

/**
 * @brief Gets the current torch strength level from hardware.
 */
int32_t getTorchStrengthLevelExt() {
	// Since all LED paths are set to the same value, we read from the first one.
	const std::string node = kTorchLedPaths[0] + "/" + TORCH_BRIGHTNESS;
	return get(node, 0);
}

/**
 * @brief Sets the torch brightness (strength) and enables/disables it.
 *
 * Disables the master switch, sets the brightness on all nodes, then re-enables
 * the master switch if 'enabled' is true.
 *
 * @param torchStrength The desired brightness level.
 * @param enabled True to enable the torch, false to keep the brightness set but switch off.
 */
void setTorchStrengthLevelExt(int32_t torchStrength, bool enabled) {
	// 1. Ensure master switch is temporarily off
	set(TOGGLE_SWITCH, 0);

	// 2. Set the desired brightness on all individual torch LED paths
	for (const auto& path : kTorchLedPaths) {
		const std::string node = path + "/" + TORCH_BRIGHTNESS;
		set(node, torchStrength);
	}

	// 3. Re-enable the master switch if the torch should be on
	if (enabled) {
		set(TOGGLE_SWITCH, 255);
	}
}

/**
 * @brief Toggles the torch on or off using the default strength.
 *
 * @param enabled True to turn on (at default strength), false to turn off.
 */
void setTorchModeExt(bool enabled) {
	int32_t strength = getTorchDefaultStrengthLevelExt();
	// Pass 0 strength if disabled, or the default strength if enabled.
	setTorchStrengthLevelExt(enabled ? strength : 0, enabled);
}
