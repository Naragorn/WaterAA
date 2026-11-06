// Checks ShaderClassMap: unknown until classed, found again, overwritten by a
// new class at the same address, null never kept, addresses that share a
// start entry, and the table emptied when it is three quarters full.

#include "Check.h"
#include "ShaderClasses.h"

using namespace wateraa;

int main() {
	std::printf("ShaderClassMap\n");
	ShaderClassMap<8> map;
	char objects[1024] = {};
	const void* a = &objects[0];
	const void* b = &objects[16];
	Check(map.Find(a) == ShaderClass::Unknown, "unknown until classed");
	Check(map.Find(nullptr) == ShaderClass::Unknown, "null is unknown");
	map.Put(a, ShaderClass::Calmed);
	map.Put(b, ShaderClass::Other);
	Check(map.Find(a) == ShaderClass::Calmed && map.Find(b) == ShaderClass::Other && map.Count() == 2,
	      "classed and found again");
	map.Put(a, ShaderClass::Other);
	Check(map.Find(a) == ShaderClass::Other && map.Count() == 2, "a new class at the same address overwrites");
	map.Put(nullptr, ShaderClass::Calmed);
	Check(map.Count() == 2 && map.Find(nullptr) == ShaderClass::Unknown, "null is not kept");
	// Addresses 8 * 16 bytes apart share a start entry in a table of 8.
	const void* c = &objects[128];
	const void* d = &objects[256];
	map.Put(c, ShaderClass::Calmed);
	map.Put(d, ShaderClass::Calmed);
	Check(map.Find(c) == ShaderClass::Calmed && map.Find(d) == ShaderClass::Calmed && map.Find(a) == ShaderClass::Other,
	      "addresses with the same start entry are told apart");
	Check(map.Find(&objects[384]) == ShaderClass::Unknown, "one more of them, not classed, is unknown");
	map.Put(&objects[32], ShaderClass::Other);
	map.Put(&objects[48], ShaderClass::Other);
	Check(map.Count() == 6, "six of eight kept");
	map.Put(a, ShaderClass::Calmed);
	Check(map.Count() == 6 && map.Find(a) == ShaderClass::Calmed, "a known address is still overwritten when full");
	map.Put(&objects[64], ShaderClass::Calmed);
	Check(map.Count() == 1 && map.Find(&objects[64]) == ShaderClass::Calmed && map.Find(a) == ShaderClass::Unknown,
	      "three quarters full: emptied, the new one kept");
	map.Clear();
	Check(map.Count() == 0 && map.Find(&objects[64]) == ShaderClass::Unknown, "cleared");
	return Finish();
}
