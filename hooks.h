#include <inttypes.h>

// Marks the start of a new phase of computation
int hooks_region_begin(int64_t trial);
// Marks the end of the current phase of computation
int hooks_region_end(int64_t trial);