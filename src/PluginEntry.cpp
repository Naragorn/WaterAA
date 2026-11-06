#include "PluginEntry.h"

namespace wateraa {

namespace {

Settings g_settings;
bool g_handled = false;

}  // namespace

bool Query(const ExtenderInterfaceHead& xse, PluginInfo* info, const Host& host) {
	info->infoVersion = kInfoVersion;
	info->name = kPluginName;
	info->version = kPluginVersion;
	host.resetLog();
	host.log("%s %u - xOBSE %u, Oblivion %08X, editor=%u", kPluginName, kPluginVersion, xse.obseVersion,
	         xse.oblivionVersion, xse.isEditor);
	return true;
}

bool Load(const ExtenderInterfaceHead& xse, const Host& host) {
	if (xse.isEditor != 0) {
		host.log("Editor: nothing to do");
		return true;
	}
	if (xse.oblivionVersion != kOblivionVersion_1_2_416) {
		host.log("Oblivion %08X is not 1.2.416 (%08X): the renderer address is for 1.2.416 only, nothing done",
		         xse.oblivionVersion, kOblivionVersion_1_2_416);
		return true;
	}
	g_settings = Settings{};
	if (!host.readSettings(&g_settings)) {
		host.log("No WaterAA.ini next to the DLL - defaults: Enable=1 CalmFrom=%.0f", static_cast<double>(g_settings.calmFrom));
	}
	if (!g_settings.enable) {
		host.log("Enable=0 in the INI: nothing done");
		return true;
	}
	if (!host.registerListener(&xse)) {
		host.log("xOBSE offered no messaging interface - cannot learn when the game is up, nothing done");
		return true;
	}
	host.log("Waiting for the game to initialize (CalmFrom=%.0f)", static_cast<double>(g_settings.calmFrom));
	return true;
}

void OnMessage(uint32_t messageType, const Host& host) {
	if (messageType != kMessageGameInitialized || g_handled) {
		return;
	}
	g_handled = true;
	void* device = host.findDevice();
	if (device == nullptr) {
		host.log("Game initialized but the renderer has no device - nothing done");
		return;
	}
	host.log("Game initialized, device %p", device);
	host.setupDevice(device, g_settings);
}

const Settings& LoadedSettings() {
	return g_settings;
}

bool GameInitializedHandled() {
	return g_handled;
}

void ResetForTest() {
	g_settings = Settings{};
	g_handled = false;
}

}  // namespace wateraa
