#include "hooks.h"
#include <stdio.h>
#include <stdlib.h>
#include "json.hpp"
using json = nlohmann::json;
using namespace std::chrono;

#if defined(USE_MPI)
  #include <mpi.h>
#endif

Hooks& Hooks::getInstance()
{
    static Hooks instance;
    return instance;
}

void
Hooks::traverse_edge(int64_t n)
{
    num_traversed_edges[omp_get_thread_num()] += n;
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

Hooks::Hooks()
 : num_traversed_edges(omp_get_max_threads())
 , perf_event_names(getPerfEventNames())
 , perf_group_size(getPerfGroupSize())
 , perf_events(perf_event_names, false)
 , perf(omp_get_max_threads(), perf_events)
{

}

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
        int64_t total_edges_traversed = 0;
        for (int64_t n : num_traversed_edges) { total_edges_traversed += n; }
        if (total_edges_traversed > 0)
        {
            results["num_traversed_edges"] = num_traversed_edges;
        }
    #endif

    results["region_name"] = name;
    results["trial"] = trial;
    results["batch"] = batch;
    #if defined(USE_MPI)
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    results["pid"] = rank;
    #endif
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
