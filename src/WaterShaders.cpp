#include "WaterShaders.h"

#include <cstring>

namespace wateraa {

namespace {

constexpr uint32_t kEndToken = 0x0000FFFF;
constexpr uint32_t kCommentOpcode = 0xFFFE;
constexpr uint32_t kDefOpcode = 0x51;

uint32_t Bits(float value) {
	uint32_t bits = 0;
	std::memcpy(&bits, &value, sizeof(bits));
	return bits;
}

// Calls visit(token index) for each of the four values of every `def`.
template <typename Visit>
void ForEachDefValue(const uint32_t* tokens, size_t count, Visit visit) {
	size_t i = 1;
	while (i < count && tokens[i] != kEndToken) {
		const uint32_t token = tokens[i];
		if ((token & 0xFFFF) == kCommentOpcode) {
			i += 1 + ((token >> 16) & 0x7FFF);
			continue;
		}
		const size_t length = (token >> 24) & 0xF;
		if ((token & 0xFFFF) == kDefOpcode && length == 5 && i + 5 < count) {
			for (size_t value = i + 2; value <= i + 5; ++value) {
				visit(value);
			}
		}
		i += 1 + length;
	}
}

}  // namespace

size_t ShaderTokenCount(const uint32_t* tokens, size_t maxTokens) {
	if (tokens == nullptr) {
		return 0;
	}
	size_t i = 1;  // past the version token
	while (i < maxTokens) {
		const uint32_t token = tokens[i];
		if (token == kEndToken) {
			return i + 1;
		}
		if ((token & 0xFFFF) == kCommentOpcode) {
			i += 1 + ((token >> 16) & 0x7FFF);
		} else {
			i += 1 + ((token >> 24) & 0xF);
		}
	}
	return 0;
}

bool CalmWobble(uint32_t* tokens, size_t count, float calmFrom) {
	if (tokens == nullptr || !(calmFrom >= kCalmFromLeast) || !(calmFrom <= kCalmFromMost)) {
		return false;
	}
	const uint32_t reachBits = Bits(1.0f / kWobbleReach);
	const uint32_t lengthBits = Bits(kWobbleLength);
	size_t reachAt = 0, lengthAt = 0;
	int reaches = 0, lengths = 0;
	ForEachDefValue(tokens, count, [&](size_t at) {
		if (tokens[at] == reachBits) {
			reachAt = at;
			++reaches;
		} else if (tokens[at] == lengthBits) {
			lengthAt = at;
			++lengths;
		}
	});
	if (reaches != 1 || lengths != 1) {
		return false;
	}
	tokens[reachAt] = Bits(1.0f / calmFrom);
	tokens[lengthAt] = Bits(kWobbleLength / kWobbleReach * calmFrom);
	return true;
}

}  // namespace wateraa
