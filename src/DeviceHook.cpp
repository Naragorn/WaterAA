#include "DeviceHook.h"

#include <windows.h>

#include <cstdio>

namespace wateraa {

namespace {

using CreatePixelShaderFn = HRESULT(__stdcall*)(void* self, const uint32_t* function, void** shader);
using SetPixelShaderFn = HRESULT(__stdcall*)(void* self, void* shader);

void** g_vtable = nullptr;
HookCallbacks g_callbacks{};

CreatePixelShaderFn g_originalCreatePixelShader = nullptr;
SetPixelShaderFn g_originalSetPixelShader = nullptr;

HRESULT __stdcall HookedCreatePixelShader(void* self, const uint32_t* function, void** shader) {
	const HRESULT hr = g_originalCreatePixelShader(self, function, shader);
	if (SUCCEEDED(hr) && shader != nullptr && *shader != nullptr && g_callbacks.onPixelShaderCreated != nullptr) {
		g_callbacks.onPixelShaderCreated(self, *shader, function);
	}
	return hr;
}

HRESULT __stdcall HookedSetPixelShader(void* self, void* shader) {
	if (g_callbacks.filterPixelShader != nullptr) {
		shader = g_callbacks.filterPixelShader(self, shader);
	}
	return g_originalSetPixelShader(self, shader);
}

bool WriteEntry(void** vtable, uint32_t slot, void* value) {
	void** entry = vtable + slot;
	DWORD previous = 0;
	if (!VirtualProtect(entry, sizeof(void*), PAGE_READWRITE, &previous)) {
		return false;
	}
	*entry = value;
	DWORD ignored = 0;
	VirtualProtect(entry, sizeof(void*), previous, &ignored);
	return true;
}

bool IsReadable(const void* address, size_t bytes) {
	MEMORY_BASIC_INFORMATION info{};
	if (VirtualQuery(address, &info, sizeof(info)) == 0 || info.State != MEM_COMMIT) {
		return false;
	}
	const DWORD readable = PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
	                       PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY;
	if ((info.Protect & readable) == 0 || (info.Protect & PAGE_GUARD) != 0) {
		return false;
	}
	const auto* end = static_cast<const unsigned char*>(info.BaseAddress) + info.RegionSize;
	return static_cast<const unsigned char*>(address) + bytes <= end;
}

struct Slot {
	uint32_t index;
	void* hook;
	void** original;  // where the entry found in the table is kept
};

const Slot kSlots[kHookedSlots] = {
	{kSlotCreatePixelShader, reinterpret_cast<void*>(&HookedCreatePixelShader), reinterpret_cast<void**>(&g_originalCreatePixelShader)},
	{kSlotSetPixelShader, reinterpret_cast<void*>(&HookedSetPixelShader), reinterpret_cast<void**>(&g_originalSetPixelShader)},
};

}  // namespace

bool LooksLikeVtable(void* const* vtable, uint32_t entries) {
	if (vtable == nullptr || entries == 0 || !IsReadable(vtable, entries * sizeof(void*))) {
		return false;
	}
	for (uint32_t i = 0; i < entries; ++i) {
		// Code lives well above the first 64 KiB, which Windows never maps.
		if (reinterpret_cast<uintptr_t>(vtable[i]) < 0x10000) {
			return false;
		}
	}
	return true;
}

bool InstallDeviceHooks(void* device, const HookCallbacks& callbacks, char* error, size_t errorLength) {
	if (g_vtable != nullptr) {
		std::snprintf(error, errorLength, "hooks are already installed");
		return false;
	}
	if (device == nullptr || !IsReadable(device, sizeof(void*))) {
		std::snprintf(error, errorLength, "device pointer %p is not readable", device);
		return false;
	}
	void** vtable = *static_cast<void***>(device);
	if (!LooksLikeVtable(vtable, kSlotLast + 1)) {
		std::snprintf(error, errorLength, "device table %p does not look like a method table", static_cast<void*>(vtable));
		return false;
	}
	for (const Slot& slot : kSlots) {
		*slot.original = vtable[slot.index];
	}
	for (int i = 0; i < kHookedSlots; ++i) {
		if (!WriteEntry(vtable, kSlots[i].index, kSlots[i].hook)) {
			for (int j = 0; j < i; ++j) {
				WriteEntry(vtable, kSlots[j].index, *kSlots[j].original);
			}
			for (const Slot& slot : kSlots) {
				*slot.original = nullptr;
			}
			std::snprintf(error, errorLength, "could not make the method table writable (error %lu)", GetLastError());
			return false;
		}
	}
	g_callbacks = callbacks;
	g_vtable = vtable;
	return true;
}

void RemoveDeviceHooks() {
	if (g_vtable == nullptr) {
		return;
	}
	for (const Slot& slot : kSlots) {
		WriteEntry(g_vtable, slot.index, *slot.original);
		*slot.original = nullptr;
	}
	g_vtable = nullptr;
	g_callbacks = HookCallbacks{};
}

bool AreDeviceHooksInstalled() {
	return g_vtable != nullptr;
}

}  // namespace wateraa
