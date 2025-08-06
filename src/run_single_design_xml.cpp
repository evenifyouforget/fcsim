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

static bool init_sim_from_xml_stdin(struct simple_sim_state* sim, int max_ticks) {
    // Read XML from stdin
    char* xml_buffer = NULL;
    size_t buffer_size = 0;
    size_t total_size = 0;
    const size_t chunk_size = 4096;
    
    while (!feof(stdin)) {
        if (total_size + chunk_size > buffer_size) {
            buffer_size = (buffer_size == 0) ? chunk_size : buffer_size * 2;
            char* new_buffer = (char*)realloc(xml_buffer, buffer_size + 1);
            if (!new_buffer) {
                free(xml_buffer);
                return false;
            }
            xml_buffer = new_buffer;
        }
        
        size_t read_size = fread(xml_buffer + total_size, 1, chunk_size, stdin);
        total_size += read_size;
        
        if (read_size == 0) {
            break;
        }
    }
    
    if (xml_buffer) {
        xml_buffer[total_size] = '\0';
    }
    
    if (total_size == 0) {
        fprintf(stderr, "No XML input received\n");
        free(xml_buffer);
        return false;
    }
    
    fprintf(stderr, "DEBUG: Read %zu bytes of XML input\n", total_size);
    
    // Parse XML
    struct xml_level level = {0};
    if (xml_parse(xml_buffer, total_size, &level) != 0) {
        fprintf(stderr, "Failed to parse XML\n");
        free(xml_buffer);
        return false;
    }
    
    free(xml_buffer);
    
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

int main(int argc, char* argv[]) {
    // Read max_ticks from command line argument
    int max_ticks = 1000; // Default value
    if (argc > 1) {
        max_ticks = atoi(argv[1]);
    }
    
    // Initialize simulation from XML input
    struct simple_sim_state sim;
    if (!init_sim_from_xml_stdin(&sim, max_ticks)) {
        fprintf(stderr, "Failed to parse XML input or initialize simulation\n");
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