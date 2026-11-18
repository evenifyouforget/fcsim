extern "C" {
#include "arena.h"
#include "design_file.h"
#include "graph.h"
#include "xml.h"
}

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char *argv[]) {
  // Read max_ticks from command line argument
  int64_t max_ticks = 1000; // Default value
  if (argc > 1) {
    max_ticks = atoi(argv[1]);
  }

  // Read entire stdin (XML or a binary design file) into a buffer
  std::string content(std::istreambuf_iterator<char>(std::cin), {});
  char *xml = new char[content.length() + 1];
  memcpy(xml, content.c_str(), content.length() + 1);

  // Set up arena
  arena *arena_ptr = new arena();
  arena_init(arena_ptr, 800, 800, xml, content.length());

  // Optionally save the loaded design as a binary design file
  if (argc > 2) {
    int len;
    uint8_t *buf = design_file_encode(&arena_ptr->design, &len);
    FILE *f = fopen(argv[2], "wb");
    if (!f || fwrite(buf, 1, len, f) != (size_t)len || fclose(f) != 0) {
      std::cerr << "could not write " << argv[2] << std::endl;
      return 1;
    }
    free(buf);
  }

  // Run to solve or end
  arena_ptr->state = STATE_RUNNING;
  while ((int64_t)arena_ptr->tick != max_ticks && !arena_ptr->has_won) {
    arena_ptr->single_ticks_remaining = 1;
    tick_func(arena_ptr);
  }

  // Report
  std::cout << (arena_ptr->has_won ? (int64_t)arena_ptr->tick_solve : -1)
            << std::endl
            << arena_ptr->tick << std::endl;

  return 0;
}

// stubs - functions that are called somewhere and therefore require linking
// but don't have any effect on this CLI use case

extern "C" {

int set_interval(void (*func)(void *arg), int delay, void *arg) { return 0; }

void clear_interval(int id) {}

double time_precise_ms() { return 0; }
}
