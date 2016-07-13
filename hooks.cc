#include "hooks.h"
#include <stdio.h>
#include <stdlib.h>
#include "json.hpp"
using json = nlohmann::json;

#include <chrono>
using namespace std::chrono;

static time_point<steady_clock> t1, t2;

#if defined(ENABLE_SNIPER_HOOKS)
    #include <hooks_base.h>
#elif defined(ENABLE_GEM5_HOOKS)
    #include <util/m5/m5op.h>
#elif defined(ENABLE_PIN_HOOKS)
    // Nothing to include here
#elif defined(ENABLE_PERF_HOOKS)
    #include <perf.h>
    #include <omp.h>
    #include <chrono>

    static std::vector<std::string> getPerfEventNames()
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

    static std::vector<std::string> perf_event_names = getPerfEventNames();
    static gBenchPerf_event perf_events(perf_event_names, false);
    static gBenchPerf_multi perf(omp_get_max_threads(), perf_events);

#elif defined(ENABLE_PAPI_HOOKS)
    #include <papi.h>

    // Get the number of elements in a static array
    #define ARRAY_LENGTH(X) (sizeof(X)/sizeof(X[0]))

    struct papi_counter
    {
        int id;
        const char * name;
    };

    // List of counters to record
    static papi_counter papi_counters[] =
    {
        {PAPI_TOT_INS, "total_instructions"},
        {PAPI_LD_INS, "total_loads"},
        {PAPI_TOT_CYC, "total_cycles"},
        {PAPI_L3_TCA, "L3_cache_accesses"},
        {PAPI_L3_TCM, "L3_cache_misses"},
        {PAPI_BR_CN, "total_branches"},
        {PAPI_BR_MSP, "branch_mispredictions"},
    };

    static const int papi_num_events = ARRAY_LENGTH(papi_counters);

    static int papi_event_ids[ARRAY_LENGTH(papi_counters)];

    // Counter values are copied to this array after counters are stopped
    static long long papi_values[ARRAY_LENGTH(papi_counters)];

    void papi_start()
    {
        // Check to make sure we have enough counters
        if (papi_num_events > PAPI_num_counters())
        {
            printf("Error in PAPI: not enough hardware counters (need %i, have %i)\n",
                papi_num_events, PAPI_num_counters());
            exit(1);
        }
        // Copy event ids to an array
        for (int i = 0; i < papi_num_events; ++i) { papi_event_ids[i] = papi_counters[i].id; }
        // Start the counters
        if (PAPI_start_counters(papi_event_ids, papi_num_events) != PAPI_OK)
        {
            printf("Error in PAPI: failed to start counters.\n");
            //exit(1);
        }
    }

    std::string papi_stop()
    {
        // Stop the counters
        if (PAPI_stop_counters(papi_values, papi_num_events) != PAPI_OK)
        {
            printf("Error in PAPI: failed to stop counters.\n");
            exit(1);
        }

        json results;
        for (int i = 0; i < papi_num_events; ++i)
        {
            results[papi_counters[i].name] = papi_values[i];
        }
        return results.dump();
    }
#endif

int __attribute__ ((noinline)) hooks_region_begin(std::string name, int64_t trial) {

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
            perf.open(tid, trial);
            perf.start(tid, trial);
        }
    #elif defined(ENABLE_PAPI_HOOKS)
        papi_start();
    #endif
    t1 = steady_clock::now();
    return 0;
}

int __attribute__ ((noinline)) hooks_region_end(std::string name, int64_t trial) {
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
            perf.stop(tid, trial);
        }
        results = json::parse(perf.toString(trial));
    #elif defined(ENABLE_PAPI_HOOKS)
        results = json::parse(papi_stop());
    #endif

    results["region_name"] = name;
    results["trial"] = trial;
    results["time_ms"] = duration<double, std::milli>(t2-t1).count();
    std::cout << std::setw(2) << results << std::endl;

    return 0;
}
