extern "C" {
#include "xml.h"
#include "graph.h"
#include "arena.h"
}

#include <cstdlib>
#include <cstring>
#include <string>
#include <iostream>
#include <vector>

int main(int argc, char* argv[]) {
    // Read max_ticks from command line argument
    int64_t max_ticks = 1000; // Default value
    if (argc > 1) {
        max_ticks = atoi(argv[1]);
    }

    // Read entire stdin into string
    std::string content(std::istreambuf_iterator<char>(std::cin), {});
    char* xml = new char[content.length() + 1];
    strcpy(xml, content.c_str());

    // Set up arena
    arena* arena_ptr = new arena();
    arena_init(arena_ptr, 800, 800, xml, content.length());

    // Run to solve or end
    while((int64_t)arena_ptr->tick != max_ticks && !arena_ptr->has_won) {
        arena_ptr->single_ticks_remaining = 1;
        arena_ptr->state = STATE_RUNNING;
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
