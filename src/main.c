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

  // Output to file (only outputs functions tracked in main.c)
  CPROF_dump_to_file("../main_profiling_report.csv");

  // Free memory
  CPROF_cleanup();
  return 0;
}