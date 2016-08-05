#include <string>
#include <inttypes.h>
#include <chrono>
#include <vector>

#if defined(ENABLE_SNIPER_HOOKS)
    #include <hooks_base.h>
#elif defined(ENABLE_GEM5_HOOKS)
    #include <util/m5/m5op.h>
#elif defined(ENABLE_PIN_HOOKS)
    // Nothing to include here
#elif defined(ENABLE_PERF_HOOKS)
    #include <perf.h>
    #include <omp.h>
#elif defined(ENABLE_PAPI_HOOKS)
    #include <papi.h>
#endif

class Hooks
{
public:
    static Hooks& getInstance();
    // Marks the start of a new phase of computation
	int region_begin(std::string name);
	// Marks the end of the current phase of computation
	int region_end(std::string name);
	// Marks the beginning of a new set of regions
	void next_trial();
    Hooks(Hooks const&)         = delete;
    void operator=(Hooks const&)  = delete;
private:
    std::chrono::time_point<std::chrono::steady_clock> t1, t2;
	int64_t trial;
#if defined(ENABLE_PERF_HOOKS)
    Hooks();
    std::vector<std::string> perf_event_names;
    gBenchPerf_event perf_events;
    gBenchPerf_multi perf;
    int perf_group_size;
    std::vector<std::string> getPerfEventNames();
    int getPerfGroupSize();
#elif defined(ENABLE_PAPI_HOOKS)
    Hooks();
    struct papi_counter
	{
	    int id;
	    std::string name;
	};
	std::vector<papi_counter> papi_counters;
    void papi_start();
    std::string papi_stop();
#else
    Hooks();
#endif
};

