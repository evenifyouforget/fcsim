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

struct simple_sim_state {
    struct design design;
    b2World* world;
    uint64_t tick;
    bool has_won;
    uint64_t tick_solve;
};

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

static bool init_sim_from_input(struct simple_sim_state* sim, int max_ticks) {
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
        
        if (scanf("%d %d %lf %lf %lf %lf %lf %d %d", 
                  &type_id, &id, &x, &y, &w, &h, &angle, &joint1, &joint2) != 9) {
            return false;
        }
        
        struct xml_block* block = (struct xml_block*)calloc(1, sizeof(struct xml_block));
        if (!block) {
            return false;
        }
        
        block->type = map_piece_type(type_id);
        block->id = i;  // Use array index as XML ID instead of ftlib id
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
        
        // Debug output
        if (type_id == 4 || type_id == 5) {
            fprintf(stderr, "DEBUG: Created goal piece - ftlib_type=%d, xml_type=%d, goal_block=%d\n", 
                    type_id, block->type, block->goal_block);
        }
        
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
    convert_xml(&level, &sim->design);
    
    // Debug: Check what blocks we have after conversion
    struct block* block;
    int level_count = 0, player_count = 0;
    for (block = sim->design.level_blocks.head; block; block = block->next) {
        level_count++;
        double x = 0, y = 0, w = 0, h = 0;
        switch (block->shape.type) {
            case SHAPE_RECT:
                x = block->shape.rect.x; y = block->shape.rect.y;
                w = block->shape.rect.w; h = block->shape.rect.h;
                break;
            case SHAPE_CIRC:
                x = block->shape.circ.x; y = block->shape.circ.y;
                w = h = block->shape.circ.radius * 2;
                break;
            default: break;
        }
        fprintf(stderr, "DEBUG: Level block %d - fcsim_type_id=%d, pos=(%.1f,%.1f), size=(%.1fx%.1f)\n", 
                level_count, block->type_id, x, y, w, h);
    }
    for (block = sim->design.player_blocks.head; block; block = block->next) {
        player_count++;
        double x = 0, y = 0, w = 0, h = 0;
        switch (block->shape.type) {
            case SHAPE_BOX:
                x = block->shape.box.x; y = block->shape.box.y;
                w = block->shape.box.w; h = block->shape.box.h;
                break;
            case SHAPE_WHEEL:
                x = block->shape.wheel.center->x; y = block->shape.wheel.center->y;
                w = h = block->shape.wheel.radius * 2;
                break;
            case SHAPE_ROD:
                x = (block->shape.rod.from->x + block->shape.rod.to->x) / 2;
                y = (block->shape.rod.from->y + block->shape.rod.to->y) / 2;
                w = sqrt(pow(block->shape.rod.to->x - block->shape.rod.from->x, 2) + 
                        pow(block->shape.rod.to->y - block->shape.rod.from->y, 2));
                h = block->shape.rod.width;
                break;
            default: break;
        }
        if (block->goal) {
            fprintf(stderr, "DEBUG: Player block %d - GOAL BLOCK with fcsim_type_id=%d, pos=(%.1f,%.1f), size=(%.1fx%.1f)\n", 
                    player_count, block->type_id, x, y, w, h);
        } else {
            fprintf(stderr, "DEBUG: Player block %d - fcsim_type_id=%d, pos=(%.1f,%.1f), size=(%.1fx%.1f)\n", 
                    player_count, block->type_id, x, y, w, h);
        }
    }
    
    xml_free(&level);
    
    // Generate physics world
    sim->world = gen_world(&sim->design);
    if (!sim->world) {
        return false;
    }
    
    sim->tick = 0;
    sim->has_won = false;
    sim->tick_solve = 0;
    
    // Debug initial setup before any physics steps
    struct block* goal_block = NULL;
    for (struct block* block = sim->design.player_blocks.head; block; block = block->next) {
        if (block->goal) {
            goal_block = block;
            break;
        }
    }
    
    if (goal_block && goal_block->body) {
        double gx = goal_block->body->m_position.x;
        double gy = goal_block->body->m_position.y;
        fprintf(stderr, "DEBUG: Tick 0 BEFORE physics - goal_pos=(%.1f,%.1f)\n", gx, gy);
    } else {
        fprintf(stderr, "DEBUG: Tick 0 - no goal block body found yet\n");
    }
    
    return true;
}

static void step_sim(struct simple_sim_state* sim) {
    step(sim->world);
    sim->tick++;
    
    // Check for win condition
    if (!sim->has_won) {
        bool goal_achieved = goal_blocks_inside_goal_area(&sim->design);
        if (sim->tick == 0 || sim->tick == 1 || sim->tick % 100 == 0) { // Debug tick 0, 1, then every 100 ticks
            // Find goal block and check its position
            struct block* goal_block = NULL;
            for (struct block* block = sim->design.player_blocks.head; block; block = block->next) {
                if (block->goal) {
                    goal_block = block;
                    break;
                }
            }
            
            if (goal_block && goal_block->body) {
                double gx = goal_block->body->m_position.x;
                double gy = goal_block->body->m_position.y;
                double goal_area_x = sim->design.goal_area.x;
                double goal_area_y = sim->design.goal_area.y;
                double goal_area_w = sim->design.goal_area.w;
                double goal_area_h = sim->design.goal_area.h;
                
                fprintf(stderr, "DEBUG: Tick %lld, goal_achieved=%d, goal_pos=(%.1f,%.1f), goal_area=(%.1f,%.1f,%.1fx%.1f)\n", 
                        (long long)sim->tick, goal_achieved, gx, gy, goal_area_x, goal_area_y, goal_area_w, goal_area_h);
            } else {
                fprintf(stderr, "DEBUG: Tick %lld, goal_achieved=%d, no goal block body found\n", 
                        (long long)sim->tick, goal_achieved);
            }
        }
        if (goal_achieved) {
            sim->has_won = true;
            sim->tick_solve = sim->tick;
            fprintf(stderr, "DEBUG: GOAL ACHIEVED at tick %lld!\n", (long long)sim->tick);
        }
    }
}

static void cleanup_sim(struct simple_sim_state* sim) {
    if (sim->world) {
        // Box2D world cleanup is handled internally
        sim->world = NULL;
    }
}

int main() {
    // Read max_ticks from stdin (first parameter)
    int max_ticks;
    if (scanf("%d", &max_ticks) != 1) {
        fprintf(stderr, "Failed to read max_ticks\n");
        return 1;
    }
    
    // Initialize simulation from ftlib format input
    struct simple_sim_state sim;
    if (!init_sim_from_input(&sim, max_ticks)) {
        fprintf(stderr, "Failed to parse input or initialize simulation\n");
        return 1;
    }
    
    // Run simulation
    while (sim.tick < (uint64_t)max_ticks && !sim.has_won) {
        step_sim(&sim);
    }
    
    // Output results in the same format as ftlib's run_single_design
    // solve_tick (or -1 if not solved), end_tick
    printf("%lld\n%lld\n", sim.has_won ? (long long)sim.tick_solve : -1LL, (long long)sim.tick);
    
    // Cleanup
    cleanup_sim(&sim);
    
    return 0;
}