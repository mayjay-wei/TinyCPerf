//
// Created by TCWei on 12/16/25.
//

#define PROFILING
#include <stdio.h>

#include "c_profiler.h"

void heavy_calculation(int n) {
  long sum = 0;
  for (int i = 0; i < n; i++) {
    sum += i;
  }
}

int light_operation(int a, int b) {
  int c = a + b;
  return c;
}

int main(void) {
  for (int i = 0; i < 5; i++) {
  // --- Profiling heavy_calculation ---
  CPROF_SCOPE(heavy_calculation, heavy_calculation(100000););

  int a = 0, b = 1;
  // --- Profiling light_operation ---
  CPROF_SCOPE(light_operation, int c = light_operation(a, b); a = c + 1;
              b += c;);

  // Call heavy_calculation again, time will be stored in
  // 'heavy_calculation_log'
  CPROF_SCOPE(heavy_calculation, heavy_calculation(200000););
  CPROF_SCOPE_TAG(heavy_calculation, 1, heavy_calculation(100000););
  const char *tag_str = "Second";
  CPROF_SCOPE_TAG(heavy_calculation, tag_str, heavy_calculation(50000););
  }

  // Output to file (only outputs functions tracked in main.c)
  CPROF_dump_to_file("../data/main_profiling_report.csv");

  // Free memory
  CPROF_cleanup();

  // === CPROF Cost Testing ===
  printf("Starting CPROF cost analysis...\n");
  
  // Test profiling overhead with different scenarios
  for (int i = 0; i < 1000; i++) {
    // Test 1: Empty operation profiling cost
    CPROF_SCOPE(profiling_empty, ;);
    
    // Test 2: Simple operation profiling cost
    CPROF_SCOPE(profiling_simple, int dummy = i * 2;);
    
    // Test 3: Small calculation profiling cost
    CPROF_SCOPE(profiling_small_calc, {
      int result = 0;
      for (int j = 0; j < 10; j++) {
        result += j;
      }
    });
  }
  
  // Test 4: Cost of statistics calculation
  for (int i = 0; i < 100; i++) {
    CPROF_SCOPE(profiling_calc_stats, {
      // Simulate some work to have meaningful data
      volatile int sum = 0;
      for (int j = 0; j < 1000; j++) {
        sum += j;
      }
      // Calculate stats (this will be measured)
      CPROF_Stats dummy_stats = CPROF_calculate_stats(__cprof_log_entries[0]);
    });
  }
  
  // Test 5: Cost of file dump operation
  for (int i = 0; i < 10; i++) {
    CPROF_SCOPE(profiling_file_dump, {
      CPROF_dump_to_file("../data/temp_profiling_cost_test.csv");
    });
  }
  
  // Output CPROF cost analysis results
  CPROF_dump_to_file("../data/cprof_cost_analysis.csv");
  
  printf("CPROF cost analysis complete. Results saved to ../data/cprof_cost_analysis.csv\n");
  
  // Final cleanup
  CPROF_cleanup();
  return 0;
}