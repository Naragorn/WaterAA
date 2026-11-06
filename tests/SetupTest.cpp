// The plugin's device work against a real Direct3D 9 device of this machine.
//
// A small pixel shader carries the game's two wobble numbers and paints the
// first of them: half red as the game has it, full red with the reach held
// from 1000 units. A quad drawn with it into a 4 x 4 target and read back
// says which shader the device really had set.

#include <cstdarg>
#include <string>
#include <vector>

#include "Check.h"
#include "DeviceHook.h"
#include "Setup.h"
#include "TestDevice.h"
#include "TestShaders.h"

using namespace wateraa;

namespace {

std::vector<std::string> g_lines;
void Log(const char* format, ...) {
	char buffer[512];
	va_list args;
	va_start(args, format);
	std::vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);
	g_lines.push_back(buffer);
	std::printf("        log: %s\n", buffer);
}
bool Logged(const char* text) {
	for (const auto& line : g_lines) {
		if (line.find(text) != std::string::npos) {
			return true;
		}
	}
	return false;
}

constexpr UINT kTargetSize = 4;

struct Vertex {
	float x, y, z, rhw;
};

struct Scene {
	IDirect3DDevice9* device = nullptr;
	IDirect3DTexture9* target = nullptr;
	IDirect3DSurface9* targetLevel = nullptr;
	IDirect3DSurface9* readback = nullptr;
	IDirect3DSurface9* back = nullptr;
	IDirect3DVertexBuffer9* quad = nullptr;

	bool Create(IDirect3DDevice9* d) {
		device = d;
		if (FAILED(device->CreateTexture(kTargetSize, kTargetSize, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
		                                 D3DPOOL_DEFAULT, &target, nullptr)) ||
		    FAILED(target->GetSurfaceLevel(0, &targetLevel)) ||
		    FAILED(device->CreateOffscreenPlainSurface(kTargetSize, kTargetSize, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM,
		                                               &readback, nullptr)) ||
		    FAILED(device->GetRenderTarget(0, &back)) ||
		    FAILED(device->CreateVertexBuffer(4 * sizeof(Vertex), 0, D3DFVF_XYZRHW, D3DPOOL_MANAGED, &quad, nullptr))) {
			return false;
		}
		const float edge = static_cast<float>(kTargetSize) - 0.5f;
		const Vertex vertices[4] = {
			{-0.5f, -0.5f, 0.5f, 1.0f}, {edge, -0.5f, 0.5f, 1.0f}, {-0.5f, edge, 0.5f, 1.0f}, {edge, edge, 0.5f, 1.0f}};
		void* data = nullptr;
		if (FAILED(quad->Lock(0, 0, &data, 0))) {
			return false;
		}
		std::memcpy(data, vertices, sizeof(vertices));
		quad->Unlock();
		device->SetFVF(D3DFVF_XYZRHW);
		device->SetStreamSource(0, quad, 0, sizeof(Vertex));
		device->SetRenderState(D3DRS_ZENABLE, FALSE);
		device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		return true;
	}

	// Draws the quad with `shader` and reads the red of the pixel at (1, 1).
	int Draw(IDirect3DPixelShader9* shader) {
		device->SetPixelShader(shader);
		device->SetRenderTarget(0, targetLevel);
		device->Clear(0, nullptr, D3DCLEAR_TARGET, 0xFF00FF00, 1.0f, 0);
		device->BeginScene();
		const HRESULT hr = device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
		device->EndScene();
		device->SetRenderTarget(0, back);
		if (FAILED(hr) || FAILED(device->GetRenderTargetData(targetLevel, readback))) {
			return -1;
		}
		D3DLOCKED_RECT locked{};
		if (FAILED(readback->LockRect(&locked, nullptr, D3DLOCK_READONLY))) {
			return -1;
		}
		const auto* row = static_cast<const BYTE*>(locked.pBits) + locked.Pitch;
		const int red = row[4 + 2];
		readback->UnlockRect();
		return red;
	}

	void Destroy() {
		device->SetPixelShader(nullptr);
		device->SetStreamSource(0, nullptr, 0, 0);
		for (IUnknown* object : {static_cast<IUnknown*>(quad), static_cast<IUnknown*>(back),
		                         static_cast<IUnknown*>(readback), static_cast<IUnknown*>(targetLevel),
		                         static_cast<IUnknown*>(target)}) {
			if (object != nullptr) {
				object->Release();
			}
		}
	}
};

bool IsHalf(int red) { return red == 127 || red == 128; }

IDirect3DPixelShader9* Make(IDirect3DDevice9* device, const uint32_t* tokens) {
	IDirect3DPixelShader9* shader = nullptr;
	device->CreatePixelShader(reinterpret_cast<const DWORD*>(tokens), &shader);
	return shader;
}

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
	Scene scene;
	if (!scene.Create(device)) {
		std::printf("the test's own target could not be made\n");
		return 1;
	}
	// A water shader made before the plugin comes up, as the game's may be.
	IDirect3DPixelShader9* early = Make(device, WobbleShader());
	IDirect3DPixelShader9* plain = Make(device, PlainShader());
	Check(IsHalf(scene.Draw(early)) && IsHalf(scene.Draw(plain)), "without the plugin both shaders paint half red");

	Options options;
	options.calmFrom = 1000.0f;

	std::printf("SetupDevice\n");
	Check(SetupDevice(device, options, &Log) && AreDeviceHooksInstalled() &&
	          Logged("the water's reflection wobble is held from 1000 units on"),
	      "the hooks go in");
	Check(!SetupDevice(device, options, &Log) && Logged("Hooks not installed: hooks are already installed"),
	      "a second setup is refused and says so");
	ShutdownDevice();
	Check(!AreDeviceHooksInstalled(), "shut down");
	Check(SetupDevice(device, options, nullptr), "set up without a log");
	ShutdownDevice();
	Check(SetupDevice(device, options, &Log), "and set up again");

	std::printf("a water shader made before the plugin came up\n");
	const int calmed = scene.Draw(early);
	std::printf("        red %d\n", calmed);
	Check(calmed == 255, "its twin is drawn: the reach held, full red");
	Check(Set(device) != early && Set(device) != nullptr, "the twin is what the device has set");
	Check(CurrentStats().calmShaders == 1 && CurrentStats().calmSets == 1 && Logged("mirrors: the reflection's wobble"),
	      "one twin made, set once, and the log says so");
	Check(scene.Draw(early) == 255 && CurrentStats().calmShaders == 1 && CurrentStats().calmSets == 2,
	      "set again: the same twin");

	std::printf("a water shader made after the plugin came up\n");
	IDirect3DPixelShader9* late = Make(device, WobbleShader());
	Check(CurrentStats().calmShaders == 2, "classed at its creation, one twin - the twin itself is not classed");
	Check(scene.Draw(late) == 255, "its twin is drawn");

	std::printf("what is left alone\n");
	Check(IsHalf(scene.Draw(plain)) && Set(device) == plain, "a shader without the pair is set as asked");
	IDirect3DPixelShader9* plainLate = Make(device, PlainShader());
	Check(IsHalf(scene.Draw(plainLate)) && Set(device) == plainLate && CurrentStats().calmShaders == 2,
	      "as is one made after the plugin came up");
	device->SetPixelShader(nullptr);
	Check(Set(device) == nullptr, "no shader stays no shader");
	const uint32_t setsBefore = CurrentStats().calmSets;
	scene.Draw(plain);
	Check(CurrentStats().calmSets == setsBefore, "none of that counts as a calmed set");

	std::printf("a shader's address used again\n");
	// The late water shader is released; whatever is created at its address
	// next is classed by its own bytecode.
	late->Release();
	IDirect3DPixelShader9* reused[8] = {};
	bool sameAddress = false;
	for (auto& shader : reused) {
		shader = Make(device, PlainShader());
		sameAddress = sameAddress || shader == late;
	}
	std::printf("        the released shader's address was %s\n", sameAddress ? "used again" : "not used again");
	bool allPlain = true;
	for (auto* shader : reused) {
		allPlain = allPlain && IsHalf(scene.Draw(shader)) && Set(device) == shader;
	}
	Check(allPlain, "new shaders without the pair are set as asked, whatever stood at their address");
	for (auto* shader : reused) {
		shader->Release();
	}

	std::printf("more water shaders than twins are kept\n");
	IDirect3DPixelShader9* many[40] = {};
	for (auto& shader : many) {
		shader = Make(device, WobbleShader());
	}
	int said = 0;
	for (const auto& line : g_lines) {
		said += line.find("More than 32 water shaders") != std::string::npos ? 1 : 0;
	}
	Check(said == 1, "32 twins are kept at a time, and the log says so once");
	int calmedCount = 0, plainCount = 0;
	for (auto* shader : many) {
		const int red = scene.Draw(shader);
		calmedCount += red == 255 ? 1 : 0;
		plainCount += IsHalf(red) ? 1 : 0;
	}
	std::printf("        %d drawn calmed, %d as they are\n", calmedCount, plainCount);
	Check(calmedCount + plainCount == 40 && plainCount >= 8 && calmedCount >= 30,
	      "the ones past the limit are drawn as the game has them");
	for (auto* shader : many) {
		shader->Release();
	}

	std::printf("a distance outside the range\n");
	ShutdownDevice();
	options.calmFrom = 5000.0f;
	SetupDevice(device, options, &Log);
	Check(IsHalf(scene.Draw(early)) && Set(device) == early && CurrentStats().calmShaders == 0,
	      "CalmFrom=5000 is the game's own: no twin");
	ShutdownDevice();
	Check(!AreDeviceHooksInstalled() && IsHalf(scene.Draw(early)), "shut down: the hooks are out, the shader as it was");

	plainLate->Release();
	plain->Release();
	early->Release();
	scene.Destroy();
	test.Destroy();
	return Finish();
}
