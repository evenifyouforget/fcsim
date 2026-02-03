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

struct checksum_state_t {
    uint64_t value = 0;
    uint64_t joint_uid = 0;
};

uint64_t _checksum_value_combine(uint64_t x, uint64_t y) {
    x = x * 0x6a999a34a7c5df1bull + y + 0xd10dbc37a7c9b29bull;
    x ^= x >> 33;
    return x;
}

void _add_value_to_checksum(checksum_state_t& state, uint64_t x) {
    state.value = _checksum_value_combine(state.value, x);
}

void _add_value_to_checksum(checksum_state_t& state, double x) {
    union {
        double as_double;
        uint64_t as_uint64;
    } union_value;
    union_value.as_double = x;
    _add_value_to_checksum(state, union_value.as_uint64);
}

void _add_area_to_checksum(checksum_state_t& state, struct area* area) {
    _add_value_to_checksum(state, area->x);
    _add_value_to_checksum(state, area->y);
    _add_value_to_checksum(state, area->w);
    _add_value_to_checksum(state, area->h);
    //_add_value_to_checksum(state, area->expand);
}

void _add_joint_to_checksum(checksum_state_t& state, struct joint* joint) {
    if(joint == nullptr) {
        _add_value_to_checksum(state, (uint64_t)(0));
        return;
    }
    _add_value_to_checksum(state, (uint64_t)(joint->_checksum_uid));
}

void _assign_joint_uid(checksum_state_t& state, struct joint* joint, int phase) {
    if(joint == nullptr) {
        return;
    }
    switch(phase) {
        case 0: {
            // set initial id
            joint->_checksum_uid = ++state.joint_uid;
            break;
        }
        case 1: {
            // count neighbours
            if(joint->prev != nullptr) {
                joint->_checksum_uid = _checksum_value_combine(joint->_checksum_uid, 1);
                joint->_checksum_uid = _checksum_value_combine(joint->_checksum_uid, joint->prev->_checksum_uid);
            }
            if(joint->next != nullptr) {
                joint->_checksum_uid = _checksum_value_combine(joint->_checksum_uid, 2);
                joint->_checksum_uid = _checksum_value_combine(joint->_checksum_uid, joint->next->_checksum_uid);
            }
            break;
        }
    }
}

void _add_block_to_checksum(checksum_state_t& state, struct block* block, int phase) {
    switch(phase) {
        case 0:
        case 1: {
            // joint-only passes
            switch(block->shape.type) {
                case shape_type::SHAPE_BOX: {
                    _assign_joint_uid(state, block->shape.box.center, phase);
                    for(int i = 0; i < 4; ++i) {
                        _assign_joint_uid(state, block->shape.box.corners[i], phase);
                    }
                    break;
                }
                case shape_type::SHAPE_ROD: {
                    _assign_joint_uid(state, block->shape.rod.from, phase);
                    _assign_joint_uid(state, block->shape.rod.to, phase);
                    break;
                }
                case shape_type::SHAPE_WHEEL: {
                    _assign_joint_uid(state, block->shape.wheel.center, phase);
                    for(int i = 0; i < 4; ++i) {
                        _assign_joint_uid(state, block->shape.wheel.spokes[i], phase);
                    }
                    break;
                }
            }
            break;
        }
        case 2: {
            // gather main hash
            _add_value_to_checksum(state, (uint64_t)(block->type_id));
            _add_value_to_checksum(state, (uint64_t)(block->shape.type));
            switch(block->shape.type) {
                case shape_type::SHAPE_RECT: {
                    _add_value_to_checksum(state, block->shape.rect.x);
                    _add_value_to_checksum(state, block->shape.rect.y);
                    _add_value_to_checksum(state, block->shape.rect.w);
                    _add_value_to_checksum(state, block->shape.rect.h);
                    _add_value_to_checksum(state, block->shape.rect.angle);
                    break;
                }
                case shape_type::SHAPE_CIRC: {
                    _add_value_to_checksum(state, block->shape.circ.x);
                    _add_value_to_checksum(state, block->shape.circ.y);
                    _add_value_to_checksum(state, block->shape.circ.radius);
                    break;
                }
                case shape_type::SHAPE_BOX: {
                    _add_value_to_checksum(state, block->shape.box.x);
                    _add_value_to_checksum(state, block->shape.box.y);
                    _add_value_to_checksum(state, block->shape.box.w);
                    _add_value_to_checksum(state, block->shape.box.h);
                    _add_value_to_checksum(state, block->shape.box.angle);
                    _add_joint_to_checksum(state, block->shape.box.center);
                    for(int i = 0; i < 4; ++i) {
                        _add_joint_to_checksum(state, block->shape.box.corners[i]);
                    }
                    break;
                }
                case shape_type::SHAPE_ROD: {
                    _add_joint_to_checksum(state, block->shape.rod.from);
                    _add_joint_to_checksum(state, block->shape.rod.to);
                    _add_value_to_checksum(state, block->shape.rod.width);
                    break;
                }
                case shape_type::SHAPE_WHEEL: {
                    _add_joint_to_checksum(state, block->shape.wheel.center);
                    _add_value_to_checksum(state, block->shape.wheel.radius);
                    _add_value_to_checksum(state, block->shape.wheel.angle);
                    //_add_value_to_checksum(state, (uint64_t)(block->shape.wheel.spin));
                    for(int i = 0; i < 4; ++i) {
                        _add_joint_to_checksum(state, block->shape.wheel.spokes[i]);
                    }
                    break;
                }
            }
            // shell is theoretically redundant, but we add it anyway
            shell shell_normalized;
            get_shell(&shell_normalized, &block->shape);
            _add_value_to_checksum(state, shell_normalized.x);
            _add_value_to_checksum(state, shell_normalized.y);
            _add_value_to_checksum(state, shell_normalized.angle);
            _add_value_to_checksum(state, (uint64_t)(shell_normalized.type));
            switch(shell_normalized.type) {
                case SHELL_CIRC: {
                    _add_value_to_checksum(state, shell_normalized.circ.radius);
                    break;
                }
                case SHELL_RECT: {
                    _add_value_to_checksum(state, shell_normalized.rect.w);
                    _add_value_to_checksum(state, shell_normalized.rect.h);
                    break;
                }
            }
            break;
        }
    }
}

extern "C" int recalculate_design_checksum(struct design *design) {
    checksum_state_t state;
    struct block *block;
    for(int phase = 0; phase < 3; ++phase) {
        for (block = design->level_blocks.head; block; block = block->next)
            _add_block_to_checksum(state, block, phase);

        for (block = design->player_blocks.head; block; block = block->next)
            _add_block_to_checksum(state, block, phase);
    }
    _add_area_to_checksum(state, &design->build_area);
    _add_area_to_checksum(state, &design->goal_area);
    int result = (int)(state.value & 0x7fffffff);
    // require not 0
    if(result == 0) {
        result = 1;
    }
    design->actual_checksum = result;
    return result;
}

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
            // clear trails
            all_trails->trails.clear();
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