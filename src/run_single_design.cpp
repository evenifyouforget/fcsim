extern "C" {
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "xml.h"
#include "graph.h"
#include "arena.h"
}

#include "box2d/b2Body.h"

#include <iostream>
#include <vector>

extern "C" {
int fp_strtod(const char *str, int len, double *res);
}

arena* arena_ptr;

// Map ftlib piece types to fcsim types
static int map_piece_type(int ftlib_type) {
    // ftlib type mapping from fcsim.h in ftlib
    // FCSIM_STATIC_RECT = 0, FCSIM_STATIC_CIRC = 1, FCSIM_DYNAMIC_RECT = 2, FCSIM_DYNAMIC_CIRC = 3,
    // FCSIM_GP_RECT = 4, FCSIM_GP_CIRC = 5, FCSIM_UPW = 6, FCSIM_CW = 7, FCSIM_CCW = 8,
    // FCSIM_WATER = 9, FCSIM_WOOD = 10
    
    // From xml.h in fcsim:
    // XML_STATIC_RECTANGLE = 0, XML_STATIC_CIRCLE = 1, XML_DYNAMIC_RECTANGLE = 2, XML_DYNAMIC_CIRCLE = 3,
    // XML_JOINTED_DYNAMIC_RECTANGLE = 4, XML_NO_SPIN_WHEEL = 5, XML_CLOCKWISE_WHEEL = 6, 
    // XML_COUNTER_CLOCKWISE_WHEEL = 7, XML_SOLID_ROD = 8, XML_HOLLOW_ROD = 9
    
    switch (ftlib_type) {
        case 0: return 0; // STATIC_RECT -> XML_STATIC_RECTANGLE
        case 1: return 1; // STATIC_CIRC -> XML_STATIC_CIRCLE
        case 2: return 2; // DYNAMIC_RECT -> XML_DYNAMIC_RECTANGLE 
        case 3: return 3; // DYNAMIC_CIRC -> XML_DYNAMIC_CIRCLE
        case 4: return 4; // GP_RECT -> XML_JOINTED_DYNAMIC_RECTANGLE
        case 5: return 5; // GP_CIRC -> XML_NO_SPIN_WHEEL
        case 6: return 5; // UPW -> XML_NO_SPIN_WHEEL
        case 7: return 6; // CW -> XML_CLOCKWISE_WHEEL
        case 8: return 7; // CCW -> XML_COUNTER_CLOCKWISE_WHEEL
        case 9: return 9; // WATER -> XML_HOLLOW_ROD
        case 10: return 8; // WOOD -> XML_SOLID_ROD
        default: return 2; // Default to dynamic rect
    }
}

double read_double_janky() {
    std::string str;
    double result;
    if(false) {
        std::cin >> result;
        return result;
    }
    std::cin >> str;
    fp_strtod(str.c_str(), str.length(), &result);
    return result;
}

static bool init_sim_from_input(int max_ticks) {
    // Read ftlib format from stdin
    int num_blocks;
    if (scanf("%d", &num_blocks) != 1) {
        return false;
    }
    
    // Create XML structures to reuse existing conversion code
    struct xml_level level = {0};
    
    // Read blocks and create linked list
    struct xml_block* prev_level_block = NULL;
    struct xml_block* prev_player_block = NULL;
    
    for (int i = 0; i < num_blocks; i++) {
        int type_id, id;
        double x, y, w, h, angle;
        int joint1, joint2;

        std::cin >> type_id >> id;
        x = read_double_janky();
        y = read_double_janky();
        w = read_double_janky();
        h = read_double_janky();
        angle = read_double_janky();
        std::cin >> joint1 >> joint2;
        
        struct xml_block* block = (struct xml_block*)calloc(1, sizeof(struct xml_block));
        if (!block) {
            return false;
        }
        
        block->type = map_piece_type(type_id);
        block->id = id;
        block->position.x = x;
        block->position.y = y;
        
        // Convert circle sizes from ftlib diameter to XML radius
        // Only apply to static/dynamic circles (1, 3), NOT to wheels (5, 7, 8) 
        if (type_id == 1 || type_id == 3) { // Only static/dynamic circles
            block->width = w / 2.0;   // ftlib diameter -> XML radius
            block->height = h / 2.0;
        } else {
            block->width = w;         // Wheels and rods use same size in both formats
            block->height = h;
        }
        
        block->rotation = angle;
        block->goal_block = (type_id == 4 || type_id == 5); // GP pieces are goal blocks
        block->joints = NULL;
        block->next = NULL;
        
        // Add joints if they exist
        if (joint1 != -1 || joint2 != -1) {
            if (joint1 != -1) {
                struct xml_joint* joint = (struct xml_joint*)calloc(1, sizeof(struct xml_joint));
                joint->id = joint1;
                joint->next = block->joints;
                block->joints = joint;
            }
            if (joint2 != -1) {
                struct xml_joint* joint = (struct xml_joint*)calloc(1, sizeof(struct xml_joint));
                joint->id = joint2;
                joint->next = block->joints;
                block->joints = joint;
            }
        }
        
        // Add to appropriate list (level blocks for static/dynamic, player blocks for GP/wheels/rods)
        if (type_id <= 3) { // Static and dynamic pieces -> level blocks
            if (!level.level_blocks) {
                level.level_blocks = block;
            } else {
                prev_level_block->next = block;
            }
            prev_level_block = block;
        } else { // GP pieces, wheels, rods -> player blocks
            if (!level.player_blocks) {
                level.player_blocks = block;
            } else {
                prev_player_block->next = block;
            }
            prev_player_block = block;
        }
    }
    
    // Read build area
    if (scanf("%lf %lf %lf %lf", &level.start.position.x, &level.start.position.y, 
              &level.start.width, &level.start.height) != 4) {
        return false;
    }
    
    // Read goal area  
    if (scanf("%lf %lf %lf %lf", &level.end.position.x, &level.end.position.y,
              &level.end.width, &level.end.height) != 4) {
        return false;
    }
    
    // Convert to internal representation using existing code
    convert_xml(&level, &arena_ptr->design);
    
    xml_free(&level);
    
    // Generate physics world
    arena_ptr->world = gen_world(&arena_ptr->design);
    if (!arena_ptr->world) {
        return false;
    }
    
    arena_ptr->tick = 0;
    arena_ptr->has_won = false;
    arena_ptr->tick_solve = 0;
    
    return true;
}

int main() {
    // Read max_ticks from stdin (first parameter)
    int max_ticks;
    if (scanf("%d", &max_ticks) != 1) {
        return 1;
    }
    
    // Initialize simulation from ftlib format input
    arena_ptr = new arena();
    if (!init_sim_from_input(max_ticks)) {
        return 1;
    }
    
    // Run to solve or end
    arena_ptr->state = STATE_RUNNING;
    change_speed_preset(arena_ptr, 2);
    while((int64_t)arena_ptr->tick != max_ticks && !arena_ptr->has_won) {
        arena_ptr->single_ticks_remaining = 1;
        tick_func(arena_ptr);
    }

    // Report
    std::cout << (arena_ptr->has_won ? arena_ptr->tick_solve : -1) << std::endl << arena_ptr->tick << std::endl;
    
    return 0;
}

// stubs - functions that are called somewhere and therefore require linking
// but don't have any effect on this CLI use case

extern "C" {

int set_interval(void (*func)(void *arg), int delay, void *arg) {return 0;}

void clear_interval(int id) {}

double time_precise_ms() {return 0;}

}
