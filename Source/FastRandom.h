#pragma once

#include <cstdint>

// Tiny xorshift32 PRNG for audio noise. Unlike std::rand() it has no global
// state or locks, so it is safe on the audio thread and every engine owns its
// own stream.
class FastRandom {
public:
    explicit FastRandom(uint32_t seed = 0x9E3779B9u) { setSeed(seed); }

    void setSeed(uint32_t seed) { state = seed != 0 ? seed : 0x1u; }

    // Uniform in [-1, 1).
    inline float nextBipolar() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<float>(state & 0xFFFFFFu) * (1.0f / 8388608.0f) - 1.0f;
    }

    // Uniform in [0, 1).
    inline float nextUnit() { return nextBipolar() * 0.5f + 0.5f; }

private:
    uint32_t state = 1u;
};
