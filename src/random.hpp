#ifndef RANDOM_HPP
#define RANDOM_HPP

#include <stdint.h>

inline uint64_t rotl(const uint64_t x, int k) {
	return (x << k) | (x >> (64 - k));
}

struct prng_t {
    uint64_t s[4];
    uint64_t next();
    double next_uniform();
};

#endif // RANDOM_HPP