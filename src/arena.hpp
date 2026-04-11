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

struct HitTestState {
  bool sticky = false;
  int ref_px = 0, ref_py = 0;            /* pixel coords when sticky started */
  std::vector<struct joint *> candidates; /* joints within hit radius, sorted by distance */
  int sel_idx = 0;                        /* index of selected candidate */
  int last_modcount = -1;                 /* design modcount at last candidate refresh */
};

struct trail_t {
  std::vector<b2Vec2> datapoints;
};

struct multi_trail_t {
  std::vector<trail_t> trails;
  bool accepting();
  void submit_frame(design *);
};

#endif // ARENA_HPP
