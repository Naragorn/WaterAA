// Checks WaterShaders: a shader's length by its tokens (the end token, a
// comment block stepped over, model 2's instruction lengths, model 1 token by
// token, no end within the limit, null) and CalmWobble (the pair in one def
// and in two, every way the pair is not there, the range of the distance).

#include <cstring>

#include "Check.h"
#include "WaterShaders.h"

using namespace wateraa;

namespace {

// ps_2_0: dcl t0.xy; dcl_2d s1; texld r0, t0, s1; mov oC0, r0.
const uint32_t kSample1[] = {
	0xFFFF0200, 0x0200001F, 0x80000000, 0xB0030000, 0x0200001F, 0x90000000, 0xA00F0801,
	0x03000042, 0x800F0000, 0xB0E40000, 0xA0E40801, 0x02000001, 0x800F0800, 0x80E40000, 0x0000FFFF,
};

void TestTokenCount() {
	std::printf("ShaderTokenCount\n");
	Check(ShaderTokenCount(kSample1, 64) == 15, "a model 2 shader: every token to the end token");
	Check(ShaderTokenCount(kSample1, 15) == 15, "the end token as the last one allowed");
	Check(ShaderTokenCount(kSample1, 14) == 0, "no end token within the limit");
	Check(ShaderTokenCount(nullptr, 64) == 0, "no bytecode");
	const uint32_t empty[] = {0xFFFF0200, 0x0000FFFF};
	Check(ShaderTokenCount(empty, 64) == 2, "a shader of the version and the end alone");
	// A comment of three tokens whose words read as end tokens.
	const uint32_t comment[] = {0xFFFF0200, 0x0003FFFE, 0x0000FFFF, 0x0000FFFF, 0x0000FFFF,
	                            0x02000001, 0x800F0800, 0x80E40000, 0x0000FFFF};
	Check(ShaderTokenCount(comment, 64) == 9, "a comment block is stepped over whole");
	// A def whose constant reads as the end token: skipped by the length.
	const uint32_t def[] = {0xFFFF0200, 0x05000051, 0xA00F0000, 0x0000FFFF, 0x0000FFFF, 0x0000FFFF, 0x0000FFFF,
	                        0x0000FFFF};
	Check(ShaderTokenCount(def, 64) == 8, "a model 2 instruction is stepped over by its length");
	// ps_1_1: tex t0; mov r0, t0 - no lengths in the opcode tokens.
	const uint32_t model1[] = {0xFFFF0101, 0x00000042, 0xB00F0000, 0x00000001, 0x800F0000, 0xB0E40000, 0x0000FFFF};
	Check(ShaderTokenCount(model1, 64) == 7, "a model 1 shader token by token");
	const uint32_t overrun[] = {0xFFFF0200, 0x0F000001, 0x0000FFFF};
	Check(ShaderTokenCount(overrun, 3) == 0, "an instruction longer than the limit ends the walk");
}

uint32_t Bits(float value) {
	uint32_t bits = 0;
	std::memcpy(&bits, &value, sizeof(bits));
	return bits;
}
float Float(uint32_t bits) {
	float value = 0;
	std::memcpy(&value, &bits, sizeof(value));
	return value;
}

void TestCalmWobble() {
	std::printf("CalmWobble\n");
	// ps_2_x: def c4 = 0.1, 0.0002, 2496, 4; mov oC0, c4.
	const uint32_t together[] = {0xFFFF0201, 0x05000051, 0xA00F0004, Bits(0.1f), Bits(1.0f / 5000.0f), Bits(2496.0f),
	                             Bits(4.0f), 0x02000001, 0x800F0800, 0xA0E40004, 0x0000FFFF};
	uint32_t code[32];
	std::memcpy(code, together, sizeof(together));
	Check(CalmWobble(code, 11, 600.0f), "the two numbers in one def: rewritten");
	Check(Float(code[4]) == 1.0f / 600.0f && Float(code[5]) == 2496.0f / 5000.0f * 600.0f,
	      "the length grows as before, up to 600 units");
	Check(code[3] == together[3] && code[6] == together[6] && code[9] == together[9] && code[0] == together[0],
	      "nothing else is touched");
	// The two numbers in two defs, a comment between them.
	const uint32_t apart[] = {0xFFFF0201, 0x05000051, 0xA00F0004, Bits(-0.5f), Bits(0.1f), Bits(1.0f / 5000.0f),
	                          Bits(0.25f), 0x0002FFFE, Bits(2496.0f), Bits(1.0f / 5000.0f), 0x05000051, 0xA00F000D,
	                          Bits(2496.0f), Bits(4.0f), Bits(-0.2f), Bits(-0.55f), 0x0000FFFF};
	std::memcpy(code, apart, sizeof(apart));
	Check(CalmWobble(code, 17, 1000.0f) && Float(code[5]) == 0.001f && Float(code[12]) == 2496.0f / 5000.0f * 1000.0f,
	      "the two numbers in two defs: rewritten");
	Check(code[8] == apart[8] && code[9] == apart[9], "the same numbers inside a comment are left");
	std::memcpy(code, together, sizeof(together));
	Check(!CalmWobble(code, 11, 5000.0f) && !CalmWobble(code, 11, 20000.0f) && !CalmWobble(code, 11, 0.0f) &&
	          !CalmWobble(code, 11, -5.0f) && !CalmWobble(code, 11, 49.0f),
	      "a distance outside 50..4999 changes nothing");
	Check(!CalmWobble(code, 11, Float(0x7FC00000)), "nor does one that is not a number");
	Check(std::memcmp(code, together, sizeof(together)) == 0, "the shader is as it was");
	Check(CalmWobble(code, 11, 50.0f), "50 is the least");
	std::memcpy(code, together, sizeof(together));
	Check(CalmWobble(code, 11, 4999.0f), "4999 the most");
	Check(!CalmWobble(nullptr, 11, 600.0f), "no bytecode");
	uint32_t plain[15];
	std::memcpy(plain, kSample1, sizeof(plain));
	Check(!CalmWobble(plain, 15, 600.0f) && std::memcmp(plain, kSample1, sizeof(plain)) == 0,
	      "a shader without the two numbers is left");
	// Only one of the two: left.
	std::memcpy(code, together, sizeof(together));
	code[5] = Bits(2500.0f);
	Check(!CalmWobble(code, 11, 600.0f) && Float(code[4]) == 1.0f / 5000.0f, "the reach without the length: left");
	std::memcpy(code, together, sizeof(together));
	code[4] = Bits(0.0003f);
	Check(!CalmWobble(code, 11, 600.0f) && Float(code[5]) == 2496.0f, "the length without the reach: left");
	// One of them twice: not the pattern.
	std::memcpy(code, together, sizeof(together));
	code[3] = Bits(2496.0f);
	Check(!CalmWobble(code, 11, 600.0f) && Float(code[5]) == 2496.0f, "the length defined twice: left");
	std::memcpy(code, together, sizeof(together));
	code[6] = Bits(1.0f / 5000.0f);
	Check(!CalmWobble(code, 11, 600.0f) && Float(code[4]) == 1.0f / 5000.0f, "the reach defined twice: left");
	// The numbers as an instruction's operands, not in a def: left.
	const uint32_t noDef[] = {0xFFFF0201, 0x05000001, 0xA00F0004, Bits(0.1f), Bits(1.0f / 5000.0f), Bits(2496.0f),
	                          Bits(4.0f), 0x0000FFFF};
	std::memcpy(code, noDef, sizeof(noDef));
	Check(!CalmWobble(code, 8, 600.0f), "the numbers outside a def: left");
	// A def cut off by the end of the buffer is not read past it.
	std::memcpy(code, together, sizeof(together));
	Check(!CalmWobble(code, 6, 600.0f) && Float(code[4]) == 1.0f / 5000.0f, "a def past the buffer's end is not read");
}

}  // namespace

int main() {
	TestCalmWobble();
	TestTokenCount();
	return Finish();
}
