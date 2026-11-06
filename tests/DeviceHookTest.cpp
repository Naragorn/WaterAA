// The method-table hooks against a real Direct3D 9 device of this machine:
// both hooked slot numbers are right (the interface's own methods reach the
// hooks), the originals still run, the filter's answer is what is set, a
// second install is refused, and removal restores the table exactly.

#include <cstring>

#include "Check.h"
#include "DeviceHook.h"
#include "TestDevice.h"
#include "TestShaders.h"

using namespace wateraa;

namespace {

struct Seen {
	int created = 0;
	void* createdShader = nullptr;
	const uint32_t* createdFunction = nullptr;
	int shaders = 0;
	void* shader = nullptr;
	void* device = nullptr;
};
Seen g_seen;

void OnCreated(void* device, void* shader, const uint32_t* function) {
	++g_seen.created;
	g_seen.createdShader = shader;
	g_seen.createdFunction = function;
	g_seen.device = device;
}
void* g_shaderAnswer = nullptr;  // null: answer with what was asked
void* FilterShader(void*, void* shader) {
	++g_seen.shaders;
	g_seen.shader = shader;
	return g_shaderAnswer != nullptr ? g_shaderAnswer : shader;
}

const HookCallbacks kCallbacks{&OnCreated, &FilterShader};

IDirect3DPixelShader9* Set(IDirect3DDevice9* device) {
	IDirect3DPixelShader9* set = nullptr;
	device->GetPixelShader(&set);
	if (set != nullptr) {
		set->Release();
	}
	return set;
}

}  // namespace

int main() {
	TestDevice test;
	if (!test.Create()) {
		std::printf("no Direct3D 9 device on this machine\n");
		return 1;
	}
	IDirect3DDevice9* device = test.device;
	void** vtable = *reinterpret_cast<void***>(device);
	void* before[kSlotLast + 1];
	std::memcpy(before, vtable, sizeof(before));
	char error[160] = {};

	std::printf("InstallDeviceHooks\n");
	Check(LooksLikeVtable(vtable, kSlotLast + 1), "the device's table looks like a method table");
	Check(!LooksLikeVtable(nullptr, 4) && !LooksLikeVtable(vtable, 0), "null and no entries do not");
	void* low[2] = {reinterpret_cast<void*>(0x1000), reinterpret_cast<void*>(0x1000)};
	Check(!LooksLikeVtable(low, 2), "entries in the first 64 KiB do not");
	Check(!InstallDeviceHooks(nullptr, kCallbacks, error, sizeof(error)) && std::strstr(error, "not readable") != nullptr,
	      "without a device the install is refused");
	void* fake[1] = {low};
	Check(!InstallDeviceHooks(fake, kCallbacks, error, sizeof(error)) && std::strstr(error, "method table") != nullptr,
	      "an object without a method table is refused");
	Check(!AreDeviceHooksInstalled(), "nothing installed by the refusals");
	Check(InstallDeviceHooks(device, kCallbacks, error, sizeof(error)) && AreDeviceHooksInstalled(), "installed");
	Check(!InstallDeviceHooks(device, kCallbacks, error, sizeof(error)) && std::strstr(error, "already") != nullptr,
	      "a second install is refused");

	std::printf("the interface's methods reach the hooks\n");
	IDirect3DPixelShader9* shader = nullptr;
	Check(SUCCEEDED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(WobbleShader()), &shader)) &&
	          g_seen.created == 1 && g_seen.createdShader == shader && g_seen.createdFunction == WobbleShader() &&
	          g_seen.device == device,
	      "CreatePixelShader (slot 106), with the bytecode");
	const DWORD broken[] = {0x12345678, 0x0000FFFF};
	IDirect3DPixelShader9* none = nullptr;
	Check(FAILED(device->CreatePixelShader(broken, &none)) && g_seen.created == 1, "a refused CreatePixelShader is not told");
	Check(SUCCEEDED(device->SetPixelShader(shader)) && g_seen.shaders == 1 && g_seen.shader == shader &&
	          Set(device) == shader,
	      "SetPixelShader (slot 107): the filter leaves it, the shader is set");
	IDirect3DPixelShader9* other = nullptr;
	device->CreatePixelShader(reinterpret_cast<const DWORD*>(PlainShader()), &other);
	g_shaderAnswer = other;
	device->SetPixelShader(shader);
	Check(g_seen.shader == shader && Set(device) == other, "the filter's answer is the shader set");
	g_shaderAnswer = nullptr;
	device->SetPixelShader(nullptr);
	Check(g_seen.shaders == 3 && g_seen.shader == nullptr && Set(device) == nullptr, "SetPixelShader with no shader");

	std::printf("RemoveDeviceHooks\n");
	RemoveDeviceHooks();
	Check(!AreDeviceHooksInstalled() && std::memcmp(before, vtable, sizeof(before)) == 0, "the table is as it was");
	device->SetPixelShader(shader);
	Check(g_seen.shaders == 3 && Set(device) == shader, "and nothing is told any more");
	RemoveDeviceHooks();
	Check(!AreDeviceHooksInstalled(), "removing twice is harmless");

	device->SetPixelShader(nullptr);
	other->Release();
	shader->Release();
	test.Destroy();
	return Finish();
}
