#pragma once

#include <cstdint>
#include <cstring>

// Small pixel shaders for the device tests, written out as tokens (the tests
// carry none of the game's bytecode).

inline uint32_t FloatBits(float value) {
	uint32_t bits = 0;
	std::memcpy(&bits, &value, sizeof(bits));
	return bits;
}

// ps_2_0: def c4 = 0.1, 0.0002, 2496, 4; def c6 = 2500, 0, 0, 1;
// mul r0, c4.y, c6.x; mov oC0, r0 - the game's two wobble numbers, and a
// picture of the first: 0.0002 * 2500 is half red, a reach held from 1000
// units (0.001 * 2500) is full red.
inline const uint32_t* WobbleShader() {
	static const uint32_t tokens[] = {
		0xFFFF0200, 0x05000051, 0xA00F0004, FloatBits(0.1f), FloatBits(1.0f / 5000.0f), FloatBits(2496.0f),
		FloatBits(4.0f), 0x05000051, 0xA00F0006, FloatBits(2500.0f), FloatBits(0.0f), FloatBits(0.0f),
		FloatBits(1.0f), 0x03000005, 0x800F0000, 0xA0550004, 0xA0000006, 0x02000001, 0x800F0800, 0x80E40000,
		0x0000FFFF,
	};
	return tokens;
}

// The same picture without the second number (2000 in place of 2496): not
// the water's pair, so not a shader the plugin touches.
inline const uint32_t* PlainShader() {
	static const uint32_t tokens[] = {
		0xFFFF0200, 0x05000051, 0xA00F0004, FloatBits(0.1f), FloatBits(1.0f / 5000.0f), FloatBits(2000.0f),
		FloatBits(4.0f), 0x05000051, 0xA00F0006, FloatBits(2500.0f), FloatBits(0.0f), FloatBits(0.0f),
		FloatBits(1.0f), 0x03000005, 0x800F0000, 0xA0550004, 0xA0000006, 0x02000001, 0x800F0800, 0x80E40000,
		0x0000FFFF,
	};
	return tokens;
}
