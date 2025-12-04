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

// recommended to keep this to at most FCSIM_EXTRA_COLORS
// but safe even if increased higher
#define MAX_JOINT_TRACKERS 23

struct trail_t {
    std::vector<b2Vec2> datapoints;
};

struct multi_trail_t {
    std::vector<uint64_t> joint_tracker_offsets;
    int joint_tracker_bump = 0;
    std::vector<trail_t> trails;
    size_t num_goal_pieces = 0;
    bool accepting();
    void submit_frame(design*);
};

std::vector<joint> generate_joints(block* block, bool allow_fake_joints);

#endif // ARENA_HPP