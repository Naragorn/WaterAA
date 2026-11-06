#pragma once

#include <cstddef>
#include <cstdint>

// The water's reflection wobble, held from a distance on.
//
// Oblivion's water shaders that mirror (WATER000, 001, 007 and 015.pso, two
// builds of each across the nineteen shader packages) read the reflection at
// the water's place shifted sideways by the wave's normal times a length that
// grows with the distance: 4 + 2496 * saturate(distance * 0.0002) world
// units - half the distance, out to 5000 units. So the wobble keeps its size
// on the screen however far the water is, while the waves themselves shrink
// to less than a pixel row. Far water's reflection is then shifted by tens of
// pixels, row by row at random, and anew with every step of the waves: it
// flickers (README, "How it was found").
//
// CalmWobble rewrites the two numbers where the shader defines them: the
// length grows as before up to `calmFrom` units and stays there, 4 + 0.4992 *
// min(distance, calmFrom), so beyond it the wobble shrinks on the screen as
// the waves do. Nearer than calmFrom nothing changes.
//
// A shader is the water's when it defines each of the two numbers exactly
// once, in `def` constants - of the 10978 shaders in the game's packages only
// those four do (tools/check-shaders.py). Nothing else in it is touched, and
// a shader without the pair is left as it is.

namespace wateraa {

constexpr float kWobbleReach = 5000.0f;  // 1 / 0.0002
constexpr float kWobbleLength = 2496.0f;
// calmFrom outside this range changes nothing: 5000 and more is the game's
// own, and under 50 units the water next to the player would lose its wobble.
constexpr float kCalmFromLeast = 50.0f;
constexpr float kCalmFromMost = 4999.0f;

// A shader's length in tokens, the version token and the end token counted.
// 0 when no end token stands within maxTokens. Comment blocks are stepped
// over whole; an instruction of shader model 2 and up by the length in its
// opcode token, one of model 1 (no length there) token by token - no operand
// token reads as the end token, bit 31 is set in each.
size_t ShaderTokenCount(const uint32_t* tokens, size_t maxTokens);

// Rewrites the pair in `tokens` (count of them, as ShaderTokenCount gives).
// False, and nothing changed, when the shader does not define each number
// exactly once or calmFrom is outside its range.
bool CalmWobble(uint32_t* tokens, size_t count, float calmFrom);

// The longest shader looked at, in tokens: the game's longest pixel shader is
// under 2000.
constexpr size_t kShaderTokensMost = 16384;

}  // namespace wateraa
