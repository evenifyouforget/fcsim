#ifndef ARENA_HPP
#define ARENA_HPP

#ifdef __wasm__
#include "stl_mock.h"
#else
#include <vector>
#endif
extern "C" {
#include "arena.h"
#include "box2d/b2Body.h"
#include "box2d/b2Vec.h"
}

#define MAX_TRACKER_OFFSETS 25

struct trail_t {
    std::vector<b2Vec2> datapoints;
};

struct multi_trail_t {
    std::vector<uint64_t> tracker_offsets;
    std::vector<trail_t> trails;
    bool accepting();
    void submit_frame(design*);
};

#endif // ARENA_HPP