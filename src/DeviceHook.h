#pragma once

#include <cstddef>
#include <cstdint>

// Hooks on the engine's IDirect3DDevice9 by replacing entries of its method
// table. The engine and every other plugin share that one table, so a hook
// is seen by all callers and sees the hooks installed before it as "the
// original". Slot numbers follow the method order of IDirect3DDevice9 in
// d3d9.h (IUnknown 0-2, then TestCooperativeLevel 3 ... CreatePixelShader
// 106, SetPixelShader 107); DeviceHookTest checks both against a real device.

namespace wateraa {

constexpr uint32_t kSlotCreatePixelShader = 106;
constexpr uint32_t kSlotSetPixelShader = 107;
constexpr uint32_t kSlotLast = kSlotSetPixelShader;
constexpr int kHookedSlots = 2;

struct HookCallbacks {
	// After a successful original CreatePixelShader, with the bytecode.
	void (*onPixelShaderCreated)(void* device, void* shader, const uint32_t* function);
	// Before the original SetPixelShader, with the shader the game asked for
	// (may be null); returns the shader to set in its place (the same one to
	// leave it).
	void* (*filterPixelShader)(void* device, void* shader);
};

// Whether a pointer about to be treated as a method table looks like one at
// the given depth: readable, and every entry a plausible code address.
bool LooksLikeVtable(void* const* vtable, uint32_t entries);

// Replaces the hooked entries. False, with a reason in error, if hooks are
// already installed (installing twice would chain a hook onto itself), the
// device or its table cannot be read, or the table cannot be made writable.
bool InstallDeviceHooks(void* device, const HookCallbacks& callbacks, char* error, size_t errorLength);

// Puts the original pointers back. Safe when nothing is installed.
void RemoveDeviceHooks();

bool AreDeviceHooksInstalled();

}  // namespace wateraa
