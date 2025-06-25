#ifdef __wasm__
#include "stddef.h"
#else
#include <cstddef>
#endif
#include "arena.hpp"
#include "stl_compat.h"
#include "interval.h"

extern "C" {
#include "random.h"
#include "xml.h"
#include "text.h"
}

// replace a creature with a new one in place, mutated from a certain parent
void garden_reproduce(garden_t* garden, size_t parent_index, size_t child_index)
{
    // reset it to a blank struct
    garden->creatures[child_index].destroy();
    garden->creatures[child_index] = creature_t();
    // make a working copy of the parent creature's design
    design* new_design = clean_copy_design(garden->creatures[parent_index].design_ptr);
    // mutate the design
    for(block* b = new_design->player_blocks.head; b; b = b->next) {
        // simplest behaviour: each piece has a random chance of being wiggled, and only wiggle rods
        if(b->type_id != FCSIM_ROD && b->type_id != FCSIM_SOLID_ROD) {
            continue;
        }
        const double offset = random_uniform() * 2 - 1;
        if(random_uniform() < 0.03) {
            b->shape.rod.from->x += offset * 1e-2;
        }
        if(random_uniform() < 0.03) {
            b->shape.rod.from->y += offset * 1e-2;
        }
    }
    // initialize the new creature from the working copy
    garden->creatures[child_index].init_copy_design(new_design);
    // free the working copy
    free_design(new_design);
}

extern "C" void tick_func(void *arg)
{
	arena* the_arena = (arena*)arg;

	double time_start = time_precise_ms();
	for(int i = 0; is_running(the_arena) && the_arena->single_ticks_remaining != 0 && i < the_arena->tick_multiply; ++i) {
		if(the_arena->single_ticks_remaining > 0)the_arena->single_ticks_remaining--;
		step(the_arena->world);
		the_arena->tick++;
		if (!the_arena->has_won && goal_blocks_inside_goal_area(the_arena->design)) {
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
        if(the_arena->preview_design == nullptr || the_arena->preview_design->modcount != the_arena->design->modcount) {
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
            the_arena->preview_design = clean_copy_design(the_arena->design);
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

    if(the_arena->garden_queued_action == GARDEN_ACTION_RESET) {
        reset_garden(the_arena);
    }else if(the_arena->garden_queued_action == GARDEN_ACTION_TAKE_BEST) {
        take_best_design_from_garden(the_arena);
    }
    the_arena->garden_queued_action = GARDEN_ACTION_NONE;
    if(the_arena->use_garden) {
        // use the remaining time budget for garden simulation
        ensure_garden_exists(the_arena);
        garden_t* garden = (garden_t*)the_arena->garden;
        /*
         * Garden algorithm description:
         * - Tick all creatures in the garden if they are legal and not stale (reached max ticks)
         * - If there is an illegal creature, or at least N creatures are stale, kill the worst stale creature and
         *   replace it with a mutant from a random other creature
         * - Adjust the ticks budget
         */
        // tick all creatures
        size_t illegal_creature_index = SIZE_MAX;
        size_t num_stale = 0;
        for (size_t j = 0; j < garden->creatures.size(); ++j) {
            num_stale += (garden->creatures[j].tick >= GARDEN_MAX_TICKS);
        }
        size_t ticks_budget_per_creature = std::max<size_t>(1, garden->total_ticks_budget
            / std::max<size_t>(1, garden->creatures.size() - num_stale));
        num_stale = 0;
        size_t stale_index = SIZE_MAX;
        for (size_t j = 0; j < garden->creatures.size(); ++j) {
            creature_t& creature = garden->creatures[j];
            if(creature.tick == 0) {
                // check if it is legal
                // we don't remember this, so we may check it redundantly
                mark_overlaps_data(creature.design_ptr, creature.world_ptr, nullptr);
                if(!is_design_legal(creature.design_ptr)) {
                    illegal_creature_index = j;
                }
            }
            for(size_t i = 0; j != illegal_creature_index &&
                i < ticks_budget_per_creature && creature.tick < GARDEN_MAX_TICKS; ++i) {
                step(creature.world_ptr);
                creature.tick++;
                creature.best_score = std::min(creature.best_score, goal_heuristic(creature.design_ptr));
                if(creature.trails.accepting()) {
                    // submit a frame to the trail
                    creature.trails.submit_frame(creature.design_ptr);
                }
            }
            const bool is_stale = (creature.tick >= GARDEN_MAX_TICKS);
            num_stale += is_stale;
            if(is_stale) {
                stale_index = j;
            }
        }
        // kill the worst creature if needed
        if(illegal_creature_index != SIZE_MAX || num_stale >= GARDEN_TRIGGER_STALE_CREATURES) {
            // find worst creature index, or just use the illegal creature if it exists
            size_t worst_index = stale_index;
            if(illegal_creature_index != SIZE_MAX) {
                worst_index = illegal_creature_index;
            }else{
                // find the worst creature by score, but only stale ones
                for(size_t i = 0; i < garden->creatures.size(); ++i) {
                    if(garden->creatures[i].tick >= GARDEN_MAX_TICKS
                        && garden->creatures[i].best_score > garden->creatures[worst_index].best_score) {
                        worst_index = i;
                    }
                }
            }
            // choose a new parent creature randomly
            size_t parent_index = worst_index;
            while(parent_index == worst_index) {
                parent_index = random_u64() % garden->creatures.size();
            }
            garden_reproduce(garden, parent_index, worst_index);
        }
        // update ticks budget estimate based on if we are early or late
        long long int tt = garden->total_ticks_budget;
        double time_end = time_precise_ms();
        if(num_stale == garden->creatures.size() && time_end - time_start >= the_arena->tick_ms * GARDEN_TIME_FACTOR) {
            // we finished late, we are over budget
            // or all designs are finished (stale)
            tt = std::max(1LL, tt - 1 - tt / 10);
        } else {
            // we finished early, we are under budget
            tt += 1 + tt / 100;
        }
        garden->total_ticks_budget = tt;
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
    const double NOT_IN_GOAL_PENALTY = 1e8;
    double score = 0;
    for (block* block_ptr = design->player_blocks.head; block_ptr != nullptr; block_ptr = block_ptr->next) {
        if (block_ptr->goal) {
            if (!block_inside_area(block_ptr, &design->goal_area)) {
                score += NOT_IN_GOAL_PENALTY;
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

void creature_t::init_copy_design(design* src_design_ptr) {
    design_ptr = clean_copy_design(src_design_ptr);
    world_ptr = gen_world(design_ptr);
}

void creature_t::destroy() {
    if(world_ptr) {
        free_world(world_ptr, design_ptr);
    }
    world_ptr = nullptr;
    if(design_ptr) {
        free_design(design_ptr);
    }
    design_ptr = nullptr;
}

void garden_t::clear() {
    for (creature_t& creature : creatures) {
        creature.destroy();
    }
    creatures.clear();
}

void ensure_garden_exists(arena* arena) {
    if (!arena->garden) {
        reset_garden(arena);
    }
}

void reset_garden(arena* arena_ptr) {
    if (!arena_ptr->garden) {
        arena_ptr->garden = _new<garden_t>();
    }
    garden_t* garden = (garden_t*)arena_ptr->garden;
    garden->clear();
    // fill with new creatures
    for(size_t i = 0; i < GARDEN_MAX_CREATURES; ++i) {
        garden->creatures.emplace_back();
        garden->creatures[i].init_copy_design(arena_ptr->design);
    }
}

void take_best_design_from_garden(arena* arena_ptr) {
    ensure_garden_exists(arena_ptr);
    garden_t* garden = (garden_t*)arena_ptr->garden;
    if (garden->creatures.size() == 0) {
        return; // nothing to take
    }
    // find the creature with the best (lowest) score
    creature_t* best_creature = &garden->creatures[0];
    for (creature_t& creature : garden->creatures) {
        if (creature.best_score < best_creature->best_score) {
            best_creature = &creature;
        }
    }
    // free the old design and world
    if(arena_ptr->world) {
        free_world(arena_ptr->world, arena_ptr->design);
        arena_ptr->world = nullptr;
    }
    free_design(arena_ptr->design);
    arena_ptr->design = nullptr;
    // copy the best design into the arena's design
    arena_ptr->design = clean_copy_design(best_creature->design_ptr);
    // regenerate the world
    arena_ptr->world = gen_world(arena_ptr->design);
    // reset operation data
    arena_reset_operations(arena_ptr);
}

extern "C" void arena_init(struct arena *arena, float w, float h, char *xml, int len)
{
	struct xml_level level;

    arena->design = _new<design>();

	arena->view.x = 0.0f;
	arena->view.y = 0.0f;
	arena->view.width = w;
	arena->view.height = h;
	arena->view.scale = 1.0f;
	arena->cursor_x = 0;
	arena->cursor_y = 0;

    arena_reset_operations(arena);

	xml_parse(xml, len, &level);
	convert_xml(&level, arena->design);

	arena->world = gen_world(arena->design);

	block_graphics_init(arena);

	/*
	glGenBuffers(1, &arena->joint_coord_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, arena->joint_coord_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(arena->joint_coords),
		     arena->joint_coords, GL_STREAM_DRAW);
	*/

	arena->tick = 0;
	text_stream_create(&arena->tick_counter, MAX_RENDER_TEXT_LENGTH);
	arena->has_won = false;

	arena->preview_gp_trajectory = false;
	arena->preview_design = NULL;
	arena->preview_world = NULL;

	change_speed_preset(arena, 2);
}

void arena_reset_operations(arena* arena) {
	arena->shift = false;
	arena->ctrl = false;

	arena->tool = TOOL_MOVE;
	arena->tool_hidden = TOOL_MOVE;
	arena->state = STATE_NORMAL;
	arena->hover_joint = NULL;
	arena->hover_block = NULL;

	arena->root_joints_moving = NULL;
	arena->root_blocks_moving = NULL;
	arena->blocks_moving = NULL;
}