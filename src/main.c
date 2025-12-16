//
// Created by TCWei on 12/16/25.
//

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

CPROF_DECLARE_LOG(heavy_calculation);
CPROF_DECLARE_LOG(light_operation);

int main(void) {
  // --- Profiling heavy_calculation ---
  CPROF_SCOPE(heavy_calculation, { heavy_calculation(100000); });

  int a = 0, b = 1;
  // --- Profiling light_operation ---
  CPROF_SCOPE(light_operation, {
    int c = light_operation(a, b);
    a = c + 1;
    b += c;
  });

  // Call heavy_calculation again, time will be stored in 'heavy_calculation_log'
  CPROF_SCOPE(heavy_calculation, { heavy_calculation(200000); });

  // Output to file (only outputs functions tracked in main.c)
  CPROF_dump_to_file("../main_profiling_report.csv");

  // Free memory
  CPROF_cleanup();
  return 0;
}