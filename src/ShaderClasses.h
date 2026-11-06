#pragma once

#include <cstddef>
#include <cstdint>

// Which pixel shader objects have a calmed twin, by their addresses.
//
// A shader is classed once - when the plugin sees it created, or the first
// time it is set if it was created before the plugin came up - and found
// again by its address at every SetPixelShader. An address is free once its
// shader is released, so a creation always overwrites what the address was
// classed as. Open addressing over a fixed table; when it is full it is
// emptied and fills again as shaders are set. Pure; ShaderClassesTest.

namespace wateraa {

enum class ShaderClass : uint8_t {
	Unknown = 0,
	Other = 1,   // no twin: set as the game asks
	Calmed = 2,  // a water shader that mirrors, with its twin
};

template <size_t Capacity>
class ShaderClassMap {
public:
	ShaderClass Find(const void* shader) const {
		if (shader == nullptr) {
			return ShaderClass::Unknown;
		}
		for (size_t probe = 0, i = Start(shader); probe < Capacity; ++probe, i = (i + 1) % Capacity) {
			if (keys_[i] == nullptr) {
				return ShaderClass::Unknown;
			}
			if (keys_[i] == shader) {
				return classes_[i];
			}
		}
		return ShaderClass::Unknown;
	}

	// Classes `shader`, over what its address was classed as before. Null is
	// not kept.
	void Put(const void* shader, ShaderClass shaderClass) {
		if (shader == nullptr) {
			return;
		}
		// Kept under three quarters full, so a probe always ends at a free entry.
		if (count_ >= Capacity - Capacity / 4 && Find(shader) == ShaderClass::Unknown) {
			Clear();
		}
		for (size_t i = Start(shader);; i = (i + 1) % Capacity) {
			if (keys_[i] == nullptr) {
				keys_[i] = shader;
				classes_[i] = shaderClass;
				++count_;
				return;
			}
			if (keys_[i] == shader) {
				classes_[i] = shaderClass;
				return;
			}
		}
	}

	void Clear() {
		for (size_t i = 0; i < Capacity; ++i) {
			keys_[i] = nullptr;
			classes_[i] = ShaderClass::Unknown;
		}
		count_ = 0;
	}

	size_t Count() const { return count_; }

private:
	static size_t Start(const void* shader) {
		// Objects are aligned: the low bits say nothing.
		return static_cast<size_t>((reinterpret_cast<uintptr_t>(shader) >> 4) * 2654435761u) % Capacity;
	}

	const void* keys_[Capacity] = {};
	ShaderClass classes_[Capacity] = {};
	size_t count_ = 0;
};

}  // namespace wateraa
