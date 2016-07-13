#include <string>
#include <inttypes.h>

// Marks the start of a new phase of computation
int hooks_region_begin(std::string name, int64_t trial);
// Marks the end of the current phase of computation
int hooks_region_end(std::string name, int64_t trial);