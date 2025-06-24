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
    trail_t trail;
    design* design_ptr = nullptr;
    b2World* world_ptr = nullptr;
    int tick = 0;
    double best_score = 1e300;
    // since we treat it as a data struct, we need to manually initialize or destroy it
    void init_copy_design(design*);
    void destroy();
};

struct garden_t {
    std::vector<creature_t> creatures;
    // dynamically updating estimate of how many ticks per creature we can fit in our time budget per frame
    int ticks_per_creature = 1;
    void clear();
};

// make the garden object if it does not already exist, ensures the pointer is non-null after
void ensure_garden_exists(arena*);
// delete the garden data, and remake a fresh garden from the current main design
void reset_garden(arena*);
// overwrite the current main design with the best scoring design from the garden
void take_best_design_from_garden(arena*);

#endif // ARENA_HPP