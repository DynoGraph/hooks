#include "hooks.h"
#include <stdio.h>
#include <stdlib.h>

#if defined(ENABLE_SNIPER_HOOKS)
    #include <hooks_base.h>
    // Marks which iteration we are on
    int sniper_hooks_iter_id = 0;
#elif defined(ENABLE_GEM5_HOOKS)
    #include <util/m5/m5op.h>
#elif defined(ENABLE_PIN_HOOKS)
    // Nothing to include here
#elif defined(ENABLE_PERF_HOOKS)
    #include <perf.h>
    #include <omp.h>
    std::vector<std::string> perf_event_names = {
        "", "--perf-event",
        "perf::PERF_COUNT_HW_INSTRUCTIONS",
        "perf::PERF_COUNT_HW_CACHE_L1D:READ:ACCESS",
        "perf::PERF_COUNT_HW_CACHE_L1D:WRITE:ACCESS",
        "perf::PERF_COUNT_HW_CPU_CYCLES",
        "perf::PERF_COUNT_HW_CACHE_L1D:READ:MISS",
        "perf::PERF_COUNT_HW_CACHE_L1D:WRITE:MISS",
        "snb_ep::OFFCORE_REQUESTS:ALL_DATA_RD:k=1:u=1:e=0:i=0:c=0:t=0",
        "perf::PERF_COUNT_HW_CACHE_LL:READ:ACCESS",
        "perf::PERF_COUNT_HW_CACHE_LL:READ:MISS",
        "snb_ep::BR_MISP_RETIRED:ALL_BRANCHES:k=1:u=1:e=0:i=0:c=0:t=0",
        "perf::PERF_COUNT_HW_CACHE_LL:WRITE:ACCESS",
        "perf::PERF_COUNT_HW_CACHE_LL:WRITE:MISS",
        "snb_ep::BR_INST_RETIRED:ALL_BRANCHES:k=1:u=1:e=0:i=0:c=0:t=0",
        "perf::PERF_COUNT_HW_CACHE_LL:PREFETCH:ACCESS",
        "perf::PERF_COUNT_HW_CACHE_LL:PREFETCH:MISS",
    };
    static gBenchPerf_event perf_events(perf_event_names);
    static gBenchPerf_multi perf(omp_get_max_threads(), perf_events);

#elif defined(ENABLE_PAPI_HOOKS)
    #include <papi.h>

    // Get the number of elements in a static array
    #define ARRAY_LENGTH(X) (sizeof(X)/sizeof(X[0]))

    static FILE* papi_output_file = NULL;

    typedef struct papi_counter
    {
        int id;
        const char * name;
    } papi_counter;

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
            printf("[DynoGraph] Error in PAPI: not enough hardware counters (need %i, have %i)\n",
                papi_num_events, PAPI_num_counters());
            exit(1);
        }
        // Copy event ids to an array
        for (int i = 0; i < papi_num_events; ++i) { papi_event_ids[i] = papi_counters[i].id; }
        // Start the counters
        if (PAPI_start_counters(papi_event_ids, papi_num_events) != PAPI_OK)
        {
            printf("[DynoGraph] Error in PAPI: failed to start counters.\n");
            //exit(1);
        }
    }

    void papi_stop()
    {
        // Stop the counters
        if (PAPI_stop_counters(papi_values, papi_num_events) != PAPI_OK)
        {
            printf("[DynoGraph] Error in PAPI: failed to stop counters.\n");
            exit(1);
        }
        // Open the output file
        //const char* papi_output_file = "/dev/stdout";
        //FILE* out = fopen(papi_output_file, "a+");
        //if (out == NULL) {
        //    printf("[DynoGraph] Error in PAPI: failed to open output file %s", papi_output_file);
        //    exit(1);
        //}
        // Write the output as JSON
        printf("{");
        for (int i = 0; i < papi_num_events; ++i)
        {
            printf("\n\t\"%s\":%lli", papi_counters[i].name, papi_values[i]);
            if (i != papi_num_events - 1) { printf(","); }
        }
        printf("\n}\n");
        // fclose(out);
    }
#endif

int __attribute__ ((noinline)) hooks_region_begin(int64_t trial) {
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

    return 0;
}

int __attribute__ ((noinline)) hooks_region_end(int64_t trial) {
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
        perf.print(trial);
    #elif defined(ENABLE_PAPI_HOOKS)
        papi_stop();
    #endif

    return 0;
}
