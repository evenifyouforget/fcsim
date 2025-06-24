#ifdef __wasm__
#include "stddef.h"
#else
#include <cstddef>
#endif
#include "arena.hpp"
#include "stl_compat.h"
#include "interval.h"

extern "C" void tick_func(void *arg)
{
	arena* the_arena = (arena*)arg;

	double time_start = time_precise_ms();
	for(int i = 0; is_running(the_arena) && the_arena->single_ticks_remaining != 0 && i < the_arena->tick_multiply; ++i) {
		if(the_arena->single_ticks_remaining > 0)the_arena->single_ticks_remaining--;
		step(the_arena->world);
		the_arena->tick++;
		if (!the_arena->has_won && goal_blocks_inside_goal_area(&the_arena->design)) {
			the_arena->has_won = true;
			the_arena->tick_solve = the_arena->tick;
			if(the_arena->autostop_on_solve) {
				the_arena->autostop_on_solve = false;
				the_arena->single_ticks_remaining = 0;
				break;
			}
		}
		double time_end = time_precise_ms();
		if(time_end - time_start >= the_arena->tick_ms)break;
	}

    if(the_arena->preview_gp_trajectory) {
        // use remaining time budget for preview hyperspeed
        if(the_arena->preview_trail == nullptr) {
            the_arena->preview_trail = _new<multi_trail_t>();
        }
        multi_trail_t* all_trails = (multi_trail_t*)the_arena->preview_trail;
        if(the_arena->preview_design == nullptr || the_arena->preview_design->modcount != the_arena->design.modcount) {
            // refresh preview design
            // manually clear old data
            if(the_arena->preview_world) {
                free_world(the_arena->preview_world, the_arena->preview_design);
            }
            the_arena->preview_world = nullptr;
            if(the_arena->preview_design) {
                free_design(the_arena->preview_design);
            }
            the_arena->preview_design = nullptr;
            // populate new data from clone
            the_arena->preview_design = clean_copy_design(&the_arena->design);
            the_arena->preview_world = gen_world(the_arena->preview_design);
            // clear trails
            all_trails->trails.clear();
        }
        // tick until time budget is exhausted
        while(all_trails->accepting()) {
            all_trails->submit_frame(the_arena->preview_design);
            step(the_arena->preview_world);
            double time_end = time_precise_ms();
            if(time_end - time_start >= the_arena->tick_ms)break;
        }
    }
}

bool multi_trail_t::accepting() {
    const size_t PREVIEW_TICK_LIMIT = 10000;
    return trails.size() == 0 || trails[0].datapoints.size() < PREVIEW_TICK_LIMIT;
}

void multi_trail_t::submit_frame(design* current_design) {
    // scan for goal pieces
    size_t trail_index = 0;
    for (block* the_block = current_design->player_blocks.head; the_block; the_block = the_block->next) {
		if (the_block->goal) {
            // record position in the corresponding location
            while(trails.size() <= trail_index) {
                trails.emplace_back();
            }
            const b2Vec2 position = the_block->body->m_position;
            trails[trail_index].datapoints.push_back(position);
            trail_index++;
		}
	}
}

extern "C" int block_inside_area(struct block *block, struct area *area)
{
	struct area bb;

	get_block_bb(block, &bb);

	return bb.x - bb.w / 2 >= area->x - area->w / 2
	    && bb.x + bb.w / 2 <= area->x + area->w / 2
	    && bb.y - bb.h / 2 >= area->y - area->h / 2
	    && bb.y + bb.h / 2 <= area->y + area->h / 2;
}

extern "C" bool goal_blocks_inside_goal_area(design* design)
{
    for (block* block_ptr = design->player_blocks.head; block_ptr != nullptr; block_ptr = block_ptr->next) {
        if (block_ptr->goal) {
            if (!block_inside_area(block_ptr, &design->goal_area))
                return false;
            return true;
        }
    }
    return false;
}

extern "C" double goal_heuristic(struct design *design) {
    const double IN_GOAL_BONUS = 1e8;
    double score = 0;
    for (block* block_ptr = design->player_blocks.head; block_ptr != nullptr; block_ptr = block_ptr->next) {
        if (block_ptr->goal) {
            if (block_inside_area(block_ptr, &design->goal_area)) {
                score -= IN_GOAL_BONUS;
            } else {
                	struct area bb;
	                get_block_bb(block_ptr, &bb);
                    double dx = bb.x - design->goal_area.x;
                    double dy = bb.y - design->goal_area.y;
                    score += dx * dx + dy * dy; // distance squared
            }
        }
    }
    return score;
}