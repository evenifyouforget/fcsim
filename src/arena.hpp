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

struct trail_t {
    std::vector<b2Vec2> datapoints;
};

struct multi_trail_t {
    std::vector<trail_t> trails;
    bool accepting();
    void submit_frame(design*);
};

struct creature_t {
    multi_trail_t trails;
    design* design_ptr = nullptr;
    b2World* world_ptr = nullptr;
    int tick = 0;
    double best_score = INFINITY; // lower is better
    // since we treat it as a data struct, we need to manually initialize or destroy it
    void init_copy_design(design*);
    void destroy();
};

struct garden_t {
    std::vector<creature_t> creatures;
    // dynamically updating estimate of how many ticks we can fit in our time budget per frame
    size_t total_ticks_budget = 1;
    void clear();
};

// make the garden object if it does not already exist, ensures the pointer is non-null after
void ensure_garden_exists(arena*);
// delete the garden data, and remake a fresh garden from the current main design
void reset_garden(arena*);
// overwrite the current main design with the best scoring design from the garden
void take_best_design_from_garden(arena*);

void arena_reset_operations(arena*);

#endif // ARENA_HPP