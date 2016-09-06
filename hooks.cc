#include "hooks.h"
#include <stdio.h>
#include <stdlib.h>
#include "json.hpp"
using json = nlohmann::json;
using namespace std::chrono;

Hooks& Hooks::getInstance()
{
    static Hooks instance;
    return instance;
}

#if defined(ENABLE_PERF_HOOKS)

std::vector<std::string>
Hooks::getPerfEventNames()
{
    std::vector<std::string> event_names = {"", "--perf-event"};
    if (const char* env_names = getenv("PERF_EVENT_NAMES"))
    {
        char * names = new char[strlen(env_names) + 1];
        strcpy(names, env_names);
        char * p = strtok(names, " ");
        while (p)
        {
            event_names.push_back(std::string(p));
            p = strtok(NULL, " ");
        }
        delete[] names;

    } else {
        printf("WARNING: No perf events found in environment; set PERF_EVENT_NAMES.\n");
    }
    return event_names;
}

int
Hooks::getPerfGroupSize()
{
    if (const char* env_group_size = getenv("PERF_GROUP_SIZE"))
    {
        return atoi(env_group_size);
    } else {
        printf("WARNING: Perf group size unspecified, defaulting to 4\n");
        return 4;
    }
}

void
Hooks::traverse_edge(int64_t n)
{
    num_traversed_edges[omp_get_thread_num()] += n;
}

Hooks::Hooks()
 : num_traversed_edges(omp_get_max_threads())
 , perf_event_names(getPerfEventNames())
 , perf_group_size(getPerfGroupSize())
 , perf_events(perf_event_names, false)
 , perf(omp_get_max_threads(), perf_events)
{

}

#elif defined(ENABLE_PAPI_HOOKS)

Hooks::Hooks()
// TODO initialize these from environment or parameter
 : papi_counters({
    {PAPI_TOT_INS, "total_instructions"},
    {PAPI_LD_INS, "total_loads"},
    {PAPI_TOT_CYC, "total_cycles"},
    {PAPI_L3_TCA, "L3_cache_accesses"},
    {PAPI_L3_TCM, "L3_cache_misses"},
    {PAPI_BR_CN, "total_branches"},
    {PAPI_BR_MSP, "branch_mispredictions"},
    })
{

}

void
Hooks::papi_start()
{
    // Check to make sure we have enough counters
    if (papi_counters.size() > PAPI_num_counters())
    {
        printf("Error in PAPI: not enough hardware counters (need %li, have %i)\n",
            papi_counters.size(), PAPI_num_counters());
        exit(1);
    }
    // Copy event ids to an array
    std::vector<int> papi_event_ids(papi_counters.size());
    for (int i = 0; i < papi_counters.size(); ++i)
    {
        papi_event_ids[i] = papi_counters[i].id;
    }
    // Start the counters
    if (PAPI_start_counters(&papi_event_ids[0], papi_event_ids.size()) != PAPI_OK)
    {
        printf("Error in PAPI: failed to start counters.\n");
        //exit(1);
    }
}

std::string
Hooks::papi_stop()
{
    // Stop the counters
    std::vector<long long> papi_values(papi_counters.size());
    if (PAPI_stop_counters(&papi_values[0], papi_values.size()) != PAPI_OK)
    {
        printf("Error in PAPI: failed to stop counters.\n");
        exit(1);
    }

    json results;
    for (int i = 0; i < papi_values.size(); ++i)
    {
        results[papi_counters[i].name] = papi_values[i];
    }
    return results.dump();
}
#else
Hooks::Hooks(){};
#endif

int __attribute__ ((noinline))
Hooks::region_begin(std::string name) {

    #if defined(ENABLE_SNIPER_HOOKS)
        parmacs_roi_begin();
    #elif defined(ENABLE_GEM5_HOOKS)
        m5_reset_stats(0,0);
    #elif defined(ENABLE_PIN_HOOKS)
        __asm__("");
    #elif defined(ENABLE_PERF_HOOKS)
        #pragma omp parallel
        {
            unsigned tid = omp_get_thread_num();
            num_traversed_edges[tid] = 0;
            perf.open(tid, trial, perf_group_size);
            perf.start(tid, trial, perf_group_size);
        }
    #elif defined(ENABLE_PAPI_HOOKS)
        papi_start();
    #endif
    t1 = steady_clock::now();
    return 0;
}

int __attribute__ ((noinline))
Hooks::region_end(std::string name) {
    t2 = steady_clock::now();
    json results;
    #if defined(ENABLE_SNIPER_HOOKS)
        parmacs_roi_end();
    #elif defined(ENABLE_GEM5_HOOKS)
        m5_dumpreset_stats(0,0);
    #elif defined(ENABLE_PIN_HOOKS)
        __asm__("");
    #elif defined(ENABLE_PERF_HOOKS)
        #pragma omp parallel
        {
            unsigned tid = omp_get_thread_num();
            perf.stop(tid, trial, perf_group_size);
        }
        results = json::parse(perf.toString(trial, perf_group_size));
        // Only print traversed edges if the function was used
        bool total_edges_traversed = 0;
        for (int64_t n : num_traversed_edges) { total_edges_traversed += n; }
        if (total_edges_traversed > 0)
        {
            results["num_traversed_edges"] = num_traversed_edges;
        }
    #elif defined(ENABLE_PAPI_HOOKS)
        results = json::parse(papi_stop());
    #endif

    results["region_name"] = name;
    results["trial"] = trial;
    results["time_ms"] = duration<double, std::milli>(t2-t1).count();
    std::cout
#ifdef HOOKS_PRETTY_PRINT
    << std::setw(2)
#endif
    << results << std::endl;

    return 0;
}

extern "C" void traverse_edge(int64_t n)
{
    Hooks::getInstance().traverse_edge(n);
}
