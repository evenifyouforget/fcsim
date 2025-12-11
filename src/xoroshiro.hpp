#ifndef XOROSHIRO_HPP
#define XOROSHIRO_HPP

// adapted from https://prng.di.unimi.it/xoshiro256plusplus.c
#include <stdint.h>

inline uint64_t rotl(const uint64_t x, int k) {
	return (x << k) | (x >> (64 - k));
}

struct xoroshiro256pp {
    uint64_t s[4];
    uint64_t next();
    void add_entropy(uint64_t value);
};

extern xoroshiro256pp general_prng;

#endif // XOROSHIRO_HPP
