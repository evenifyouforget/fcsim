#include "xoroshiro.hpp"

uint64_t xoroshiro256pp::next() {
    const uint64_t result = rotl(s[0] + s[3], 23) + s[0];

    const uint64_t t = s[1] << 17;

    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];

    s[2] ^= t;

    s[3] = rotl(s[3], 45);

    return result;
}

void xoroshiro256pp::add_entropy(uint64_t value) {
    s[0] += value;
    s[1] += value * 0x3abf7d710007d1a7ull;
    s[2] += 0x2a0546aec5cd23bull;
}

xoroshiro256pp general_prng;

// glue code
extern "C" void general_prng_add_entropy(uint64_t value) {
    general_prng.add_entropy(value);
}