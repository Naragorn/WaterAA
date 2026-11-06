// Every flow of Query, Load and OnMessage, with the side effects replaced by
// fakes.

#include <cstdarg>
#include <string>
#include <vector>

#include "Check.h"
#include "PluginEntry.h"

using namespace wateraa;

namespace {

struct Fake {
	int resets = 0;
	bool iniExists = true;
	Settings iniSettings;
	bool registerResult = true;
	int registrations = 0;
	void* device = nullptr;
	int finds = 0;
	int setups = 0;
	Settings setupSettings;
	void* setupDevice = nullptr;
	std::vector<std::string> lines;

	bool Logged(const char* text) const {
		for (const auto& line : lines) {
			if (line.find(text) != std::string::npos) {
				return true;
			}
		}
		return false;
	}
};
Fake* g_fake = nullptr;

void FakeReset() { ++g_fake->resets; }
void FakeLog(const char* format, ...) {
	char buffer[512];
	va_list args;
	va_start(args, format);
	std::vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);
	g_fake->lines.push_back(buffer);
}
bool FakeReadSettings(Settings* settings) {
	if (!g_fake->iniExists) {
		return false;
	}
	*settings = g_fake->iniSettings;
	return true;
}
bool FakeRegister(const ExtenderInterfaceHead*) {
	++g_fake->registrations;
	return g_fake->registerResult;
}
void* FakeFindDevice() {
	++g_fake->finds;
	return g_fake->device;
}
void FakeSetup(void* device, const Settings& settings) {
	++g_fake->setups;
	g_fake->setupDevice = device;
	g_fake->setupSettings = settings;
}

const Host kHost{&FakeReset, &FakeLog, &FakeReadSettings, &FakeRegister, &FakeFindDevice, &FakeSetup};
const ExtenderInterfaceHead kGame{22, kOblivionVersion_1_2_416, 0, 0};

Fake Fresh() {
	ResetForTest();
	return Fake{};
}

void TestQuery() {
	std::printf("Query\n");
	Fake fake = Fresh();
	g_fake = &fake;
	PluginInfo info{};
	Check(Query(kGame, &info, kHost), "the query is answered");
	Check(info.infoVersion == kInfoVersion && std::string(info.name) == "WaterAA" && info.version == kPluginVersion,
	      "name and versions filled in");
	Check(fake.resets == 1 && fake.Logged("WaterAA 1 - xOBSE 22"), "the log is started with the versions");
}

void TestLoad() {
	std::printf("Load\n");
	{
		Fake fake = Fresh();
		g_fake = &fake;
		const ExtenderInterfaceHead editor{22, kOblivionVersion_1_2_416, 1, 1};
		Check(Load(editor, kHost) && fake.registrations == 0 && fake.Logged("Editor"), "the editor: nothing registered");
	}
	{
		Fake fake = Fresh();
		g_fake = &fake;
		const ExtenderInterfaceHead other{22, 0x010201A1, 0, 0};
		Check(Load(other, kHost) && fake.registrations == 0 && fake.Logged("is not 1.2.416"),
		      "another game build: nothing registered");
	}
	{
		Fake fake = Fresh();
		g_fake = &fake;
		fake.iniExists = false;
		Check(Load(kGame, kHost) && fake.registrations == 1 && fake.Logged("No WaterAA.ini"),
		      "no INI: the defaults, registered");
		Check(LoadedSettings().enable && LoadedSettings().calmFrom == 200.0f && fake.Logged("CalmFrom=200"),
		      "the defaults: on, calm from 200 units");
	}
	{
		Fake fake = Fresh();
		g_fake = &fake;
		fake.iniSettings.enable = false;
		Check(Load(kGame, kHost) && fake.registrations == 0 && fake.Logged("Enable=0"), "Enable=0: nothing registered");
	}
	{
		Fake fake = Fresh();
		g_fake = &fake;
		fake.registerResult = false;
		Check(Load(kGame, kHost) && fake.registrations == 1 && fake.Logged("no messaging interface"),
		      "no messaging interface: said, still loaded");
	}
	{
		Fake fake = Fresh();
		g_fake = &fake;
		fake.iniSettings.calmFrom = 350.0f;
		Check(Load(kGame, kHost) && fake.Logged("(CalmFrom=350)") && LoadedSettings().calmFrom == 350.0f,
		      "the INI's settings are kept and logged");
	}
}

void TestMessage() {
	std::printf("OnMessage\n");
	{
		Fake fake = Fresh();
		g_fake = &fake;
		OnMessage(kMessageGameInitialized - 1, kHost);
		Check(fake.finds == 0 && !GameInitializedHandled(), "another message is ignored");
	}
	{
		Fake fake = Fresh();
		g_fake = &fake;
		OnMessage(kMessageGameInitialized, kHost);
		Check(fake.finds == 1 && fake.setups == 0 && fake.Logged("has no device") && GameInitializedHandled(),
		      "no device: said, nothing set up");
		OnMessage(kMessageGameInitialized, kHost);
		Check(fake.finds == 1, "and not asked again");
	}
	{
		Fake fake = Fresh();
		g_fake = &fake;
		fake.iniSettings.calmFrom = 400.0f;
		Load(kGame, kHost);
		int device = 0;
		fake.device = &device;
		OnMessage(kMessageGameInitialized, kHost);
		Check(fake.setups == 1 && fake.setupDevice == &device && fake.setupSettings.calmFrom == 400.0f,
		      "the device is set up with the settings Load read");
		OnMessage(kMessageGameInitialized, kHost);
		Check(fake.setups == 1, "a repeat is ignored");
	}
}

}  // namespace

int main() {
	TestQuery();
	TestLoad();
	TestMessage();
	return Finish();
}
