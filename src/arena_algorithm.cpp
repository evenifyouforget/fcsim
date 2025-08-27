#ifdef __wasm__
#include "stddef.h"
#else
#include <cstddef>
#include <iostream>
#include <iomanip>
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
        #ifdef CLI
        // log all blocks just before step
        std::cerr << std::setprecision(17);
        std::cerr << "Tick " << the_arena->tick << std::endl;
        for(block* block_ptr = the_arena->design.player_blocks.head; block_ptr; block_ptr = block_ptr->next) {
            b2Body* body_ptr = block_ptr->body;
            std::cerr << "- ID = " << block_ptr->id << ", Type = " << (int)block_ptr->type_id << ", Pos = (" << body_ptr->m_position.x
            << ", " << body_ptr->m_position.y << "), Vel = (" << body_ptr->m_linearVelocity.x << ", "
            << body_ptr->m_linearVelocity.y << "), Ang = " << body_ptr->m_rotation << ", AngVel = " << body_ptr->m_angularVelocity
            << std::endl;
        }
        #endif
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
            the_arena->preview_has_won = false;
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
            // clear trails and free memory (replace with empty vector)
            all_trails->trails = std::vector<trail_t>();
        }
        bool is_preview_design_legal = is_design_legal(the_arena->preview_design);
        // tick until time budget is exhausted
        while(all_trails->accepting()) {
            all_trails->submit_frame(the_arena->preview_design);
            step(the_arena->preview_world);
            if(is_preview_design_legal && goal_blocks_inside_goal_area(the_arena->preview_design)) {
                the_arena->preview_has_won = true;
            }
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