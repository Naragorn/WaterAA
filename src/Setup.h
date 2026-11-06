#pragma once

#include <cstdint>

// The device side: the two hooks' callbacks and the Direct3D work they do.
// The decisions are in WaterShaders.h and ShaderClasses.h; SetupTest runs
// this against a real device.

namespace wateraa {

using LogFn = void (*)(const char* format, ...);

struct Options {
	// [Water] CalmFrom: the distance from which the reflection's wobble stops
	// growing (WaterShaders.h, CalmWobble).
	float calmFrom = 200.0f;
};

// What the plugin did since SetupDevice, for the log and the tests.
struct Stats {
	uint32_t calmShaders = 0;  // water shaders given a calmed twin
	uint32_t twinsRefused = 0; // twins the device would not make
	uint32_t calmSets = 0;     // times a twin was set in its shader's place
};

// Installs the hooks on the engine's device. False (and logged) when they
// cannot be installed.
bool SetupDevice(void* device, const Options& options, LogFn log);

// Releases the calmed twins and removes the hooks.
void ShutdownDevice();

const Stats& CurrentStats();

}  // namespace wateraa
