#include <chrono>
#include "interval.h"

extern "C" double time_precise_ms() {
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(end.time_since_epoch()).count();
}