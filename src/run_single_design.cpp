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
#include <cstring>

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



int main(int argc, char* argv[]) {
    // CHIMERA: Read max_ticks from command line argument (matching XML version interface)
    int64_t max_ticks = 1000; // Default value
    if (argc > 1) {
        max_ticks = atoi(argv[1]);
    }

    // Read max_ticks from stdin (first parameter) for ftlib compatibility
    int stdin_max_ticks;
    if (scanf("%d", &stdin_max_ticks) != 1) {
        fprintf(stderr, "Failed to read max_ticks from stdin\n");
        return 1;
    }
    max_ticks = stdin_max_ticks; // Use stdin value if provided
    
    // CHIMERA: Parse ftlib format to XIR (ftlib → XIR)
    struct xml_level level = {0};
    
    // Read blocks and create linked list
    int num_blocks;
    if (scanf("%d", &num_blocks) != 1) {
        return 1;
    }
    
    struct xml_block* prev_level_block = NULL;
    struct xml_block* prev_player_block = NULL;
    
    for (int i = 0; i < num_blocks; i++) {
        int type_id, id;
        double x, y, w, h, angle;
        int joint1, joint2;
        
        if (scanf("%d %d %lf %lf %lf %lf %lf %d %d", 
                  &type_id, &id, &x, &y, &w, &h, &angle, &joint1, &joint2) != 9) {
            return 1;
        }
        
        struct xml_block* block = (struct xml_block*)calloc(1, sizeof(struct xml_block));
        if (!block) {
            return 1;
        }
        
        block->type = map_piece_type(type_id);
        block->id = id;
        block->position.x = x;
        block->position.y = y;
        
        if (type_id == 1 || type_id == 3) {
            block->width = w / 2.0;
            block->height = h / 2.0;
        } else {
            block->width = w;
            block->height = h;
        }
        
        block->rotation = angle;
        block->goal_block = (type_id == 4 || type_id == 5);
        block->joints = NULL;
        block->next = NULL;
        
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
        
        if (type_id <= 3) {
            if (!level.level_blocks) {
                level.level_blocks = block;
            } else {
                prev_level_block->next = block;
            }
            prev_level_block = block;
        } else {
            if (!level.player_blocks) {
                level.player_blocks = block;
            } else {
                prev_player_block->next = block;
            }
            prev_player_block = block;
        }
    }
    
    // Read build area and goal area
    if (scanf("%lf %lf %lf %lf", &level.start.position.x, &level.start.position.y, 
              &level.start.width, &level.start.height) != 4) {
        return 1;
    }
    
    if (scanf("%lf %lf %lf %lf", &level.end.position.x, &level.end.position.y,
              &level.end.width, &level.end.height) != 4) {
        return 1;
    }

    // CHIMERA: Set up arena by inlining arena_init and replacing XML parsing
    arena* arena_ptr = new arena();
    
    // Zero-initialize the entire struct to match original arena_init behavior
    memset(arena_ptr, 0, sizeof(struct arena));
    
    // Arena initialization (from arena_init) 
    arena_ptr->view.x = 0.0f;
    arena_ptr->view.y = 0.0f;
    arena_ptr->view.width = 800;
    arena_ptr->view.height = 800;
    arena_ptr->view.scale = 1.0f;
    arena_ptr->cursor_x = 0;
    arena_ptr->cursor_y = 0;
    arena_ptr->shift = false;
    arena_ptr->ctrl = false;
    arena_ptr->tool = TOOL_MOVE;
    arena_ptr->tool_hidden = TOOL_MOVE;
    arena_ptr->state = STATE_NORMAL;
    arena_ptr->hover_joint = NULL;
    arena_ptr->hover_block = NULL;
    arena_ptr->root_joints_moving = NULL;
    arena_ptr->root_blocks_moving = NULL;
    arena_ptr->blocks_moving = NULL;
    // Initialize missing fields
    arena_ptr->move_orig_x = 0.0f;
    arena_ptr->move_orig_y = 0.0f;
    arena_ptr->move_orig_joint = NULL;
    arena_ptr->move_orig_block = NULL;
    arena_ptr->new_block = NULL;
    arena_ptr->block_graphics_v2 = NULL;
    arena_ptr->block_graphics_v2b = NULL;
    arena_ptr->frame_counter = 0;
    arena_ptr->single_ticks_remaining = -1;  // Default for normal playback
    arena_ptr->autostop_on_solve = false;
    arena_ptr->preview_trail = NULL;
    arena_ptr->ui_buttons = NULL;
    arena_ptr->ui_toolbar_opened = false;
    arena_ptr->ui_speedbar_opened = false;
    
    // CHIMERA: Replace xml_parse + convert_xml with direct XIR → internal design conversion
    convert_xml(&level, &arena_ptr->design);
    xml_free(&level);
    
    // Continue arena initialization (from arena_init)
    arena_ptr->world = gen_world(&arena_ptr->design);
    arena_ptr->tick = 0;
    arena_ptr->has_won = false;
    arena_ptr->preview_gp_trajectory = false;
    arena_ptr->preview_design = NULL;
    arena_ptr->preview_world = NULL;
    arena_ptr->preview_has_won = false;
    arena_ptr->lock_if_preview_solves = false;

    change_speed_preset(arena_ptr, 2);

    // TO CLAUDE - DO NOT MODIFY ANYTHING BELOW THIS LINE

    // Run to solve or end
    arena_ptr->state = STATE_RUNNING;
    while((int64_t)arena_ptr->tick != max_ticks && !arena_ptr->has_won) {
        arena_ptr->single_ticks_remaining = 1;
        tick_func(arena_ptr);
    }

    // Report
    std::cout << (arena_ptr->has_won ? (int64_t)arena_ptr->tick_solve : -1) << std::endl << arena_ptr->tick << std::endl;

    return 0;
}

// stubs - functions that are called somewhere and therefore require linking
// but don't have any effect on this CLI use case

extern "C" {

int set_interval(void (*func)(void *arg), int delay, void *arg) {return 0;}

void clear_interval(int id) {}

double time_precise_ms() {return 0;}

}
