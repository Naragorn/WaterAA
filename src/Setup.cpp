#include "Setup.h"

#include "DeviceHook.h"
#include "ShaderClasses.h"
#include "WaterShaders.h"

#include <d3d9.h>
#include <windows.h>

namespace wateraa {

namespace {

// The game makes a few hundred pixel shaders.
constexpr size_t kShaderClassEntries = 4096;
// Four of the game's water shaders mirror; room for a mod's own.
constexpr size_t kTwinsMost = 32;

// A water shader and its twin with the wobble held (CalmWobble).
struct Twin {
	const void* original = nullptr;
	IDirect3DPixelShader9* calm = nullptr;
};

struct State {
	Options options{};
	LogFn log = nullptr;
	Stats stats{};
	ShaderClassMap<kShaderClassEntries> shaders;
	Twin twins[kTwinsMost];
	size_t twinCount = 0;
	bool makingTwin = false;
	bool saidFull = false;
};

State g;

void NoLog(const char*, ...) {}

void* TwinOf(const void* original) {
	for (size_t i = 0; i < g.twinCount; ++i) {
		if (g.twins[i].original == original) {
			return g.twins[i].calm;
		}
	}
	return nullptr;
}

void ForgetTwin(const void* original) {
	for (size_t i = 0; i < g.twinCount; ++i) {
		if (g.twins[i].original == original) {
			g.twins[i].calm->Release();
			g.twins[i] = g.twins[--g.twinCount];
			g.twins[g.twinCount] = Twin{};
			return;
		}
	}
}

void ReleaseTwins() {
	for (size_t i = 0; i < g.twinCount; ++i) {
		g.twins[i].calm->Release();
		g.twins[i] = Twin{};
	}
	g.twinCount = 0;
}

// The class of a shader by its bytecode: one that mirrors gets its twin.
ShaderClass ClassOf(void* device, const void* shader, const uint32_t* function) {
	static uint32_t tokens[kShaderTokensMost];
	const size_t count = ShaderTokenCount(function, kShaderTokensMost);
	if (count == 0) {
		return ShaderClass::Other;
	}
	for (size_t i = 0; i < count; ++i) {
		tokens[i] = function[i];
	}
	if (!CalmWobble(tokens, count, g.options.calmFrom)) {
		return ShaderClass::Other;
	}
	if (g.twinCount == kTwinsMost) {
		if (!g.saidFull) {
			g.saidFull = true;
			g.log("More than %u water shaders that mirror - the rest is drawn as the game has it",
			      static_cast<unsigned>(kTwinsMost));
		}
		return ShaderClass::Other;
	}
	IDirect3DPixelShader9* calm = nullptr;
	g.makingTwin = true;
	const HRESULT hr =
		static_cast<IDirect3DDevice9*>(device)->CreatePixelShader(reinterpret_cast<const DWORD*>(tokens), &calm);
	g.makingTwin = false;
	if (FAILED(hr) || calm == nullptr) {
		++g.stats.twinsRefused;
		g.log("Water shader %p (%u tokens): its calmed twin was refused (0x%08lX) - drawn as the game has it", shader,
		      static_cast<unsigned>(count), hr);
		return ShaderClass::Other;
	}
	g.twins[g.twinCount++] = {shader, calm};
	++g.stats.calmShaders;
	g.log("Water shader %p (%u tokens) mirrors: the reflection's wobble is held from %.0f units on", shader,
	      static_cast<unsigned>(count), static_cast<double>(g.options.calmFrom));
	return ShaderClass::Calmed;
}

// A shader made before the plugin came up: classed by the bytecode the
// shader object hands back.
ShaderClass ClassOfExisting(void* device, void* shader) {
	static uint32_t tokens[kShaderTokensMost];
	UINT bytes = 0;
	auto* object = static_cast<IDirect3DPixelShader9*>(shader);
	if (FAILED(object->GetFunction(nullptr, &bytes)) || bytes == 0 || bytes > sizeof(tokens) ||
	    FAILED(object->GetFunction(tokens, &bytes))) {
		return ShaderClass::Other;
	}
	return ShaderTokenCount(tokens, bytes / 4) == 0 ? ShaderClass::Other : ClassOf(device, shader, tokens);
}

void OnPixelShaderCreated(void* device, void* shader, const uint32_t* function) {
	if (g.makingTwin) {
		return;  // the plugin's own twin
	}
	// A shader object's address is free again once the shader is released: a
	// new shader at a known address is whatever its own bytecode says.
	ForgetTwin(shader);
	g.shaders.Put(shader, ClassOf(device, shader, function));
}

void* OnPixelShader(void* device, void* shader) {
	ShaderClass shaderClass = g.shaders.Find(shader);
	if (shaderClass == ShaderClass::Unknown && shader != nullptr) {
		// Not seen created, or the table was emptied since: classed anew (a
		// twin it had is made again).
		ForgetTwin(shader);
		shaderClass = ClassOfExisting(device, shader);
		g.shaders.Put(shader, shaderClass);
	}
	if (shaderClass != ShaderClass::Calmed) {
		return shader;
	}
	void* calm = TwinOf(shader);
	if (calm == nullptr) {
		return shader;
	}
	++g.stats.calmSets;
	return calm;
}

}  // namespace

bool SetupDevice(void* device, const Options& options, LogFn log) {
	g = State{};
	g.options = options;
	g.log = log != nullptr ? log : &NoLog;
	HookCallbacks callbacks{};
	callbacks.onPixelShaderCreated = &OnPixelShaderCreated;
	callbacks.filterPixelShader = &OnPixelShader;
	char error[160] = {};
	if (!InstallDeviceHooks(device, callbacks, error, sizeof(error))) {
		g.log("Hooks not installed: %s - nothing done", error);
		return false;
	}
	g.log("Hooks installed on device %p: the water's reflection wobble is held from %.0f units on", device,
	      static_cast<double>(options.calmFrom));
	return true;
}

void ShutdownDevice() {
	ReleaseTwins();
	RemoveDeviceHooks();
}

const Stats& CurrentStats() {
	return g.stats;
}

}  // namespace wateraa
